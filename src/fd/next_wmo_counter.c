/*
 *  next_wmo_counter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2009 Deutscher Wetterdienst (DWD),
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
 **   next_wmo_counter - gets and updates the internal WMO counter
 **
 ** SYNOPSIS
 **   int next_wmo_counter(int counter_fd, int *counter)
 **
 ** DESCRIPTION
 **   The function next_counter reads and returns the current counter.
 **   It then increments the counter by one and writes this value back
 **   to the counter file. When the value is larger then MAX_WMO_COUNTER
 **   (999) it resets the counter to zero.
 **
 ** RETURN VALUES
 **   On success it will return the counter value else INCORRECT will be
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <unistd.h>                   /* read(), write(), lseek()        */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                    /* fcntl()                         */
#include <errno.h>
#include "wmodefs.h"


/*########################### next_wmo_counter() ########################*/
int
next_wmo_counter(int counter_fd, int *counter)
{
   struct flock wulock = {F_WRLCK, SEEK_SET, 0, 1};

   /* Try to lock file which holds counter. */
   if (fcntl(counter_fd, F_SETLKW, &wulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not set write lock : %s", strerror(errno));
      return(INCORRECT);
   }

   /* Ensure that counter does not become larger then MAX_WMO_COUNTER. */
   if (((*counter)++ >= MAX_WMO_COUNTER) || (*counter < 0))
   {
      *counter = 0;
   }

   /* Unlock file which holds the counter. */
   wulock.l_type = F_UNLCK;
   if (fcntl(counter_fd, F_SETLKW, &wulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not unset write lock : %s", strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
