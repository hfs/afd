/*
 *  check_fra_dir_pos.c - Part of AFD, an automatic file distribution program.
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
 **   check_fra_dir_pos - checks if the dir_position in the FRA is
 **                       still correct
 **
 ** SYNOPSIS
 **   check_fra_dir_pos [--version] [-w <working directory>] [-f]
 **
 ** DESCRIPTION
 **   This program checks if the dir_position in structure FRA is still
 **   correct. With the -f option one can force it to correct this. This
 **   however is very dangerous and should only be done as a last
 **   resort.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.2002 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <sys/mman.h>                    /* mmap()                       */
#include <errno.h>
#include "version.h"

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs = 0;
#ifndef _NO_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct fileretrieve_status *fra;

/* Local function prototypes. */
static void usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                 fd,
                       force,
                       gotcha,
                       i, j,
                       *no_of_dir_names;
   char                file[MAX_PATH_LENGTH],
                       *ptr,
                       url[MAX_PATH_LENGTH],
                       user[MAX_PATH_LENGTH],
                       work_dir[MAX_PATH_LENGTH];
   struct stat         stat_buf;
   struct dir_name_buf *dnb;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if ((argc == 2) &&
       (argv[1][0] == '-') && (argv[1][1] == 'f') && (argv[1][2] == '\0'))
   {
      force = YES;
   }
   else if (argc == 1)
        {
           force = NO;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if (fra_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FRA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_dir_names = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dnb = (struct dir_name_buf *)ptr;

   (void)fprintf(stdout,
                 "            Number of directories: %d   FRA ID: %d\n\n",
                 no_of_dirs, fra_id);
   for (i = 0; i < no_of_dirs; i++)
   {
      gotcha = NO;
      if (fra[i].protocol != LOC)
      {
         int up = 0,
             wp;

         ptr = fra[i].url;
         wp = sprintf(url, "%s%s%s/", p_work_dir, AFD_FILE_DIR, INCOMING_DIR);

         /* Away with the scheme. */
         while ((*ptr != '/') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '/')
         {
            while (*ptr == '/')
            {
               ptr++;
            }

            /* Store user name. */
            while ((*ptr != ':') && (*ptr != '\0'))
            {
               url[wp] = *ptr;
               user[up] = *ptr;
               ptr++; wp++; up++;
            }
            user[up] = '\0';
            if (*ptr == ':')
            {
               /* Away with the password. */
               while ((*ptr != '@') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '@')
               {
                  int length = 0;

                  url[wp] = *ptr;
                  wp++; ptr++;
                  while ((*ptr != '/') && (*ptr != '\0') &&
                         (length < (MAX_HOSTNAME_LENGTH + 1)))
                  {
                     url[wp] = *ptr;
                     ptr++; wp++; length++;
                  }
                  if (length == (MAX_HOSTNAME_LENGTH + 1))
                  {
                     /* If hostname is to long away with it. */
                     while ((*ptr != '/') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                  }
                  if (*ptr == '/')
                  {
                     if (*(ptr + 1) == '/')
                     {
                        ptr++;
                        url[wp] = '/';
                        wp++;
                        (void)strcpy(&url[wp], user);
                        wp += up;
                     }
                     (void)strcpy(&url[wp], ptr);
                  }
                  else
                  {
                     url[wp] = '\0';
                  }
               }
            }
         }
      }
      else
      {
         (void)strcpy(url, fra[i].url);
      }
      for (j = 0; j < *no_of_dir_names; j++)
      {
         if (strcmp(url, dnb[j].dir_name) == 0)
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == YES)
      {
         if (fra[i].dir_pos == j)
         {
            (void)fprintf(stdout, "%s [%d %d] -> Ok\n",
                          fra[i].url, fra[i].dir_pos, dnb[j].dir_id);
         }
         else
         {
            if (force == YES)
            {
               fra[i].dir_pos = j;
               (void)fprintf(stdout,
                             "%s [%d %d] -> INCORRECT %d should be %d. Corrected.\n",
                             fra[i].url, fra[i].dir_pos, dnb[j].dir_id,
                             fra[i].dir_pos, j);
            }
            else
            {
               (void)fprintf(stdout,
                             "%s [%d %d] -> INCORRECT %d should be %d\n",
                             fra[i].url, fra[i].dir_pos, dnb[j].dir_id,
                             fra[i].dir_pos, j);
            }
         }
      }
      else
      {
         (void)fprintf(stdout, "%s -> NOT in directory buffer! [%s]\n",
                       fra[i].url, url);
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : check_fra_dir_pos [--version] [-w working directory] [-f]\n");
   return;
}
