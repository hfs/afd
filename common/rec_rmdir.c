/*
 *  rec_rmdir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rec_rmdir - deletes a directory recursive
 **
 ** SYNOPSIS
 **   int rec_rmdir(char *dirname)
 **
 ** DESCRIPTION
 **   Deletes 'dirname' recursive. This means all files and
 **   directories in 'dirname' are removed.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to delete the directory.
 **   When successful it will return 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.1996 H.Kiehl Created
 **   24.09.2000 H.Kiehl Use rmdir() instead of unlink() for removing
 **                      directories. Causes problems with Solaris.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* remove()                              */
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* unlink(), rmdir()                     */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>

extern int  sys_log_fd;


/*############################ rec_rmdir() ##############################*/
int
rec_rmdir(char *dirname)
{
   char          *ptr;
   struct stat   stat_buf;
   struct dirent *dirp;
   DIR           *dp;
   int           ret = 0;

   if (stat(dirname, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to stat() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* Make sure it is NOT a directory */
   if (S_ISDIR(stat_buf.st_mode) == 0)
   {
#ifdef _WORKING_UNLINK
      if (unlink(dirname) < 0)
#else
      if (remove(dirname) < 0)
#endif /* _WORKING_UNLINK */
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to delete %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      return(0);
   }

   /* It's a directory */
   ptr = dirname + strlen(dirname);
   *ptr++ = '/';
   *ptr = '\0';

   if ((dp = opendir(dirname)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
      if ((ret = rec_rmdir(dirname)) != 0)
      {
         break;
      }
   }
   ptr[-1] = 0;
   if (ret == 0)
   {
      if (rmdir(dirname) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to rmdir() %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
         (void)closedir(dp);
         return(INCORRECT);
      }
   }
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(ret);
}
