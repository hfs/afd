/*
 *  rec.c - Part of AFD, an automatic file distribution program.
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
 **   rec - writes a formated message to a file descriptor
 **
 ** SYNOPSIS
 **   int rec(int fd, char *sign, char *fmt, ...)
 **
 ** DESCRIPTION
 **   The function rec() logs all data specified by 'fmt' to the
 **   file descriptor 'fd'. Each log entry will start with a date
 **   string and a 'sign' telling the severity of the log entry
 **   (eg. <W> for warning, <F> for fatal, etc).
 **
 ** RETURN VALUES
 **   Returns the number of bytes written to the file descriptor 'fd'
 **   when successful. If it fails it will return -errno.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.02.1996 H.Kiehl Created
 **   09.05.1997 H.Kiehl Optimized by removing sprintf() and strftime() call.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* vsprintf(), sprintf()           */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), strftime(), localtime() */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <errno.h>


/*################################ rec() ################################*/
int
rec(int fd, char *sign, char *fmt, ...)
{
   int       length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH];
   va_list   ap;
#ifndef _OLD_REC
   struct tm *p_ts;
#endif

   tvalue = time(NULL);
#ifdef _OLD_REC
   length = strftime(buf, MAX_LINE_LENGTH, "%d %X ", localtime(&tvalue));
   length += sprintf(&buf[length], "%s ", sign);
#else
   p_ts    = localtime(&tvalue);
   buf[0]  = (p_ts->tm_mday / 10) + '0';
   buf[1]  = (p_ts->tm_mday % 10) + '0';
   buf[2]  = ' ';
   buf[3]  = (p_ts->tm_hour / 10) + '0';
   buf[4]  = (p_ts->tm_hour % 10) + '0';
   buf[5]  = ':';
   buf[6]  = (p_ts->tm_min / 10) + '0';
   buf[7]  = (p_ts->tm_min % 10) + '0';
   buf[8]  = ':';
   buf[9]  = (p_ts->tm_sec / 10) + '0';
   buf[10] = (p_ts->tm_sec % 10) + '0';
   buf[11] = ' ';
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';
   length = 16;
#endif

   va_start(ap, fmt);
   length += vsprintf(&buf[length], fmt, ap);

   if (write(fd, buf, length) != length)
   {
      return(-errno);
   }
   va_end(ap);

   return(length);
}
