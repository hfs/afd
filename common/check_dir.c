/*
 *  check_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_dir - Checks if a directory exists, if not it tries
 **               to create this directory
 **
 ** SYNOPSIS
 **   int check_dir(char *directory)
 **
 ** DESCRIPTION
 **   This function checks if the 'directory' exists and
 **   if not it will try to create it. If it does exist, it
 **   is checked if it has the correct access permissions.
 **
 ** RETURN VALUES
 **   SUCCESS if this directoy exists or has been created.
 **   Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.12.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>            /* access()                               */
#include <errno.h>

/* External global variables */
extern int sys_log_fd;


/*############################ check_dir() ##############################*/
int
check_dir(char *directory, int access_mode)
{
   struct stat stat_buf;

   if (stat(directory, &stat_buf) == -1)
   {
      if (mkdir(directory, DIR_MODE) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to create directory %s : %s (%s %d)\n",
                   directory, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   else if (!S_ISDIR(stat_buf.st_mode))
        {
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "There already exists a file %s, thus unable to create the directory. (%s %d)\n",
                     directory, __FILE__, __LINE__);
           return(INCORRECT);
        }
        else /* Lets check if the correct permissions are set */
        {
           if (access(directory, access_mode) == -1)
           {
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Incorrect permission for directory %s (%s %d)\n",
                        directory, __FILE__, __LINE__);
              return(INCORRECT);
           }
        }

   return(SUCCESS);
}
