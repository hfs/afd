/*
 *  lock_region.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lock_region - locks a specific region of a file
 **
 ** SYNOPSIS
 **   int lock_region(int fd, off_t offset)
 **
 ** DESCRIPTION
 **   This function locks the region specified by offset plus
 **   one byte in the file with file descriptor fd. If the region
 **   is already locked by another process it does NOT wait for
 **   it being unlocked again.
 **
 ** RETURN VALUES
 **   Either IS_LOCKED when the region is locked or IS_NOT_LOCKED
 **   when it has succesfully locked the region. The function exits
 **   with LOCK_REGION_ERROR if fcntl() call fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                     /* strerror()                    */
#include <stdlib.h>                     /* exit()                        */
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"


/*########################### lock_region() #############################*/
int
lock_region(int fd, off_t offset)
{
   struct flock wlock;

   wlock.l_type   = F_WRLCK;
   wlock.l_whence = SEEK_SET;
   wlock.l_start  = offset;
   wlock.l_len    = 1;

   if (fcntl(fd, F_SETLK, &wlock) == -1)
   {
      if ((errno == EACCES) || (errno == EAGAIN))
      {
         /* The file is already locked! */
         return(IS_LOCKED);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "fcntl() error : %s", strerror(errno));
         exit(LOCK_REGION_ERROR);
      }
   }

   return(IS_NOT_LOCKED);
}
