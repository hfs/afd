/*
 *  trans_db_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_db_log - writes formated log output to transfer debug log
 **
 ** SYNOPSIS
 **   void trans_db_log(char *sign, char *file, int line, char *fmt, ...)
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
 **   02.07.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

extern int    trans_db_log_fd;
extern char   msg_str[],
              *p_work_dir,
              tr_hostname[];
extern struct job db;


/*########################### trans_db_log() #############################*/
void
trans_db_log(char *sign, char *file, int line, char *fmt, ...)
{
   register int i = 0;
   char         *ptr;
   size_t       header_length,
                length;
   time_t       tvalue;
   char         buf[MAX_LINE_LENGTH];
   va_list      ap;
   struct tm    *p_ts;

   if ((trans_db_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
      char trans_db_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(trans_db_log_fifo, p_work_dir);
      (void)strcat(trans_db_log_fifo, FIFO_DIR);
      (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);
      if ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(trans_db_log_fifo) == SUCCESS) &&
                ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) == -1))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not open fifo <%s> : %s",
                          TRANS_DEBUG_LOG_FIFO, strerror(errno));
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       TRANS_DEBUG_LOG_FIFO, strerror(errno));
         }
      }
   }

   tvalue = time(NULL);
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

   ptr = tr_hostname;
   while (*ptr != '\0')
   {
      buf[length + i] = *ptr;
      ptr++; i++;
   }
   while (i < MAX_HOSTNAME_LENGTH)
   {
      buf[length + i] = ' ';
      i++;
   }
   length += i;
   buf[length] = '[';
   buf[length + 1] = db.job_no + 48;
   buf[length + 2] = ']';
   buf[length + 3] = ':';
   buf[length + 4] = ' ';
   length += 5;
   header_length = length;

   va_start(ap, fmt);
   length += vsprintf(&buf[length], fmt, ap);
   va_end(ap);
   if (buf[length - 1] == '\n')
   {
      length--;
      buf[length] = '\0';
   }

   if ((file == NULL) || (line == 0))
   {
      buf[length] = '\n';
      length += 1;
   }
   else
   {
      length += sprintf(&buf[length], " #%d (%s %d)\n",
                        db.job_id, file, line);
   }
   if (msg_str[0] != '\0')
   {
      char tmp_char;

      tmp_char = buf[header_length];
      buf[header_length] = '\0';
      length += sprintf(&buf[length], "%s%s\n", buf, msg_str);
      buf[header_length] = tmp_char;
   }

   if (write(trans_db_log_fd, buf, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }

   return;
}
