/*
 *  rlock_region.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rlock_region - sets a read lock for a specific region in a file
 **
 ** SYNOPSIS
 **   void rlock_region(int fd, off_t offset)
 **
 ** DESCRIPTION
 **   This function sets a read lock to the region specified by offset
 **   plus one byte in the file with file descriptor fd. If the region
 **   is already locked by another process it does NOT care since
 **   it is a read lock.
 **
 ** RETURN VALUES
 **   None. The function exits with LOCK_REGION_ERROR if fcntl() call
 **   fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.09.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                     /* strerror()                    */
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

extern int sys_log_fd;


/*########################### rlock_region() ############################*/
void
rlock_region(int fd, off_t offset)
{
   struct flock rlock;

   rlock.l_type   = F_RDLCK;
   rlock.l_whence = SEEK_SET;
   rlock.l_start  = offset;
   rlock.l_len    = 1;

   if (fcntl(fd, F_SETLKW, &rlock) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "fcntl() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(LOCK_REGION_ERROR);
   }

   return;
}
