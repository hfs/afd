/*
 *  next_counter.c - Part of AFD, an automatic file distribution program.
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
 **   next_counter - gets and updates the internal AFD counter
 **
 ** SYNOPSIS
 **   int next_counter(int *counter)
 **
 ** DESCRIPTION
 **   The function next_counter reads and returns the current counter.
 **   It then increments the counter by one and writes this value back
 **   to the counter file. When the value is larger then MAX_MSG_PER_SEC
 **   (9999) it resets the counter to zero.
 **
 ** RETURN VALUES
 **   Returns the current value 'counter' in the AFD counter file and
 **   SUCCESS. Else INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.08.1996 H.Kiehl Created
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
#include "amgdefs.h"

/* External global variables */
extern int counter_fd;


/*############################# next_counter() ##########################*/
int
next_counter(int *counter)
{
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};

   /* Try to lock file which holds counter */
   if (fcntl(counter_fd, F_SETLKW, &wlock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Could not set write lock.");
      return(INCORRECT);
   }

   /* Go to beginning in file */
   if (lseek(counter_fd, 0, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() : %s", strerror(errno));
      if (fcntl(counter_fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unset write lock : %s", strerror(errno));
      }
      return(INCORRECT);
   }

   /* Read the value of counter */
   if (read(counter_fd, counter, sizeof(int)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not read value of counter : %s", strerror(errno));
      if (fcntl(counter_fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unset write lock : %s", strerror(errno));
      }
      return(INCORRECT);
   }

   /* Go to beginning in file */
   if (lseek(counter_fd, 0, SEEK_SET) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not seek() : %s", strerror(errno));
      if (fcntl(counter_fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unset write lock : %s", strerror(errno));
      }
      return(INCORRECT);
   }

   /* Ensure that counter does not become larger then MAX_MSG_PER_SEC */
   if (((*counter)++ >= MAX_MSG_PER_SEC) || (*counter < 0))
   {
      *counter = 0;
   }

   /* Write new value into counter file */
   if (write(counter_fd, counter, sizeof(int)) != sizeof(int))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not write value to counter file : %s", strerror(errno));
      if (fcntl(counter_fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unset write lock : %s", strerror(errno));
      }
      return(INCORRECT);
   }

   /* Unlock file which holds the counter */
   if (fcntl(counter_fd, F_SETLKW, &ulock) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not unset write lock : %s", strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
