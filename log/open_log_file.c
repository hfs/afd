/*
 *  open_log_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   open_log_file - opens a file or reading and writing and creates
 **                   it if does not exist
 **
 ** SYNOPSIS
 **   FILE *open_log_file(char *log_file_name)
 **
 ** DESCRIPTION
 **   This function tries to open and create (if it does not exist)
 **   the file log_file_name. If it fails to do so because the disk
 **   is full, it will continue trying until it succeeds.
 **
 ** RETURN VALUES
 **   Upon successful completion open_log_file() returns a FILE
 **   pointer.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.11.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <unistd.h>               /* sleep()                             */
#include <errno.h>
#include "logdefs.h"

/* External global variables. */
extern int sys_log_fd;


/*########################### open_log_file() ###########################*/
FILE *
open_log_file(char *log_file_name)
{
   static FILE *log_file;

   if ((log_file = fopen(log_file_name, "a+")) == NULL)
   {
      if (errno == ENOSPC)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "DISK FULL!!! Will retry in %d second interval. (%s %d)\n",
                   DISK_FULL_RESCAN_TIME, __FILE__, __LINE__);

         while (errno == ENOSPC)
         {
            (void)sleep(DISK_FULL_RESCAN_TIME);
            errno = 0;
            if ((log_file = fopen(log_file_name, "a+")) == NULL)
            {
               if (errno != ENOSPC)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Could not fopen() %s : %s (%s %d)\n",
                            log_file_name, strerror(errno),
                            __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
         }

         (void)rec(sys_log_fd, INFO_SIGN,
                   "Continuing after disk was full. (%s %d)\n",
                   __FILE__, __LINE__);
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not fopen() %s : %s (%s %d)\n",
                   log_file_name, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   return(log_file);
}
