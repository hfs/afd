/*
 *  afdcfg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   afdcfg - configure AFD
 **
 ** SYNOPSIS
 **   afdcfg [-w <working directory>] option
 **                                    -a      enable archive
 **                                    -A      disable archive
 **                                    -r      enable retrieving of files
 **                                    -R      disable retrieving of files
 **                                    -s      status of the above flags
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.10.2000 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                       /* open()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "permission.h"
#include "version.h"

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;

/* Local function prototypes */
static void                usage(char *);

#define ENABLE_ARCHIVE_SEL   1
#define DISABLE_ARCHIVE_SEL  2
#define ENABLE_RETRIEVE_SEL  3
#define DISABLE_RETRIEVE_SEL 4
#define STATUS_SEL           5


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdcfg() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         action;
   char        *perm_buffer,
               *ptr,
               user[128],
               sys_log_fifo[MAX_PATH_LENGTH],
               work_dir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc != 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (get_arg(&argc, argv, "-a", NULL, 0) != SUCCESS)
   {
      if (get_arg(&argc, argv, "-A", NULL, 0) != SUCCESS)
      {
         if (get_arg(&argc, argv, "-r", NULL, 0) != SUCCESS)
         {
            if (get_arg(&argc, argv, "-R", NULL, 0) != SUCCESS)
            {
               if (get_arg(&argc, argv, "-s", NULL, 0) != SUCCESS)
               {
                  usage(argv[0]);
                  exit(INCORRECT);
               }
               else
               {
                  action = STATUS_SEL;
               }
            }
            else
            {
               action = DISABLE_RETRIEVE_SEL;
            }
         }
         else
         {
            action = ENABLE_RETRIEVE_SEL;
         }
      }
      else
      {
         action = DISABLE_ARCHIVE_SEL;
      }
   }
   else /* Enable archive */
   {
      action = ENABLE_ARCHIVE_SEL;
   }
   get_user(user);

   /*
    * Ensure that the user may use this program.
    */
   switch(get_permissions(&perm_buffer))
   {
      case NONE     : (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      {
                         int permission = NO;

                         if ((perm_buffer[0] == 'a') &&
                             (perm_buffer[1] == 'l') &&
                             (perm_buffer[2] == 'l'))
                         {
                            permission = YES;
                         }
                         else
                         {
                            if (posi(perm_buffer, AFD_CFG_PERM) != NULL)
                            {
                               permission = YES;
                            }
                         }
                         free(perm_buffer);
                         if (permission != YES)
                         {
                            (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                            exit(INCORRECT);
                         }
                      }
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      break;

      default       : (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   (void)sprintf(sys_log_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open system log fifo */
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   ptr = (char *)fsa - 3;

   switch(action)
   {
      case ENABLE_ARCHIVE_SEL :
         if (*ptr & DISABLE_ARCHIVE)
         {
            *ptr ^= DISABLE_ARCHIVE;
         }
         else
         {
            (void)fprintf(stdout, "Archiving is already enabled.\n");
         }
         break;
      case DISABLE_ARCHIVE_SEL :
         if (*ptr & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, "Archiving is already disabled.\n");
         }
         else
         {
            *ptr |= DISABLE_ARCHIVE;
         }
         break;
      case ENABLE_RETRIEVE_SEL :
         if (*ptr & DISABLE_RETRIEVE)
         {
            *ptr ^= DISABLE_RETRIEVE;
         }
         else
         {
            (void)fprintf(stdout, "Retrieving is already enabled.\n");
         }
         break;
      case DISABLE_RETRIEVE_SEL :
         if (*ptr & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, "Retrieving is already disabled.\n");
         }
         else
         {
            *ptr |= DISABLE_RETRIEVE;
         }
         break;
      case STATUS_SEL :
         if (*ptr & DISABLE_ARCHIVE)
         {
            (void)fprintf(stdout, "Archiving : Disabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Archiving : Enabled\n");
         }
         if (*ptr & DISABLE_RETRIEVE)
         {
            (void)fprintf(stdout, "Retrieving: Disabled\n");
         }
         else
         {
            (void)fprintf(stdout, "Retrieving: Enabled\n");
         }
         break;
      default :
         (void)fprintf(stderr, "Impossible! (%s %d)\n", __FILE__, __LINE__);
         exit(INCORRECT);
   }

   (void)fsa_detach();

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] options\n",
                 progname);
   (void)fprintf(stderr,
                 "                                        -a      enable archive\n");
   (void)fprintf(stderr,
                 "                                        -A      disable archive\n");
   (void)fprintf(stderr,
                 "                                        -r      enable retrieving of files\n");
   (void)fprintf(stderr,
                 "                                        -R      disable retrieving of files\n");
   (void)fprintf(stderr,
                 "                                        -s      status of the above flags\n");
   return;
}
