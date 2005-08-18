/*
 *  exec_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Deutscher Wetterdienst (DWD),
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
 **   exec_cmd - executes a shell command
 **
 ** SYNOPSIS
 **   int exec_cmd(char   *cmd,
 **                char   *buffer,
 **                int    log_fd,
 **                char   *name,
 **                int    name_length,
 **                time_t exec_timeout)
 **
 ** DESCRIPTION
 **   exec_cmd() executes a command specified in 'cmd' by calling
 **   /bin/sh -c 'cmd', and returns after the command has been
 **   completed. If log_fd is greater then -1, the command
 **   gets logged to that file.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to execute the command 'cmd'.
 **   In buffer will be the results of STDOUT and STDERR.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.02.1997 H.Kiehl Created
 **   03.10.1998 H.Kiehl Try reading from pipe while program is running
 **                      to avoid any deadlocks.
 **   14.07.2000 H.Kiehl Return the exit status of the process we started.
 **   10.07.2002 H.Kiehl Initialize the return buffer.
 **   20.08.2004 H.Kiehl Log command to a log file when log_fd is greater
 **                      then -1.
 **   04.09.2004 H.Kiehl Added exec_timeout parameter.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <stdlib.h>                   /* exit()                          */
#include <time.h>                     /* time()                          */
#include <signal.h>
#include <unistd.h>                   /* read(), close(), STDOUT_FILENO, */
                                      /* STDERR_FILENO, select()         */
#include <sys/time.h>                 /* struct timeval                  */
#include <sys/types.h>
#include <sys/wait.h>                 /* waitpid()                       */
#include <sys/times.h>                /* times()                         */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>                    /* O_NONBLOCK                      */
#endif
#include <errno.h>

#define  READ  0
#define  WRITE 1


