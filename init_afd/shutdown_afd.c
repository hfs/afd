/*
 *  shutdown_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2004 Deutscher Wetterdienst (DWD),
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
 **   shutdown_afd - does a shutdown of the AFD
 **
 ** SYNOPSIS
 **   void shutdown_afd(char *fake_user)
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
 **   12.04.1996 H.Kiehl Created
 **   13.08.1997 H.Kiehl Made this a function.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval                          */
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <unistd.h>           /* select(), unlink()                      */
#include <errno.h>
#include "version.h"

/* External global variables */
extern int  sys_log_fd;
extern char *p_work_dir;


/*########################### shutdown_afd() ############################*/
void
shutdown_afd(char *fake_user)
{
   int            afd_cmd_fd,
                  afd_resp_fd,
                  status,
                  val;
   fd_set         rset;
   char           buffer[DEFAULT_BUFFER_SIZE],
                  afd_cmd_fifo[MAX_PATH_LENGTH],
                  afd_resp_fifo[MAX_PATH_LENGTH],
                  user[MAX_FULL_USER_ID_LENGTH];
   struct timeval timeout;

   /* Initialise variables */
   (void)strcpy(afd_cmd_fifo, p_work_dir);
   (void)strcat(afd_cmd_fifo, FIFO_DIR);
   (void)strcpy(afd_resp_fifo, afd_cmd_fifo);
   (void)strcat(afd_resp_fifo, AFD_RESP_FIFO);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);

   if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((afd_resp_fd = open(afd_resp_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    afd_resp_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Tell user what we are doing */
   get_user(user, fake_user);
   (void)rec(sys_log_fd, CONFIG_SIGN, "Starting AFD shutdown (%s) ...\n",
             user);

   /* Send SHUTDOWN command */
   if (send_cmd(SHUTDOWN, afd_cmd_fd) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to send stop command to %s : %s (%s %d)\n",
                    AFD, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Now lets wait for a reply from 'init_afd', but not more then 10s.
    */

   /* Initialise descriptor set and timeout */
   FD_ZERO(&rset);
   FD_SET(afd_resp_fd, &rset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = 10L;

   /* Wait for message x seconds and then continue. */
   status = select(afd_resp_fd + 1, &rset, NULL, NULL, &timeout);

   /* Did we get a timeout? */
   if (status == 0)
   {
      (void)fprintf(stderr, "\nAFD is NOT responding!\n");

      /*
       * Since the AFD does not answer and we have already send
       * the shutdown command, it is necessary to remove this command
       * from the FIFO.
       */
      if ((val = fcntl(afd_cmd_fd, F_GETFL, 0)) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to get file status flag : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      val |= O_NONBLOCK;
      if (fcntl(afd_cmd_fd, F_SETFL, val) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to set file status flag : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)read(afd_cmd_fd, buffer, DEFAULT_BUFFER_SIZE);

      /*
       * Telling the user we failed to do a shutdown is not of
       * much use. It would now be better if we kill all jobs
       * and shared memory areas of the AFD.
       */
      if (check_afd(1L) == 0)
      {
         (void)fprintf(stderr, "Removed all AFD processes and resources.\n");

         /* Remove AFD_ACTIVE file */
         (void)strcpy(afd_cmd_fifo, p_work_dir);
         (void)strcat(afd_cmd_fifo, FIFO_DIR);
         (void)strcat(afd_cmd_fifo, AFD_ACTIVE_FILE);
         if (unlink(afd_cmd_fifo) == -1)
         {
            (void)fprintf(stderr, "Failed to unlink() %s : %s (%s %d)\n",
                          afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   else if (FD_ISSET(afd_resp_fd, &rset))
        {
           if (read(afd_resp_fd, buffer, DEFAULT_BUFFER_SIZE) > 0)
           {
              if (buffer[0] == ACKN)
              {
                 /* Tell user we are done */
                 (void)rec(sys_log_fd, INFO_SIGN, "Done!\n");
              }
              else
              {
                 (void)fprintf(stderr,
                               "Hmm. Something is wrong here! (%s %d)\n",
                               __FILE__, __LINE__);
              }
           }
        }
   else if (status < 0)
        {
           (void)rec(sys_log_fd, FATAL_SIGN, "Select error : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
           exit(INCORRECT);
        }
        else
        {
           (void)rec(sys_log_fd, FATAL_SIGN, "Unknown condition. (%s %d)\n",
                     __FILE__, __LINE__);
           exit(INCORRECT);
        }

   return;
}
