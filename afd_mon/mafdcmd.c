/*
 *  mafdcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mafdcmd - send commands to the AFD monitor
 **
 ** SYNOPSIS
 **   mafdcmd [-w <working directory>] option AFD|position
 **                                    -e      enable AFD
 **                                    -E      disable AFD
 **                                    -r      retry
 **                                    -X      toggle enable/disable AFD
 **                                    -v      Just print the version number.
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
 **   16.11.2002 H.Kiehl Created
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
#include "mondefs.h"
#include "permission.h"
#include "version.h"

/* Global variables */
int                    sys_log_fd = STDERR_FILENO,
                       msa_fd = -1,
                       msa_id,
                       no_of_afd_names,
                       no_of_afds = 0,
                       options = 0;
#ifndef _NO_MMAP
off_t                  msa_size;
#endif
char                   **afds = NULL,
                       *p_work_dir;
struct mon_status_area *msa;

/* Local function prototypes */
static int             get_afd_position(struct mon_status_area *, char *, int);
static void            eval_input(int, char **),
                       usage(char *);

#define ENABLE_AFD_OPTION    1
#define DISABLE_AFD_OPTION   2
#define TOGGLE_AFD_OPTION    4
#define RETRY_OPTION         8


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ mafdcmd() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         errors = 0,
               i,
               position;
   char        *perm_buffer,
               *ptr,
               user[128],
               sys_log_fifo[MAX_PATH_LENGTH],
               work_dir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if ((argc > 1) &&
       (argv[1][0] == '-') && (argv[1][1] == 'v') && (argv[1][2] == '\0'))
   {
      (void)fprintf(stdout,
#ifdef PRE_RELEASE
                    "%d.%d.%d-pre%d\n", MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
                    "%d.%d.%d\n", MAJOR, MINOR, BUG_FIX);
#endif
      exit(SUCCESS);
   }

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   set_afd_euid(work_dir);

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   eval_input(argc, argv);
   get_user(user);

   /*
    * Ensure that the user may use this program.
    */
   switch(get_permissions(&perm_buffer))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
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
               if (posi(perm_buffer, MAFD_CMD_PERM) != NULL)
               {
                  permission = YES;

                  /*
                   * Check if the user may do everything
                   * he has requested. If not remove it
                   * from the option list.
                   */
                  if ((options & ENABLE_AFD_OPTION) ||
                      (options & DISABLE_AFD_OPTION))
                  {
                     if (posi(perm_buffer, DISABLE_AFD_PERM) == NULL)
                     {
                        if (options & ENABLE_AFD_OPTION)
                        {
                           options ^= ENABLE_AFD_OPTION;
                        }
                        if (options & DISABLE_AFD_OPTION)
                        {
                           options ^= DISABLE_AFD_OPTION;
                        }
                        if (options & TOGGLE_AFD_OPTION)
                        {
                           options ^= TOGGLE_AFD_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable a AFD.\n",
                                      user);
                     }
                  }
                  if (options & RETRY_OPTION)
                  {
                     if (posi(perm_buffer, RETRY_PERM) == NULL)
                     {
                        options ^= RETRY_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to retry.\n",
                                      user);
                     }
                  }
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

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (msa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   (void)sprintf(sys_log_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, MON_SYS_LOG_FIFO);
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
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   for (i = 0; i < no_of_afd_names; i++)
   {
      position = -1;
      ptr = afds[i];
      while ((*ptr != '\0') && (isdigit(*ptr)))
      {
         ptr++;
      }
      if ((*ptr == '\0') && (ptr != afds[i]))
      {
         position = atoi(afds[i]);
         if ((position < 0) || (position > (no_of_afds - 1)))
         {
            (void)fprintf(stderr,
                          "WARNING : Position %d out of range. Ignoring. (%s %d)\n",
                          position, __FILE__, __LINE__);
            errors++;
            continue;
         }
      }
      if (position < 0)
      {
         if ((position = get_afd_position(msa, afds[i], no_of_afds)) < 0)
         {
            (void)fprintf(stderr,
                          "WARNING : Could not find AFD %s in MSA. (%s %d)\n",
                          afds[i], __FILE__, __LINE__);
            errors++;
            continue;
         }
      }

      /*
       * ENABLE AFD
       */
      if (options & ENABLE_AFD_OPTION)
      {
         if (msa[position].connect_status == DISABLED)
         {
            int  fd;
            char mon_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(mon_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, MON_CMD_FIFO);
            if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               errors++;
            }
            else
            {
               int  length;
               char cmd[2 + MAX_INT_LENGTH];

               length = sprintf(cmd, "%c %d", ENABLE_MON, position) + 1;
               if (write(fd, cmd, length) != length)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Failed to write() to %s : %s (%s %d)\n",
                            mon_cmd_fifo, strerror(errno),
                            __FILE__, __LINE__);
                  errors++;
               }
               else
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: ENABLED (%s) [mafdcmd].\n",
                            MAX_AFD_NAME_LENGTH, msa[position].afd_alias, user);
               }
               if (close(fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Failed to close() FIFO %s : %s (%s %d)\n",
                            mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "INFO    : AFD %s is already enabled.\n",
                          msa[position].afd_alias);
         }
      }

      /*
       * DISABLE AFD
       */
      if (options & DISABLE_AFD_OPTION)
      {
         if (msa[position].connect_status == DISABLED)
         {
            (void)fprintf(stderr,
                          "INFO    : AFD %s is already disabled.\n",
                          msa[position].afd_alias);
         }
         else
         {
            int  fd;
            char mon_cmd_fifo[MAX_PATH_LENGTH];

            (void)sprintf(mon_cmd_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, MON_CMD_FIFO);
            if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               errors++;
            }
            else
            {
               int  length;
               char cmd[2 + MAX_INT_LENGTH];

               length = sprintf(cmd, "%c %d", DISABLE_MON, position) + 1;
               if (write(fd, cmd, length) != length)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Failed to write() to %s : %s (%s %d)\n",
                            mon_cmd_fifo, strerror(errno),
                            __FILE__, __LINE__);
                  errors++;
               }
               else
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: DISABLED (%s) [mafdcmd].\n",
                            MAX_AFD_NAME_LENGTH, msa[position].afd_alias, user);
               }
               if (close(fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Failed to close() FIFO %s : %s (%s %d)\n",
                            mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
      }

      /*
       * ENABLE or DISABLE a AFD.
       */
      if (options & TOGGLE_AFD_OPTION)
      {
         int  fd;
         char mon_cmd_fifo[MAX_PATH_LENGTH];

         (void)sprintf(mon_cmd_fifo, "%s%s%s",
                       p_work_dir, FIFO_DIR, MON_CMD_FIFO);
         if ((fd = open(mon_cmd_fifo, O_RDWR)) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
            errors++;
         }
         else
         {
            int  length;
            char action,
                 cmd[2 + MAX_INT_LENGTH];

            if (msa[position].connect_status == DISABLED)
            {
               length = sprintf(cmd, "%c %d", ENABLE_MON, position) + 1;
               action = ENABLE_MON;
            }
            else /* DISABLE AFD */
            {
               length = sprintf(cmd, "%c %d", DISABLE_MON, position) + 1;
               action = DISABLE_MON;
            }
            if (write(fd, cmd, length) != length)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to write() to %s : %s (%s %d)\n",
                         mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
               errors++;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: %s (%s) [mafdcmd].\n",
                         MAX_AFD_NAME_LENGTH, msa[position].afd_alias,
                         (action == DISABLE_MON) ? "DISABLE" : "ENABLE", user);
            }
            if (close(fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Failed to close() FIFO %s : %s (%s %d)\n",
                         mon_cmd_fifo, strerror(errno),
                         __FILE__, __LINE__);
            }
         }
      }

      if (options & RETRY_OPTION)
      {
         int  fd;
         char retry_fifo[MAX_PATH_LENGTH];

         (void)sprintf(retry_fifo, "%s%s%s%d",
                       p_work_dir, FIFO_DIR, RETRY_MON_FIFO, position);
         if ((fd = open(retry_fifo, O_RDWR)) == -1)
         {
            (void)fprintf(stderr,
                          "WARNING : Failed to open() %s : %s (%s %d)\n",
                          retry_fifo, strerror(errno),
                          __FILE__, __LINE__);
            errors++;
         }
         else
         {
            if (write(fd, &position, sizeof(int)) != sizeof(int))
            {
               (void)fprintf(stderr,
                             "WARNING : Failed to write() to %s : %s (%s %d)\n",
                             retry_fifo, strerror(errno),
                             __FILE__, __LINE__);
               errors++;
            }
            if (close(fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Failed to close() FIFO %s : %s (%s %d)\n"
                         RETRY_FD_FIFO, strerror(errno),
                         __FILE__, __LINE__);
            }
         }
      }
   } /* for (i = 0; i < no_of_afd_names; i++) */

   (void)msa_detach();

   exit(errors);
}


