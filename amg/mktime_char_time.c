/*
 *  mktime_char_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mktime_char_time -
 **
 ** SYNOPSIS
 **   void mktime_char_time(char *time_char, time_t convert_time);
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

/* External global variables */
extern int sys_log_fd;


/*######################### mktime_char_time()###########################*/
void
mktime_char_time(char *time_char, time_t convert_time)
{
   struct tm *bd_time;     /* Broken-down time */

   bd_time = gmtime(&convert_time);

   /* Write 'minute' field (0-59) */
   *time_char = bd_time->tm_min;

   /* Write 'hour' field (60-82) */
   bd_time->tm_hour;
   
   /* Write 'day of month' field (83-114) */
   bd_time->tm_mday;
   
   /* Write 'month' field (115-126) */
   bd_time->tm_mon + 1;

   /* Write 'day of week' field (127-133) */
   bd_time->tm_wday + 1;

   return;
}
