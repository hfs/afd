/*
 *  error_action.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   error_action - call a script and execute it if it exists
 **
 ** SYNOPSIS
 **   void error_action(char *alias_name, char *action, int type)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. When it fails it will exit with INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.02.2005 H.Kiehl Created
 **   23.11.2007 H.Kiehl Added warn and error action for directories.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*########################### error_action() ############################*/
void
error_action(char *alias_name, char *action, int type)
{
   int  event_class;
   char fullname[MAX_PATH_LENGTH];

   if (type == HOST_ERROR_ACTION)
   {
      (void)sprintf(fullname, "%s%s%s/%s",
                    p_work_dir, ETC_DIR, ERROR_ACTION_DIR, alias_name);
      event_class = EC_HOST;
   }
   else if (type == DIR_WARN_ACTION)
        {
           (void)sprintf(fullname, "%s%s%s/%s",
                         p_work_dir, ETC_DIR, DIR_WARN_ACTION_DIR, alias_name);
           event_class = EC_DIR;
        }
        else
        {
           (void)sprintf(fullname, "%s%s%s/%s",
                         p_work_dir, ETC_DIR, DIR_ERROR_ACTION_DIR, alias_name);
           event_class = EC_DIR;
        }

   if (eaccess(fullname, X_OK) == 0)
   {
      int   status;
      pid_t pid;
      char  reason_str[38 + MAX_INT_LENGTH + 1];

      if ((pid = fork()) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         return;
      }
      else if (pid == 0) /* Child process. */
           {
              if ((pid = fork()) < 0)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Could not create a new process : %s",
                            strerror(errno));
                 _exit(INCORRECT);
              }
              else if (pid > 0)
                   {
                      _exit(SUCCESS);
                   }

              if (execlp(fullname, fullname, action, (char *)0) < 0)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to start process %s : %s",
                            fullname, strerror(errno));
                 _exit(INCORRECT);
              }
              else
              {
                 system_log(DEBUG_SIGN, NULL, 0,
                            "Error action: %s %s", fullname, action);
              }
              _exit(SUCCESS);
           }

      if (waitpid(pid, &status, 0) != pid)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                    "Failed to wait for pid %ld : %s",
#else
                    "Failed to wait for pid %lld : %s",
#endif
                    (pri_pid_t)pid, strerror(errno));
      }
      if (WIFEXITED(status))
      {
         (void)sprintf(reason_str, "%d", WEXITSTATUS(status));
      }
      else if (WIFSIGNALED(status))
           {
              (void)sprintf(reason_str,
                            "Abnormal termination caused by signal %d",
                            WTERMSIG(status));
           }
           else
           {
              (void)strcpy(reason_str, "Unable to determine return code");
           }
      if ((action[0] == 's') && (action[1] == 't') && (action[2] == 'a') &&
          (action[3] == 'r') && (action[4] == 't') && (action[5] == '\0'))
      {
         event_log(0L, event_class, ET_AUTO, EA_EXEC_ERROR_ACTION_START,
                   "%s%c%s", alias_name, SEPARATOR_CHAR, reason_str);
      }
      else if ((action[0] == 's') && (action[1] == 't') && (action[2] == 'o') &&
               (action[3] == 'p') && (action[4] == '\0'))
           {
              event_log(0L, event_class, ET_AUTO, EA_EXEC_ERROR_ACTION_STOP,
                        "%s%c%s", alias_name, SEPARATOR_CHAR, reason_str);
           }
   }
   return;
}
