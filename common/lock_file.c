/*
 *  lock_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   lock_file - locks a file
 **
 ** SYNOPSIS
 **   int lock_file(char *file, int block_flag)
 **
 ** DESCRIPTION
 **   If 'block_flag' is set to ON, lock_file() will wait for the
 **   lock on 'file' to be released if it is already locked by
 **   another process. The caller is responsible for closing the
 **   file descriptor returned by this function.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to lock file 'file' or
 **   IS_LOCKED if it is already locked by another process.
 **   Otherwise it returns the file-descriptor 'fd' of the
 **   file 'file'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.03.1996 H.Kiehl Created
 **   04.01.1997 H.Kiehl When already locked, return IS_LOCKED.
 **
 */
DESCR__E_M3

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

extern int sys_log_fd;


/*############################ lock_file() ##############################*/
int
lock_file(char *file, int block_flag)
{
   int          fd;
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1};

   if ((fd = coe_open(file, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open() %s : %s (%s %d)\n",
                file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (block_flag == ON)
   {
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not set read lock : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   else
   {
      if (fcntl(fd, F_SETLK, &wlock) == -1)
      {
         if ((errno == EACCES) || (errno == EAGAIN))
         {
            /* The file is already locked, so don't bother */
            return(IS_LOCKED);
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not set read lock : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
      }
   }

   return(fd);
}
