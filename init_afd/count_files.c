/*
 *  count_files.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   count_files - counts the number of files and bytes in directory
 **
 ** SYNOPSIS
 **   void count_files(char *dirname, unsigned int *files, off_t *bytes)
 **
 ** DESCRIPTION
 **   The function count_files counts the number of files and bytes
 **   int the directory 'dirname'. If there are directories in
 **   'dirname' they will not be counted.
 **
 ** RETURN VALUES
 **   Returns the number of files (in 'files') and the number of bytes
 **   (in 'bytes') in the directory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.05.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>

/* External global variables. */
extern int sys_log_fd;


/*########################### count_files() #############################*/
void
count_files(char *dirname, unsigned int *files, off_t *bytes)
{
   DIR *dp;

   *files = 0;
   *bytes = 0;
   if ((dp = opendir(dirname)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char          *ptr,
                    fullname[MAX_PATH_LENGTH];
      struct stat   stat_buf;
      struct dirent *p_dir;

      (void)strcpy(fullname, dirname);
      ptr = fullname + strlen(fullname);
      *ptr++ = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(ptr, p_dir->d_name);
            if (stat(fullname, &stat_buf) == -1)
            {
               if (errno != ENOENT)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Can't access file %s : %s (%s %d)\n",
                            fullname, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               /* Sure it is a normal file? */
               if (S_ISREG(stat_buf.st_mode))
               {
                  (*bytes) += stat_buf.st_size;
                  (*files)++;
               }
            }
         }
         errno = 0;
      }
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
      }

      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
