/*
 *  create_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_message - creates a message that is used by the sf_xxx
 **                    process
 **
 ** SYNOPSIS
 **   int create_message(int job_id, char *recipient, char *options)
 **
 ** DESCRIPTION
 **   This function creates a message in th AFD message directory.
 **   The name of the message is the job ID. From the contents of
 **   this message the sf_xxx process no where to send the files
 **   and with what options.
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to create the message, otherwise
 **   INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **   09.08.1998 H.Kiehl Return SUCCESS when we managed to create the
 **                      message.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strlen(), strerror()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* write(), close()                         */
#include <fcntl.h>           /* open()                                   */
#include <errno.h>

/* External global variables */
extern char msg_dir[],
            *p_msg_dir;


/*########################### create_message() ##########################*/
int
create_message(int job_id, char *recipient, char *options)
{
   int  fd,
        length;
   char buffer[MAX_RECIPIENT_LENGTH + 3];

   (void)sprintf(p_msg_dir, "%d", job_id);
   if ((fd = open(msg_dir, O_RDWR | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() <%s> : %s", msg_dir, strerror(errno));
      return(INCORRECT);
   }

   length = sprintf(buffer, "%s\n", DESTINATION_IDENTIFIER);
   if (write(fd, buffer, length) != length)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to write() to <%s> : %s", msg_dir, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }

   length = sprintf(buffer, "%s\n\n", recipient);
   if (write(fd, buffer, length) != length)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to write() to <%s> : %s", msg_dir, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }

   if (options != NULL)
   {
      length = sprintf(buffer, "%s\n", OPTION_IDENTIFIER);
      if (write(fd, buffer, length) != length)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write() to <%s> : %s", msg_dir, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      length = strlen(options);
      if (write(fd, options, length) != length)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write() to <%s> : %s", msg_dir, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /* Don't forget the newline, options does not have it. */
      if (write(fd, "\n", 1) != 1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write() to <%s> : %s", msg_dir, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
   }

   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   return(SUCCESS);
}
