/*
 *  remove_time_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   remove_time_dir - removes all files in a time directory
 **
 ** SYNOPSIS
 **   void remove_time_dir(char *host_name, int job_id, int reason)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* remove()                                */
#include <string.h>           /* strerror(), strcpy(), strlen()          */
#include <sys/types.h>
#include <sys/stat.h>         /* S_ISDIR()                               */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* rmdir()                                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int               sys_log_fd;
extern char              time_dir[];
#ifdef _DELETE_LOG
extern struct delete_log dl;
#endif

/* #define _CHECK_TIME_DIR_DEBUG 1 */


/*++++++++++++++++++++++++++ remove_time_dir() ++++++++++++++++++++++++++*/
void
remove_time_dir(char *host_name, int job_id, int reason)
{
#ifdef _CHECK_TIME_DIR_DEBUG
   (void)rec(sys_log_fd, INFO_SIGN,
             "Removing time directory %s (%s %d)\n",
             time_dir, __FILE__, __LINE__);
#else
   DIR *dp;

   if ((dp = opendir(time_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s to remove old time jobs : %s (%s %d)\n",
                time_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      int           number_deleted = 0;
      off_t         file_size_deleted = 0;
      char          *ptr;
      struct dirent *p_dir;
      struct stat   stat_buf;

      ptr = time_dir + strlen(time_dir);
      *(ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);

         if (stat(time_dir, &stat_buf) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      time_dir, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (remove(time_dir) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to remove() file %s : %s (%s %d)\n",
                         time_dir, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
#ifdef _DELETE_LOG
               int    prog_name_length;
               size_t dl_real_size;
#endif

               number_deleted++;
               file_size_deleted += stat_buf.st_size;
#ifdef _DELETE_LOG
               (void)strcpy(dl.file_name, p_dir->d_name);
               (void)sprintf(dl.host_name, "%-*s %x",
                             MAX_HOSTNAME_LENGTH, host_name, reason);
               *dl.file_size = stat_buf.st_size;
               *dl.job_number = job_id;
               *dl.file_name_length = strlen(p_dir->d_name);
               if (reason == OTHER_DEL)
               {
                  prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                             "AMG Failed to locate time job after DIR_CONFIG update.");
               }
               else
               {
                  prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                             "Host was disabled.");
               }
               dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "write() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
#endif
            }
         }
         errno = 0;
      }

      *(ptr - 1) = '\0';
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (rmdir(time_dir) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not rmdir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
#endif

   return;
}
