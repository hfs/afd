/*
 *  trans_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000, 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_log - writes formated log output to the show_olog dialog
 **
 ** SYNOPSIS
 **   void trans_log(char *sign, char *file, int line, char *fmt, ...)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.06.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "show_olog.h"

/* External global variables. */
extern int              timeout_flag;
extern char             msg_str[];
extern struct send_data *db;
extern Widget           log_output;
extern XmTextPosition   wpr_position;


/*############################ trans_log() ###############################*/
void
trans_log(char *sign, char *file, int line, char *fmt, ...)
{
   size_t    header_length,
             length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH];
   va_list   ap;
   struct tm *p_ts;

   tvalue = time(NULL);
   p_ts    = localtime(&tvalue);
   buf[0]  = (p_ts->tm_hour / 10) + '0';
   buf[1]  = (p_ts->tm_hour % 10) + '0';
   buf[2]  = ':';
   buf[3]  = (p_ts->tm_min / 10) + '0';
   buf[4]  = (p_ts->tm_min % 10) + '0';
   buf[5]  = ':';
   buf[6]  = (p_ts->tm_sec / 10) + '0';
   buf[7] = (p_ts->tm_sec % 10) + '0';
   buf[8] = ' ';
   buf[9] = sign[0];
   buf[10] = sign[1];
   buf[11] = sign[2];
   buf[12] = ' ';
   header_length = length = 13;

   va_start(ap, fmt);
   length += vsprintf(&buf[length], fmt, ap);
   va_end(ap);

   if (timeout_flag == ON)
   {
      char *tmp_ptr;

      if (buf[length - 1] == '.')
      {
         tmp_ptr = &buf[length - 1];
      }
      else
      {
         tmp_ptr = &buf[length];
      }
      if ((file == NULL) || (line == 0))
      {
         length += sprintf(tmp_ptr, " due to timeout (%lds)\n", db->timeout);
      }
      else
      {
         length += sprintf(tmp_ptr, " due to timeout (%lds). (%s %d)\n",
                           db->timeout, file, line);
      }
   }
   else
   {
      if ((file == NULL) || (line == 0))
      {
         buf[length] = '\n';
         length += 1;
      }
      else
      {
         length += sprintf(&buf[length], " (%s %d)\n", file, line);
      }
   }
   if (msg_str[0] != '\0')
   {
      char tmp_char;

      tmp_char = buf[header_length];
      buf[header_length] = '\0';
      length += sprintf(&buf[length], "%s%s\n", buf, msg_str);
      buf[header_length] = tmp_char;
   }

   XmTextInsert(log_output, wpr_position, buf);
   wpr_position += length;
   XmTextShowPosition(log_output, wpr_position);
   XtVaSetValues(log_output, XmNcursorPosition, wpr_position, NULL);

   return;
}
