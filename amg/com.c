/*
 *  com.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1998 Deutscher Wetterdienst (DWD),
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
 **   com - sends a command to the dir_check command fifo
 **
 ** SYNOPSIS
 **   int com(char action)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when transmission of command <action> was successful,
 **   otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   26.03.1998 H.Kiehl Put this function into a separate file.
 **
 */
DESCR__E_M3

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* read(), write(), close()            */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables */
extern int  no_of_dir,
            shm_id,
            sys_log_fd;
extern char cmd_fifo[MAX_PATH_LENGTH],
            resp_fifo[MAX_FILENAME_LENGTH];


/*################################ com() ################################*/
int
com(char action)
{
   int            write_fd,
                  read_fd,
                  length,
                  status;
   char           buffer[MAX_FIFO_BUFFER];
   fd_set         rset;
   struct timeval timeout;

   /* Open fifo to send command to job */
   if ((write_fd = open(cmd_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open fifo to wait for answer from job */
   if ((read_fd = open(resp_fifo, (O_RDONLY | O_NONBLOCK))) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                resp_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write command to command fifo */
   buffer[0] = action;
   buffer[1] = '\0';
   length = 2;
   if (action == START)
   {
      length += sprintf(&buffer[2], "%d %d", shm_id, no_of_dir);
   }
#ifdef _FIFO_DEBUG
   show_fifo_data('W', "ip_cmd", buffer, length, __FILE__, __LINE__);
#endif
   if (write(write_fd, buffer, length) != length)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not write to fifo %s : %s (%s %d)\n",
                cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise descriptor set and timeout */
   FD_ZERO(&rset);
   FD_SET(read_fd, &rset);
   timeout.tv_usec = 0;
   timeout.tv_sec = JOB_TIMEOUT;

   /* Wait for message x seconds and then continue. */
   status = select((read_fd + 1), &rset, NULL, NULL, &timeout);

   if ((status > 0) && (FD_ISSET(read_fd, &rset)))
   {
      if ((length = read(read_fd, buffer, 10)) > 0)
      {
#ifdef _FIFO_DEBUG
         show_fifo_data('R', "ip_resp", buffer, length, __FILE__, __LINE__);
#endif
         if (buffer[length - 1] != ACKN)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Received garbage (%d) while reading from fifo. (%s %d)\n",
                      buffer[length - 1], __FILE__, __LINE__);
         }

         /* Should any action be taken??????????????????? */
      }
   }
   else if (status < 0)
        {
           (void)rec(sys_log_fd, FATAL_SIGN, "select() error : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
           exit(INCORRECT);
        }
        else if (status == 0)
             {
                /* The other side does not answer. */
                (void)rec(sys_log_fd, WARN_SIGN,
                          "Did not receive any reply from %s. (%s %d)\n",
                          IT_PROC_NAME, __FILE__, __LINE__);

                /* So what do we do now???????????  */
                (void)close(write_fd);
                (void)close(read_fd);
                return(INCORRECT);
             }
             else /* Impossible! Unknown error. */
             {
                (void)rec(sys_log_fd, FATAL_SIGN,
                          "Ouch! What now? @!$(%. (%s %d)\n",
                          __FILE__, __LINE__);
                exit(INCORRECT);
             }

   if (close(write_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (close(read_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}
