/*
 *  exec_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
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
 **                char   **buffer,
 **                int    log_fd,
 **                char   *name,
 **                int    name_length,
 **                char   *job,
 **                time_t exec_timeout,
 **                int    dup_stderr)
 **
 ** DESCRIPTION
 **   exec_cmd() executes a command specified in 'cmd' by calling
 **   /bin/sh -c 'cmd', and returns after the command has been
 **   completed. If log_fd is greater then -1, the command
 **   gets logged to that file.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to execute the command 'cmd'.
 **   In buffer will be the results of STDOUT and STDERR. The caller
 **   is responcible to free this memory.
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
 **   18.05.2006 H.Kiehl Allocate a ring buffer to read what is being
 **                      returned by the 'cmd', which the caller must
 **                      now free.
 **   11.02.2007 H.Kiehl Added job string when writting to log_fd.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <stdlib.h>                   /* exit(), realloc(), free()       */
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

#define READ            0
#define WRITE           1


/*############################## exec_cmd() #############################*/
int
exec_cmd(char   *cmd,
         char   **buffer,
         int    log_fd,
         char   *name,
         int    name_length,
         char   *job,
         time_t exec_timeout,
         int    dup_stderr)
{
   int        channels[2],
              fds[2],
              exit_status = INCORRECT,
              max_pipe_size,
              max_read_buffer;
   char       *p_cmd;
   pid_t      child_pid;
   clock_t    start_time;
   struct tms tval;

   if ((pipe(channels) == -1) || (pipe(fds) == -1))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pipe() error : %s", strerror(errno));
      return(INCORRECT);
   }

   if ((max_pipe_size = fpathconf(fds[0], _PC_PIPE_BUF)) < 0)
   {
      max_pipe_size = DEFAULT_FIFO_SIZE;
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
   if (dup_stderr == YES)
   {
      max_read_buffer = 131072;
   }
   else
   {
      max_read_buffer = 1048576;
   }

   switch (child_pid = fork())
   {
      case -1: /* Failed to fork */
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fork() error : %s", strerror(errno));
         return(INCORRECT);

      case 0 : /* Child process */
         {
            char dummy;

            (void)close(channels[READ]);
            dup2(channels[WRITE], STDOUT_FILENO);
            if (dup_stderr == YES)
            {
               dup2(channels[WRITE], STDERR_FILENO);
            }
            if (log_fd > -1)
            {
#if SIZEOF_PID_T == 4
               (void)rec(log_fd, INFO_SIGN, "%-*s%s: [%d] %s\n",
#else
               (void)rec(log_fd, INFO_SIGN, "%-*s%s: [%lld] %s\n",
#endif
                         name_length, name, job, (pri_pid_t)getpid(), p_cmd);
            }

            /* Synchronize with parent. */
            (void)close(fds[1]);
            if (read(fds[0], &dummy, 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "read() error : %s", strerror(errno));
            }
            (void)close(fds[0]);

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
                           have_warned = NO,
                           n = 0,
                           nleft,
                           proc_status,
                           status,
                           wrap_around = NO;
            char           *read_ptr;
            fd_set         rset;
            time_t         exec_start_time;
            struct timeval timeout;

            if (*buffer != NULL)
            {
               free(*buffer);
            }
            if ((*buffer = malloc(max_pipe_size + 1)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          max_pipe_size, strerror(errno));
               return(INCORRECT);
            }
            nleft = max_pipe_size;
            read_ptr = *buffer;
            *read_ptr = '\0';

            /* Synchronize with child. */
            (void)close(fds[0]);
            if (write(fds[1], "", 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "write() error : %s", strerror(errno));
            }
            (void)close(fds[1]);

            if (exec_timeout > 0L)
            {
               exec_start_time = time(NULL);
            }

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
                        (void)rec(log_fd, INFO_SIGN, "%-*s%s: [%d] Done.\n",
#else
                        (void)rec(log_fd, INFO_SIGN, "%-*s%s: [%lld] Done.\n",
#endif
                                  name_length, name, job, (pri_pid_t)child_pid);
                     }
                     else
                     {
                        (void)rec(log_fd, INFO_SIGN,
#if SIZEOF_PID_T == 4
                                  "%-*s%s: [%d] Exec time: %.3fs\n",
#else
                                  "%-*s%s: [%lld] Exec time: %.3fs\n",
#endif
                                  name_length, name, job, (pri_pid_t)child_pid,
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
                        read_ptr = *buffer;
                        set_fl(channels[READ], O_NONBLOCK);
                        if ((n = read(channels[READ], read_ptr, max_pipe_size)) < 0)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "read() error : %s", strerror(errno));
                           return(INCORRECT);
                        }
                        if (n > 0)
                        {
                           buffer[n] = '\0';
                        }
                     }
                     else
                     {
                        if (bytes_read >= max_read_buffer)
                        {
                           wrap_around = YES;
                           bytes_read = 0;
                           read_ptr = *buffer;
                           nleft = max_read_buffer;
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "exec_cmd(): Max read buffer reached! Starting from beginning.");
                        }
                        else
                        {
                           if (wrap_around == NO)
                           {
                              if (nleft < (max_pipe_size / 2))
                              {
                                 size_t size;

                                 size = bytes_read + max_pipe_size + 1;
                                 if ((*buffer = realloc(*buffer, size)) == NULL)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               "Failed to realloc() %d bytes : %s",
                                               size, strerror(errno));
                                    return(INCORRECT);
                                 }
                                 nleft = nleft + max_pipe_size;
                                 read_ptr = *buffer + bytes_read;
                              }
                           }
                        }
                        set_fl(channels[READ], O_NONBLOCK);
                        if ((n = read(channels[READ], read_ptr, nleft)) < 0)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "read() error : %s", strerror(errno));
                           return(INCORRECT);
                        }
                        if (n > 0)
                        {
                           *(read_ptr + n) = '\0';
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
                       if (bytes_read >= max_read_buffer)
                       {
                          wrap_around = YES;
                          bytes_read = 0;
                          read_ptr = *buffer;
                          nleft = max_read_buffer;
                          if (have_warned != YES)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "exec_cmd(): Max read buffer reached! Starting from beginning.");
                             have_warned = YES;
                          }
                       }
                       else
                       {
                          if (wrap_around == NO)
                          {
                             if (nleft == 0)
                             {
                                size_t size;

                                size = bytes_read + max_pipe_size + 1;
                                if ((*buffer = realloc(*buffer, size)) == NULL)
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "Failed to realloc() %d bytes : %s",
                                              size, strerror(errno));
                                   return(INCORRECT);
                                }
                                nleft = max_pipe_size;
                                read_ptr = *buffer + bytes_read;
                             }
                          }
                       }
                       if ((n = read(channels[READ], read_ptr, nleft)) < 0)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "read() error : %s", strerror(errno));
                          return(INCORRECT);
                       }
                       if (n > 0)
                       {
                          bytes_read += n;
                          nleft -= n;
                          read_ptr += n;
                          *read_ptr = '\0';
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
                                   (pri_pid_t)child_pid);
                     }
                     my_usleep(10000L);

                     if (waitpid(child_pid, NULL, WNOHANG) == child_pid)
                     {
                        if (log_fd > -1)
                        {
                           (void)rec(log_fd, WARN_SIGN,
#if SIZEOF_PID_T == 4
                                     "%-*s%s: [%d] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#else
                                     "%-*s%s: [%lld] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#endif
                                     name_length, name, job,
                                     (pri_pid_t)child_pid, p_cmd, exec_timeout);
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
                                      (pri_pid_t)child_pid);
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
                                           "%-*s%s: [%d] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#else
                                           "%-*s%s: [%lld] Killed command \"%s\" due to timeout (execution time > %lds).\n",
#endif
                                           name_length, name, job,
                                           (pri_pid_t)child_pid,
                                           p_cmd, exec_timeout);
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
