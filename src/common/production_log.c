/*
 *  production_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   production_log - writes production data to the production log fifo
 **
 ** SYNOPSIS
 **   void production_log(time_t         creation_time,
 **                       unsigned short unique_number,
 **                       unsigned int   split_job_counter,
 **                       char           *fmt,
 **                       ...)
 **
 ** DESCRIPTION
 **   When process wants to log the files it changed, it writes them
 **   via a fifo. The data it will look as follows:
 **       <ML><UDN>|<OFN>|<NFL>[|<CMD>]\n
 **         |   |     |     |      |
 **         |   |     |     |      +-------> Command executed.
 **         |   |     |     +--------------> New filename.
 **         |   |     +--------------------> Original File Name.
 **         |   +--------------------------> Unique ID.
 **         +------------------------------> The length of this message of
 **                                          type unsigned short.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.03.2001 H.Kiehl Created
 **   02.10.2004 H.Kiehl Change format and show command executed.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                   /* STDERR_FILENO                   */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>
#include "fddefs.h"

/* External global variables */
#ifdef _PRODUCTION_LOG
extern int  production_log_fd;
extern char *p_work_dir;
#endif /* _PRODUCTION_LOG */


/*########################## production_log() ###########################*/
void
production_log(time_t         creation_time,
               unsigned short unique_number,
               unsigned int   split_job_counter,
               char           *fmt,
               ...)
{
#ifdef _PRODUCTION_LOG
   size_t  length;
   char    production_buffer[MAX_INT_LENGTH + MAX_INT_LENGTH + MAX_PRODUCTION_BUFFER_LENGTH + 1];
   va_list ap;

   if ((production_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
      char production_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(production_log_fifo, p_work_dir);
      (void)strcat(production_log_fifo, FIFO_DIR);
      (void)strcat(production_log_fifo, PRODUCTION_LOG_FIFO);
      if ((production_log_fd = coe_open(production_log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (make_fifo(production_log_fifo) != SUCCESS)
            {
               return;
            }
            else
            {
               if ((production_log_fd = coe_open(production_log_fifo, O_RDWR)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not open fifo %s : %s",
                             production_log_fifo, strerror(errno));
                  return;
               }
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo %s : %s",
                       production_log_fifo, strerror(errno));
            return;
         }
      }
   }

   length = sizeof(short);
   length += sprintf(&production_buffer[length], "%x_%x_%x%c",
                     creation_time, unique_number,
                     split_job_counter, SEPARATOR_CHAR);
   va_start(ap, fmt);
   length += vsprintf(&production_buffer[length], fmt, ap) + 1;
   production_buffer[length] = '\n';
   *(unsigned short *)production_buffer = (unsigned short)length;
   va_end(ap);
   if (write(production_log_fd, production_buffer, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "write() error : %s",
                 strerror(errno));
   }

#endif /* _PRODUCTION_LOG */
   return;
}
