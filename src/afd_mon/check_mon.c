/*
 *  check_mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2003 Deutscher Wetterdienst (DWD),
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
 **   check_mon - Checks if another mafd process is running in this
 **               directory
 **
 ** SYNOPSIS
 **   int check_mon(long wait_time)
 **
 ** DESCRIPTION
 **   This function checks if another mafd is running. If not and the
 **   mafd process has crashed, it removes all jobs that might have
 **   survived the crash.
 **   This function is always called when we start the mafd process.
 **
 ** RETURN VALUES
 **   Returns 1 if another mafd process is active, otherwise it returns 0.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.08.1998 H.Kiehl Created
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>               /* O_RDWR, O_CREAT, O_WRONLY, etc       */
#endif
#include <errno.h>
#include "mondefs.h"

/* Local global variables */
static struct stat stat_buf;

/* External global variables */
extern int  sys_log_fd;
extern char *p_work_dir,
            mon_active_file[],
            mon_cmd_fifo[],
            probe_only_fifo[];

/* Local functions */
static void kill_jobs(void);


/*############################ check_mon() ##############################*/
int
check_mon(long wait_time)
{
   int            read_fd,
                  mon_cmd_fd,
                  n,
                  status;
   char           buffer[2];
#ifdef _FIFO_DEBUG
   char           cmd[2];
#endif
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf_fifo;

   if (stat(mon_active_file, &stat_buf) == 0)
   {
      /*
       * Uups. Seems like another mafd process is running.
       * Make sure that this is really the case. It could be that
       * when it crashed really hard that it had no time to
       * remove this file.
       */
      if ((mon_cmd_fd = open(mon_cmd_fifo, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to open() `%s' : %s (%s %d)\n",
                   mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         /* Now we have no way to determine if another */
         /* AFD is still running. Lets kill ALL jobs   */
         /* which appear in the 'mon_active_file'.     */
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
               exit(INCORRECT);
            }
         }
         if ((read_fd = open(probe_only_fifo, (O_RDONLY | O_NONBLOCK))) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not open fifo %s : %s (%s %d)\n",
                      probe_only_fifo, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /* Make sure there is no garbage in the fifo. */
         while (read(read_fd, buffer, 1) > 0)
         {
            ;
         }
#ifdef _FIFO_DEBUG
         cmd[0] = IS_ALIVE; cmd[1] = '\0';
         show_fifo_data('W', "mon_cmd", cmd, 1, __FILE__, __LINE__);
#endif
         if (send_cmd(IS_ALIVE, mon_cmd_fd) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Was not able to send command via fifo. (%s %d)\n",
                      __FILE__, __LINE__);
            exit(INCORRECT);
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
            /* No answer from the other AFD_MON. Lets       */
            /* assume it has crashed. Now we have to remove */
            /* any old jobs and shared memory areas that    */
            /* have survived the crash.                     */
            kill_jobs();
         }
         else if (FD_ISSET(read_fd, &rset))
              {
                 /* Ahhh! Another AFD_MON is working here. */
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
                       if (close(mon_cmd_fd) == -1)
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
                       exit(INCORRECT);
                    }
                 }
                 else if (n < 0)
                      {
                         (void)rec(sys_log_fd, ERROR_SIGN,
                                   "read() error : %s (%s %d)\n",
                                   strerror(errno),  __FILE__, __LINE__);
                         exit(INCORRECT);
                      }
              }
         else if (status < 0)
              {
                 (void)rec(sys_log_fd, FATAL_SIGN,
                           "Select error : %s (%s %d)\n",
                           strerror(errno),  __FILE__, __LINE__);
                 exit(INCORRECT);
              }
              else
              {
                 (void)rec(sys_log_fd, FATAL_SIGN,
                           "Unknown condition. Maybe you can tell what's going on here. (%s %d)\n",
                           __FILE__, __LINE__);
                 exit(INCORRECT);
              }

         if (close(read_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         if (close(mon_cmd_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   /* Else we did not find the AFD_MON active file */
   /* and we may just continue.                    */

   return(0);
}


/*++++++++++++++++++++++++++++ kill_jobs() ++++++++++++++++++++++++++++++*/
static void
kill_jobs(void)
{
   char *ptr,
        *buffer;
   int  i,
        no_of_process,
        read_fd;

   if ((read_fd = open(mon_active_file, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to open %s : %s (%s %d)\n",
                mon_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((buffer = calloc(stat_buf.st_size, sizeof(char))) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n",
                strerror(errno),  __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (read(read_fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "read() error : %s (%s %d)\n",
                strerror(errno),  __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(read_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno),  __FILE__, __LINE__);
   }

   /* Kill the log processes. */
   ptr = buffer;
   ptr += sizeof(pid_t); /* Ignore afd_mon process */
   if (*(pid_t *)ptr > 0)
   {
      (void)kill(*(pid_t *)ptr, SIGINT); /* system log */
   }
   ptr += sizeof(pid_t);
   if (*(pid_t *)ptr > 0)
   {
      (void)kill(*(pid_t *)ptr, SIGINT); /* monitor log */
   }
   ptr += sizeof(pid_t);

   /* Try to send kill signal to all running process */
   no_of_process = *(int *)ptr;
   ptr += sizeof(pid_t);
   for (i = 0; i < no_of_process; i++)
   {
      if (*(pid_t *)ptr > 0)
      {
         (void)kill(*(pid_t *)ptr, SIGINT);
      }
      ptr += sizeof(pid_t);
   }

   if (buffer != NULL)
   {
      free(buffer);
   }

   return;
}