/*############################## exec_cmd() #############################*/
int
exec_cmd(char   *cmd,
         char   *buffer,
         int    log_fd,
         char   *name,
         int    name_length,
         time_t exec_timeout)
{
   int        exit_status = INCORRECT,
              channels[2];
   char       *p_cmd;
   pid_t      child_pid;
   clock_t    start_time;
   struct tms tval;

   if (pipe(channels) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pipe() error : %s", strerror(errno));
      return(INCORRECT);
   }
   if (log_fd > -1)
   {
      start_time = times(&tval);
      p_cmd = cmd;
      while ((*p_cmd != '&') && (*p_cmd != '\0'))
      {
         p_cmd++;
      }
      if (*p_cmd == '&')
      {
         if (*(p_cmd + 1) == '&')
         {
            p_cmd += 3;
         }
      }
      else
      {
         p_cmd = cmd;
      }
   }

   switch (child_pid = fork())
   {
      case -1: /* Failed to fork */
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fork() error : %s", strerror(errno));
         return(INCORRECT);

      case 0 : /* Child process */
         {
            (void)close(channels[READ]);
            dup2(channels[WRITE], STDOUT_FILENO);
            dup2(channels[WRITE], STDERR_FILENO);
            if (log_fd > -1)
            {
#if SIZEOF_PID_T == 4
               (void)rec(log_fd, INFO_SIGN, "%-*s: [%d] %s\n",
#else
               (void)rec(log_fd, INFO_SIGN, "%-*s: [%lld] %s\n",
#endif
                         name_length, name, getpid(), p_cmd);
            }
            (void)execl("/bin/sh", "sh", "-c", cmd, (char *)0);
         }
         _exit(INCORRECT);

      default: /* Parent process */
         (void)close(channels[WRITE]);

         /*
          * It can happen that a program can talk so much
          * garbage that it fills the pipe buffer of the
          * the kernel, thus causing a deadlock. So we
          * do need to check and read data from the pipe.
          * Simply sitting and waiting for the process
          * to return can be fatal.
          */
         {
            int            bytes_read = 0,
                           n,
                           proc_status,
                           status;
            fd_set         rset;
            time_t         exec_start_time;
            struct timeval timeout;

            if (exec_timeout > 0L)
            {
               exec_start_time = time(NULL);
            }

            buffer[0] = '\0';
            FD_ZERO(&rset);
            for (;;)
            {
               if (waitpid(child_pid, &proc_status, WNOHANG) > 0)
               {
                  if (log_fd > -1)
                  {
                     long clktck;

                     if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
                     {
#if SIZEOF_PID_T == 4
                        (void)rec(log_fd, INFO_SIGN, "%-*s: [%d] Done.\n",
#else
                        (void)rec(log_fd, INFO_SIGN, "%-*s: [%lld] Done.\n",
#endif
                                  name_length, name, child_pid);
                     }
                     else
                     {
                        (void)rec(log_fd, INFO_SIGN,
#if SIZEOF_PID_T == 4
                                  "%-*s: [%ld] Exec time: %.3fs\n",
#else
                                  "%-*s: [%lld] Exec time: %.3fs\n",
#endif
                                  name_length, name, child_pid,
                                  (times(&tval) - start_time) / (double)clktck);
                     }
                  }

                  if (WIFEXITED(proc_status))
                  {
                     if ((exit_status = WEXITSTATUS(proc_status)))
                     {
                        /*
                         * If the exit status is non-zero lets assume
                         * that we have failed to execute the command.
                         */
                        set_fl(channels[READ], O_NONBLOCK);
                        if ((n = read(channels[READ], &buffer[bytes_read],
                                      (MAX_PATH_LENGTH + MAX_PATH_LENGTH - bytes_read))) < 0)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "read() error : %s", strerror(errno));
                           return(INCORRECT);
                        }
                        if (n > 0)
                        {
                           buffer[bytes_read + n] = '\0';
                        }
                     }
                  }
                  else
                  {
                     exit_status = INCORRECT;
                  }
                  break;
               }

               /* Initialise descriptor set and timeout */
               FD_SET(channels[READ], &rset);
               timeout.tv_usec = 50000L;
               timeout.tv_sec = 0L;

               status = select(channels[READ] + 1, &rset, NULL, NULL, &timeout);

               if (status == 0)
               {
                  /* Hmmm. Nothing to be done. */;
               }
               else if (FD_ISSET(channels[READ], &rset))
                    {
                       if ((MAX_PATH_LENGTH - bytes_read) < 2)
                       {
                          bytes_read = 0;
                       }
                       if ((n = read(channels[READ], &buffer[bytes_read],
                                     (MAX_PATH_LENGTH - bytes_read))) < 0)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "read() error : %s", strerror(errno));
                          return(INCORRECT);
                       }
                       if (n > 0)
                       {
                          bytes_read += n;
                          buffer[bytes_read] = '\0';
                       }
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "select() error : %s", strerror(errno));
                       return(INCORRECT);
                    }

               if (exec_timeout > 0L)
               {
                  if ((time(NULL) - exec_start_time) > exec_timeout)
                  {
                     if (kill(child_pid, SIGINT) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                   "Failed to kill() process %d, due to exec timeout.",
#else
                                   "Failed to kill() process %lld, due to exec timeout.",
#endif
                                   child_pid);
                     }
                     my_usleep(10000L);

                     if (waitpid(child_pid, NULL, WNOHANG) == child_pid)
                     {
                        if (log_fd > -1)
                        {
                           (void)rec(log_fd, WARN_SIGN,
#if SIZEOF_PID_T == 4
                                     "%-*s: [%d] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#else
                                     "%-*s: [%lld] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#endif
                                     name_length, name, child_pid, p_cmd,
                                     exec_timeout);
                        }
                        exit_status = INCORRECT;
                        break;
                     }
                     else
                     {
                        int counter = 0;

                        if (kill(child_pid, SIGKILL) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                      "Failed to kill() process %d, due to exec timeout.",
#else
                                      "Failed to kill() process %lld, due to exec timeout.",
#endif
                                      child_pid);
                        }
                        do
                        {
                           my_usleep(10000L);
                           counter++;
                           if (waitpid(child_pid, NULL, WNOHANG) == child_pid)
                           {
                              if (log_fd > -1)
                              {
                                 (void)rec(log_fd, WARN_SIGN,
#if SIZEOF_PID_T == 4
                                           "%-*s: [%d] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#else
                                           "%-*s: [%lld] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#endif
                                           name_length, name, child_pid, p_cmd,
                                           exec_timeout);
                              }
                              counter = 101;
                           }
                        } while (counter < 101);
                        exit_status = INCORRECT;
                        break;
                     }
                  }
               }
            } /* for (;;) */
         }

         if (close(channels[READ]) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }

         break;
   }

   return(exit_status);
}
