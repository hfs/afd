/*
 *  make_xprocess.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   make_xprocess - creates a new process
 **
 ** SYNOPSIS
 **   void make_xprocess(char *progname,
 **                      char *real_progname,
 **                      char **args,
 **                      int position)
 **
 ** DESCRIPTION
 **   This function forks to start another process. The process id
 **   as well as the process name is held in the structure apps_list.
 **   For each process that is started with this function a
 **   XtAppAddInput() handler is started, that removes the process
 **   from the structure apps_list.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.09.1998 H.Kiehl Created
 **   24.01.1999 H.Kiehl If command (progname, args) returns anything
 **                      it is shown with warn dialog.
 **
 */
DESCR__E_M3

#include <string.h>         /* strerror(), memmove()                     */
#include <unistd.h>         /* fork(), read(), close()                   */
#include <stdlib.h>         /* realloc(), free()                         */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>       /* waitpid()                                 */
#include <errno.h>

#include <Xm/Xm.h>
#include "x_common_defs.h"

/* External global variables */
extern int              no_of_active_process,
                        sys_log_fd;
extern Widget           appshell;
extern struct apps_list *apps_list;

/* Local function prototypes */
static void             check_fork(XtPointer, int *, XtInputId *);


/*+++++++++++++++++++++++++++ make_xprocess() +++++++++++++++++++++++++++*/
void
make_xprocess(char *progname,
              char *real_progname,
              char **args,
              int position)
{
   pid_t pid;
   int   fork_fds[2];

   if (pipe(fork_fds) == -1)
   {
      (void)xrec(appshell, FATAL_DIALOG, "pipe() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   if ((no_of_active_process % 10) == 0)
   {
      size_t new_size = ((no_of_active_process / 10) + 1) *
                        10 * sizeof(struct apps_list);

      if ((apps_list = realloc(apps_list, new_size)) == NULL)
      {
         (void)xrec(appshell, FATAL_DIALOG, "realloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
   }
   if (pipe(apps_list[no_of_active_process].app_reply_fd) == -1)
   {
      (void)xrec(appshell, FATAL_DIALOG, "pipe() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   if ((pid = fork()) < 0)
   {
      (void)xrec(appshell, FATAL_DIALOG, "Failed to fork() : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   else if (pid == 0)
        {
           (void)close(fork_fds[0]);
           (void)close(apps_list[no_of_active_process].app_reply_fd[0]);
           dup2(apps_list[no_of_active_process].app_reply_fd[1], STDOUT_FILENO);
           dup2(apps_list[no_of_active_process].app_reply_fd[1], STDERR_FILENO);
#ifdef _RESOLVE_RSH_PROBLEM_
           if ((progname[0] == 'r') &&
               (progname[1] == 's') &&
               (progname[2] == 'h') &&
               (progname[3] == '\0'))
           {
              if (freopen("/dev/null", "r+", stdin) == NULL)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Failed to redirect stdin to /dev/null : %s (%s %d)\n",
                           strerror(errno), __FILE__, __LINE__);
              }
           }
#endif

           /* Child process */
           errno = 0;
           (void)execvp(progname, args); /* NOTE: NO return from execvp() */

           /* Tell parent errno of the execvp() command. */
           (void)write(fork_fds[1], &errno, sizeof(int));
           _exit(INCORRECT);
        }

   apps_list[no_of_active_process].pid = pid;
   apps_list[no_of_active_process].position = position;
   (void)strcpy(apps_list[no_of_active_process].progname, real_progname);
   (void)close(fork_fds[1]);
   (void)close(apps_list[no_of_active_process].app_reply_fd[1]);
   no_of_active_process++;
   (void)XtAppAddInput(XtWidgetToApplicationContext(appshell),
                       fork_fds[0], (XtPointer)XtInputReadMask,
                       (XtInputCallbackProc)check_fork,
                       (XtPointer)pid);

   return;
}


/*---------------------------- check_fork() -----------------------------*/
static void
check_fork(XtPointer client_data, int *fd, XtInputId *id)
{
   int   i,
         pos = -1,
         tmp_errno = 0;
   pid_t pid = (pid_t)client_data;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (apps_list[i].pid == pid)
      {
         pos = i;
         break;
      }
   }

   if (read(*fd, &tmp_errno, sizeof(int)) < 0)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "read() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (tmp_errno != 0)
   {
      if (pos != -1)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to start process %s : %s (%s %d)",
                    apps_list[pos].progname, strerror(tmp_errno),
                    __FILE__, __LINE__);
      }
      else
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to start process %d : %s (%s %d)",
                    pid, strerror(tmp_errno), __FILE__, __LINE__);
      }
   }
   if (close(*fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Check if any error occured while executing the command.
    * NOTE: We must catch any data in the app_reply_fd now and NOT
    *       after the waitpid(), since this will destroy this
    *       discriptor and select() on such a dummy will cause
    *       havoc!
    */
   if (pos != -1)
   {
      int            status;
      fd_set         rset;
      struct timeval timeout;

      FD_ZERO(&rset);
      FD_SET(apps_list[pos].app_reply_fd[0], &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 0L;
      status = select(apps_list[pos].app_reply_fd[0] + 1, &rset, NULL, NULL, &timeout);
      if (status == 0)
      {
         /* Seems program called had no complains. */;
      }
      else if (FD_ISSET(apps_list[pos].app_reply_fd[0], &rset))
           {
              int  n;
              char buffer[MAX_PATH_LENGTH];

              if ((n = read(apps_list[pos].app_reply_fd[0], buffer, MAX_PATH_LENGTH)) < 0)
              {
                 (void)xrec(appshell, WARN_DIALOG,
                            "read() error : %s (%s %d)",
                            strerror(errno), __FILE__, __LINE__);
              }
              else
              {
                 if (n > 0)
                 {
                    buffer[n] = '\0';
                    (void)xrec(appshell, WARN_DIALOG, "%s", buffer);
                 }
              }
           }
           else
           {
              (void)xrec(appshell, WARN_DIALOG,
                         "select() error : %s (%s %d)",
                         strerror(errno), __FILE__, __LINE__);
           }
      (void)close(apps_list[pos].app_reply_fd[0]);
   }

   if (waitpid(pid, NULL, 0) != pid)
   {
      (void)xrec(appshell, FATAL_DIALOG, "Failed to waitpid() : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   no_of_active_process--;
   if (no_of_active_process == 0)
   {
      if (apps_list != NULL)
      {
         free(apps_list);
         apps_list = NULL;
      }
   }
   else
   {
      if (pos != -1)
      {
         if (pos != no_of_active_process)
         {
            size_t move_size = (no_of_active_process - pos) *
                               sizeof(struct apps_list);

            (void)memmove(&apps_list[pos], &apps_list[pos + 1], move_size);
         }
      }
   }

   XtRemoveInput(*id);

   return;
}
