/*
 *  remove_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_dir - removes one directory with all its files
 **
 ** SYNOPSIS
 **   int remove_dir(char *dirname)
 **
 ** DESCRIPTION
 **   Deletes the directory 'dirname' with all its files. If there
 **   are directories within this directory the function will fail.
 **   For this purpose rather use the function rec_rmdir().
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to delete the directory.
 **   When successful it will return 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.12.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* remove()                              */
#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* rmdir()                               */
#include <sys/types.h>
#include <dirent.h>             /* opendir(), readdir(), closedir()      */
#include <errno.h>

/* External global variables. */
extern int sys_log_fd;


/*############################# remove_dir() ############################*/
int
remove_dir(char *dirname)
{
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

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
#ifdef _WORKING_UNLINK
      if (unlink(dirname) == -1)
#else
      if (remove(dirname) == -1)
#endif /* _WORKING_UNLINK */
      {
         if (errno == ENOENT)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to delete %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to delete %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
            (void)closedir(dp);
            return(INCORRECT);
         }
      }
   }
   ptr[-1] = 0;
   if (rmdir(dirname) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to rmdir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      (void)closedir(dp);
      return(INCORRECT);
   }
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}
