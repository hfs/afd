/*
 *  remove_pool_directory.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 1998 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_pool_directory - removes all files in a job directory
 **
 ** SYNOPSIS
 **   void remove_pool_directory(char *job_dir, int dir_id)
 **
 ** DESCRIPTION
 **   The function remove_job_directory() removes all files in job_dir
 **   and the job_directory. All files deleted are logged in the
 **   delete log.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* sprintf()                      */
#include <string.h>                    /* strerror()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                    /* opendir(), closedir(), DIR,    */
                                       /* readdir(), struct dirent       */
#include <unistd.h>                    /* rmdir(), unlink()              */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int               sys_log_fd;
#ifdef _DELETE_LOG
extern struct delete_log dl;
#endif


/*####################### remove_pool_directory() ########################*/
void
remove_pool_directory(char *job_dir, int dir_id)
{
#ifdef _DELETE_LOG
   char          *ptr;
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   if ((dp = opendir(job_dir)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Could not opendir() %s : %s (%s %d)\n",
                job_dir, strerror(errno), __FILE__, __LINE__);
      return;
   }
   ptr = job_dir + strlen(job_dir);
   *(ptr++) = '/';

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      (void)strcpy(ptr, p_dir->d_name);
      if (stat(job_dir, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Can't access file %s : %s (%s %d)\n",
                   job_dir, strerror(errno), __FILE__, __LINE__);
         continue;
      }

      /* Sure it is a normal file? */
      if (S_ISDIR(stat_buf.st_mode) == 0)
      {
         if (unlink(job_dir) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to unlink() file %s : %s (%s %d)\n",
                      p_dir->d_name, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            int    prog_name_length;
            size_t dl_real_size;

            (void)strcpy(dl.file_name, p_dir->d_name);
            (void)sprintf(dl.host_name, "%-*s %x",
                          MAX_HOSTNAME_LENGTH,
                          "-",
                          OTHER_DEL);
            *dl.file_size = stat_buf.st_size;
            *dl.job_number = dir_id;
            *dl.file_name_length = strlen(p_dir->d_name);
            prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                       "AMG Removed from pool directory.");
            dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
            if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "write() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
      }
      errno = 0;
   } /* while ((p_dir = readdir(dp)) != NULL) */

   *ptr = '\0';
   if (errno)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not readdir() %s : %s (%s %d)\n",
                job_dir, strerror(errno), __FILE__, __LINE__);
   }
   if (closedir(dp) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Could not closedir() %s : %s (%s %d)\n",
                job_dir, strerror(errno), __FILE__, __LINE__);
   }

   if (rmdir(job_dir) == -1)
   {
      if ((errno == ENOTEMPTY) || (errno == EEXIST))
      {
         (void)rec_rmdir(job_dir);
      }
      else
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to rmdir() %s : %s (%s %d)\n",
                   job_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
#endif /* _DELETE_LOG */

   return;
}