/*++++++++++++++++++++++++++++ eval_input() +++++++++++++++++++++++++++++*/
static void
eval_input(int argc, char *argv[])
{
   int  correct = YES,       /* Was input/syntax correct?      */
        need_afdname = NO;
   char progname[128];

   (void)strcpy(progname, argv[0]);

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         -e : enable AFD                                */
   /*         -E : disable AFD                               */
   /*         -r : retry                                     */
   /*         -X : toggle enable/disable AFD                 */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      if (*(argv[0] + 2) == '\0')
      {
         switch(*(argv[0] + 1))
         {
            case 'e': /* enable AFD */
               options ^= ENABLE_AFD_OPTION;
               need_afdname = YES;
               break;

            case 'E': /* disable AFD */
               options ^= DISABLE_AFD_OPTION;
               need_afdname = YES;
               break;

            case 'r': /* Retry */
               options ^= RETRY_OPTION;
               need_afdname = YES;
               break;

            case 'X': /* Toggle enable/disable AFD */
               options ^= TOGGLE_AFD_OPTION;
               break;

            default : /* Unknown parameter */

               (void)fprintf(stderr, "ERROR  : Unknown parameter %c. (%s %d)\n",
                            *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch(*(argv[0] + 1)) */
      }
      else
      {
         (void)fprintf(stderr, "ERROR  : Unknown option %s. (%s %d)\n",
                      argv[0], __FILE__, __LINE__);
         correct = NO;
      }
   }

   if (correct != NO)
   {
      no_of_afd_names = argc;
      if (no_of_afd_names > 0)
      {
         int i = 0;

         RT_ARRAY(afds, no_of_afd_names, (MAX_AFD_NAME_LENGTH + 1), char);
         while (argc > 0)
         {
            (void)strcpy(afds[i], argv[0]);
            argc--; argv++;
            i++;
         }
      }
      else
      {
         if (need_afdname == YES)
         {
            (void)fprintf(stderr, "ERROR   : No AFD names specified!\n");
            correct = NO;
         }
      }
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage(progname);
      exit(1);
   }

   return;
}


/*######################### get_afd_position() ##########################*/
static int
get_afd_position(struct mon_status_area *msa,
                 char                   *afd_alias,
                 int                    no_of_afds)
{
   int position;

   for (position = 0; position < no_of_afds; position++)
   {
      if (CHECK_STRCMP(msa[position].afd_alias, afd_alias) == 0)
      {
         return(position);
      }
   }

   /* AFD not found in struct. */
   return(INCORRECT);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] options AFD|position\n",
                 progname);
   (void)fprintf(stderr,
                 "                                           -e      enable AFD\n");
   (void)fprintf(stderr,
                 "                                           -E      disable AFD\n");
   (void)fprintf(stderr,
                 "                                           -r      retry\n");
   (void)fprintf(stderr,
                 "                                           -X      toggle enable/disable AFD\n");
   (void)fprintf(stderr,
                 "                                           -v      just print Version\n");
   return;
}
