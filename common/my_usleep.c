/*
 *  my_usleep.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Deutscher Wetterdienst (DWD),
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
 **   my_usleep - sleeps for interval of microseconds
 **
 ** SYNOPSIS
 **   void my_usleep(unsigned long msec)
 **
 ** DESCRIPTION
 **   The my_usleep() function suspends execution of the calling
 **   process for msec microseconds.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.02.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>                   /* exit()                          */
#include <sys/time.h>                 /* struct timeval                  */
#include <sys/types.h>                /* fd_set                          */
#include <unistd.h>                   /* select()                        */
#include <errno.h>

extern int sys_log_fd;


/*############################# my_usleep() #############################*/
void
my_usleep(unsigned long msec)
{
   struct timeval timeout;

   timeout.tv_sec = 0;
   timeout.tv_usec = msec;

   if (select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Select error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
