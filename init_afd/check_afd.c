/*
 *  check_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Deutscher Wetterdienst (DWD),
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
 **   check_afd - Checks if another afd is running in this directory
 **
 ** SYNOPSIS
 **   int check_afd(long wait_time)
 **
 ** DESCRIPTION
 **   This function checks if another afd is running. If not and the
 **   AFD has crashed, it removes all jobs and shared memory areas
 **   which might have survived the crash.
 **   This function is always called when we start the AFD.
 **
 ** RETURN VALUES
 **   Returns 1 if another AFD is active, otherwise it returns 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.04.1996 H.Kiehl Created
 **   03.05.1998 H.Kiehl Added wait_time parameter to function call.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   12.05.2005 H.Kiehl Do not bail out if AFD_CONFIG is zero.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* NULL                                 */
#include <stdlib.h>              /* calloc(), free()                     */
#include <string.h>              /* strerror(), memset()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>            /* struct timeval, FD_SET...            */
#include <signal.h>              /* kill()                               */
#include <unistd.h>              /* select()                             */
#include <fcntl.h>               /* O_RDWR, O_CREAT, O_WRONLY, etc       */
#include <errno.h>

/* Local global variables */
static struct stat stat_buf;

/* External global variables */
extern int  sys_log_fd;
extern char *p_work_dir,
            afd_active_file[],
            afd_cmd_fifo[],
            probe_only_fifo[];

/* Local functions */
static void kill_jobs(void);


/*############################ check_afd() ##############################*/
int
check_afd(long wait_time)
{
   int            read_fd,
                  afd_cmd_fd,
                  n,
                  status;
   char           buffer[2];
#ifdef _FIFO_DEBUG
   char           cmd[2];
#endif
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf_fifo;

   if (stat(afd_active_file, &stat_buf) == 0)
   {
      /*
       * Uups. Seems like another AFD is running.
       * Make sure that this is really the case. It could be that
       * when the AFD crashes really hard that it had no time to
       * remove this file.
       */
      if ((afd_cmd_fd = coe_open(afd_cmd_fifo, O_RDWR)) < 0)
      {
         /* Now we have no way to determine if another */
         /* AFD is still running. Lets kill ALL jobs   */
         /* which appear in the 'afd_active_file'.     */
         kill_jobs();
      }
      else
      {
         if ((stat(probe_only_fifo, &stat_buf_fifo) < 0) ||
             (!S_ISFIFO(stat_buf_fifo.st_mode)))
         {
            if (make_fifo(probe_only_fifo) < 0)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Could not create fifo %s. (%s %d)\n",
                         probe_only_fifo, __FILE__, __LINE__);
               exit(-3);
            }
         }
         if ((read_fd = coe_open(probe_only_fifo, (O_RDONLY | O_NONBLOCK))) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not open fifo %s : %s (%s %d)\n",
                      probe_only_fifo, strerror(errno), __FILE__, __LINE__);
            exit(-4);
         }

         /* Make sure there is no garbage in the fifo. */
         while (read(read_fd, buffer, 1) > 0)
         {
            ;
         }
#ifdef _FIFO_DEBUG
         cmd[0] = IS_ALIVE; cmd[1] = '\0';
         show_fifo_data('W', "afd_cmd", cmd, 1, __FILE__, __LINE__);
#endif
         if (send_cmd(IS_ALIVE, afd_cmd_fd) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Was not able to send command via fifo. (%s %d)\n",
                      __FILE__, __LINE__);
            exit(-5);
         }

         /* Lets see if it's still alive. For this we   */
         /* listen on another fifo. If we listen on the */
         /* same fifo we might read our own request.    */
         FD_ZERO(&rset);
         FD_SET(read_fd, &rset);
         timeout.tv_usec = 0L;
         timeout.tv_sec = wait_time;

         /* Wait for message x seconds and then continue. */
         status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

         if (status == 0)
         {
            /* No answer from the other AFD. Lets assume it */
            /* has crashed. Now we have to remove any old   */
            /* jobs and shared memory areas that have       */
            /* survived the crash.                          */
            kill_jobs();
         }
         else if (FD_ISSET(read_fd, &rset))
              {
                 /* Ahhh! Another AFD is working here. */
                 /* Lets quickly vanish before someone */
                 /* notice our presence.               */
                 if ((n = read(read_fd, buffer, 1)) > 0)
                 {
#ifdef _FIFO_DEBUG
                    show_fifo_data('R', "probe_only", buffer, 1, __FILE__, __LINE__);
#endif
                    if (buffer[0] == ACKN)
                    {
                       if (close(read_fd) == -1)
                       {
                          (void)rec(sys_log_fd, DEBUG_SIGN,
                                    "close() error : %s (%s %d)\n",
                                    strerror(errno), __FILE__, __LINE__);
                       }
                       if (close(afd_cmd_fd) == -1)
                       {
                          (void)rec(sys_log_fd, DEBUG_SIGN,
                                    "close() error : %s (%s %d)\n",
                                    strerror(errno), __FILE__, __LINE__);
                       }
                       return(1);
                    }
                    else
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Reading garbage from fifo %s. (%s %d)\n",
                                 probe_only_fifo,  __FILE__, __LINE__);
                       exit(-6);
                    }
                 }
                 else if (n < 0)
                      {
                         (void)rec(sys_log_fd, ERROR_SIGN,
                                   "read() error : %s (%s %d)\n",
                                   strerror(errno),  __FILE__, __LINE__);
                         exit(-7);
                      }
              }
         else if (status < 0)
              {
                 (void)rec(sys_log_fd, FATAL_SIGN,
                           "Select error : %s (%s %d)\n",
                           strerror(errno),  __FILE__, __LINE__);
                 exit(-8);
              }
              else
              {
                 (void)rec(sys_log_fd, FATAL_SIGN,
                           "Unknown condition. Maybe you can tell what's going on here. (%s %d)\n",
                           __FILE__, __LINE__);
                 exit(-9);
              }

         if (close(read_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         if (close(afd_cmd_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   /* Else we did not find the AFD active file and */
   /* we may just continue.                        */

   return(0);
}


/*++++++++++++++++++++++++++++ kill_jobs() ++++++++++++++++++++++++++++++*/
static void
kill_jobs(void)
{
   if (stat_buf.st_size > 0)
   {
      char *ptr,
           *buffer;
      int  i,
           read_fd;

      if ((read_fd = open(afd_active_file, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open %s : %s (%s %d)\n",
                   afd_active_file, strerror(errno), __FILE__, __LINE__);
         exit(-10);
      }

      if ((buffer = calloc(stat_buf.st_size, sizeof(char))) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "calloc() error : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         exit(-11);
      }

      if (read(read_fd, buffer, stat_buf.st_size) != stat_buf.st_size)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "read() error : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         exit(-12);
      }

      /* Try to send kill signal to all running process. */
      ptr = buffer;
      for (i = 0; i <= NO_OF_PROCESS; i++)
      {
         if (*(pid_t *)ptr > 0)
         {
            (void)kill(*(pid_t *)ptr, SIGINT);
         }
         ptr += sizeof(pid_t);
      }

      (void)close(read_fd);
      free(buffer);
   }

   return;
}
