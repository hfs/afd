/*
 *  exec_cmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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
 **   int exec_cmd(char *cmd, char *buffer)
 **
 ** DESCRIPTION
 **   exec_cmd() executes a command specified in 'cmd' by calling
 **   /bin/sh -c 'cmd', and returns after the command has been
 **   completed.
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
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strlen()                        */
#include <stdlib.h>                   /* exit()                          */
#include <unistd.h>                   /* read(), close(), STDOUT_FILENO, */
                                      /* STDERR_FILENO, select()         */
#include <sys/time.h>                 /* struct timeval                  */
#include <sys/types.h>
#include <sys/wait.h>                 /* waitpid()                       */
#include <fcntl.h>                    /* O_NONBLOCK                      */
#include <errno.h>

#define  READ  0
#define  WRITE 1


/*############################## exec_cmd() #############################*/
int
exec_cmd(char *cmd, char *buffer)
{
   int   exit_status = INCORRECT,
         channels[2];
   pid_t child_pid;

   if (pipe(channels) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pipe() error : %s", strerror(errno));
      return(INCORRECT);
   }

   switch (child_pid = fork())
   {
      case -1: /* Failed to fork */
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "fork() error : %s", strerror(errno));
         return(INCORRECT);

      case 0 : /* Child process */
         (void)close(channels[READ]);
         dup2(channels[WRITE], STDOUT_FILENO);
         dup2(channels[WRITE], STDERR_FILENO);
         (void)execl("/bin/sh", "sh", "-c", cmd, (char *)0);
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
            struct timeval timeout;

            buffer[0] = '\0';
            FD_ZERO(&rset);
            for (;;)
            {
               if (waitpid(child_pid, &proc_status, WNOHANG) > 0)
               {
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
