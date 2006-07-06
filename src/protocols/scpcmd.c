/*
 *  scpcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   scpcmd - commands to send files via the SCP protocol
 **
 ** SYNOPSIS
 **   int  scp_connect(char *hostname, int port, char *user,
 **                    char *passwd, char *dir)
 **   int  scp_open_file(char *filename, off_t size, mode_t mode)
 **   int  scp_close_file(void)
 **   int  scp_write(char *block, int size)
 **   void scp_quit(void)
 **
 ** DESCRIPTION
 **   scpcmd provides a set of commands to communicate with a SSH
 **   (Secure Shell) server via pipes. The following functions are
 **   required to send a file:
 **      scp_connect()    - build a connection to the SSH server
 **      scp_open_file()  - open a file
 **      scp_close_file() - close a file
 **      scp_write()      - write data to the pipe
 **      scp_quit()       - disconnect from the SSH server
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will return INCORRECT. 'timeout_flag' is just a flag to
 **   indicate that the 'transfer_timeout' time has been reached.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.04.2001 H.Kiehl        Created
 **   30.09.2001 H.Kiehl        Added functions scp1_cmd_connect(),
 **                             scp1_chmod() and scp1_move().
 **   10.03.2003 F.Olivie (Alf) Support for SCP2.
 **                             Added timeouts and identity file handling.
 **   10.12.2003 H.Kiehl        Remove everything with ctrl connection,
 **                             since it does not work on all systems.
 **                             The SCP protocol was not designed for this.
 **   25.12.2003 H.Kiehl        Added tracing.
 **   01.01.2006 H.Kiehl        Move ssh_cmd() and pty code to common.c.
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <sys/types.h>        /* fd_set                                  */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/stat.h>         /* S_ISUID, S_ISGID, etc                   */
#include <signal.h>           /* signal(), kill()                        */
#include <unistd.h>           /* select(), write(), close()              */
#include <errno.h>
#include "scpdefs.h"
#include "fddefs.h"           /* struct job                              */


/* External global variables */
extern int        timeout_flag;
extern char       msg_str[];
extern long       transfer_timeout;
extern char       tr_hostname[];
extern struct job db;

/* Local global variables */
static int        data_fd = -1;
static pid_t      data_pid = -1;

/* Local function prototypes. */
static void       sig_ignore_handler(int);


/*########################### scp_connect() #############################*/
int
scp_connect(char *hostname, int port, char *user, char *passwd, char *dir)
{
   int  status;
   char cmd[MAX_PATH_LENGTH];

   (void)sprintf(cmd, "scp -t %s", (dir[0] == '\0') ? "." : dir);

   if ((status = ssh_exec(hostname, port, user, passwd, cmd, NULL,
                          &data_fd, &data_pid)) == SUCCESS)
   {
      status = ssh_login(data_fd, passwd);
   }
   return(status);
}


/*########################## scp_open_file() ############################*/
int
scp_open_file(char *filename, off_t size, mode_t mode)
{
   int    status;
   size_t length;
   char   cmd[MAX_PATH_LENGTH];

#if SIZEOF_OFF_T == 4
   length = sprintf(cmd, "C%04o %ld %s\n",
#else
   length = sprintf(cmd, "C%04o %lld %s\n",
#endif
                    (mode & (S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)),
                    size, filename);
   if ((status = pipe_write(data_fd, cmd, length)) != length)
   {
      if (errno != 0)
      {
         cmd[length - 1] = '\0';
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Failed to pipe_write() <%s> to pipe [%d] : %s",
                   cmd, status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      status = get_ssh_reply(data_fd, YES);
   }
   return(status);
}


/*########################### scp_close_file() ##########################*/
int
scp_close_file(void)
{
   int status;

   if ((status = pipe_write(data_fd, "\0", 1)) != 1)
   {
      if (errno != 0)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Failed to pipe_write() [close file] to pipe [%d] : %s",
                   status, strerror(errno));
      }
      status = INCORRECT;
   }
   else
   {
      status = get_ssh_reply(data_fd, YES);
   }
   return(status);
}


/*############################## scp_write() ############################*/
int
scp_write(char *block, int size)
{
   int            status;
   fd_set         wset;
   struct timeval timeout;

   /* Initialise descriptor set */
   FD_ZERO(&wset);
   FD_SET(data_fd, &wset);
   timeout.tv_usec = 0L;
   timeout.tv_sec = transfer_timeout;

   /* Wait for message x seconds and then continue. */
   status = select(data_fd + 1, NULL, &wset, NULL, &timeout);

   if (status == 0)
   {
      /* timeout has arrived */
      timeout_flag = ON;
      return(INCORRECT);
   }
   else if (FD_ISSET(data_fd, &wset))
        {
           /* In some cases, the write system call hangs. */
           signal(SIGALRM, sig_ignore_handler); /* Ignore the default which */
                                                /* is to abort.             */
           my_siginterrupt(SIGALRM, 1); /* Allow SIGALRM to interrupt write. */

           alarm(transfer_timeout); /* Set up an alarm to interrupt write. */
           if ((status = write(data_fd, block, size)) != size)
           {
              int tmp_errno = errno;

              alarm(0);
              signal(SIGALRM, SIG_DFL); /* restore the default which is abort. */
              trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                        "write() error (%d) : %s", status, strerror(tmp_errno));
              return(tmp_errno);
           }
           alarm(0);
           signal(SIGALRM, SIG_DFL); /* Restore the default which is abort. */
#ifdef WITH_TRACE
           trace_log(NULL, 0, BIN_W_TRACE, block, size, NULL);
#endif
        }
   else if (status < 0)
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "select() error : %s", strerror(errno));
           return(INCORRECT);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                     "Unknown condition.");
           return(INCORRECT);
        }
   
   return(SUCCESS);
}


/*############################## scp_quit() #############################*/
void
scp_quit(void)
{
   /* Remove ssh process for writing data. */
   if (data_pid != -1)
   {
      int   loop_counter,
            max_waitpid_loops,
            status;
      pid_t return_pid;

      /* Close pipe for read/write data connection. */
      if (data_fd != -1)
      {
         if (close(data_fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to close() write pipe to ssh process : %s",
                      strerror(errno));
         }
         data_fd = -1;
      }

      loop_counter = 0;
      max_waitpid_loops = (transfer_timeout / 2) * 10;
      while (((return_pid = waitpid(data_pid, &status, WNOHANG)) != data_pid) &&
             (return_pid != -1) &&
             (loop_counter < max_waitpid_loops))
      {
         my_usleep(100000L);
         loop_counter++;
      }
      if ((return_pid == -1) || (loop_counter >= max_waitpid_loops))
      {
         msg_str[0] = '\0';
         if (return_pid == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to catch zombie of data ssh process : %s",
                      strerror(errno));
         }
         if (data_pid > 0)
         {
            if (kill(data_pid, SIGKILL) == -1)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_PID_T == 4
                         "Failed to kill() data ssh process %d : %s",
#else
                         "Failed to kill() data ssh process %lld : %s",
#endif
                         data_pid, strerror(errno));
            }
            else
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "Killing hanging data ssh process.");
            }
         }
         else
         {
            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_PID_T == 4
                      "Hmm, pid is %d!!!", data_pid);
#else
                      "Hmm, pid is %lld!!!", data_pid);
#endif
         }
      }
      data_pid = -1;
   }

   return;
}


/*+++++++++++++++++++++++++ sig_ignore_handler() ++++++++++++++++++++++++*/
static void
sig_ignore_handler(int signo)
{
   /* Do nothing. This just allows us to receive a SIGALRM, thus */
   /* interrupting a system call without being forced to abort.  */
}
