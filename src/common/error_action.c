/*
 *  error_action.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void error_action(char *alias_name, char *action)
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

/* External global variables */
extern char *p_work_dir;


/*########################### error_action() ############################*/
void
error_action(char *alias_name, char *action)
{
   char fullname[MAX_PATH_LENGTH];

   (void)sprintf(fullname, "%s%s%s/%s",
                 p_work_dir, ETC_DIR, ERROR_ACTION_DIR, alias_name);
   if (eaccess(fullname, X_OK) == 0)
   {
      pid_t pid;

      if ((pid = fork()) < 0)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s", strerror(errno));
         return;
      }
      else if (pid == 0) /* First child process */
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
              _exit(SUCCESS);
           }

      if (waitpid(pid, NULL, 0) != pid)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                    "Failed to wait for pid %ld : %s", pid, strerror(errno));
#else
                    "Failed to wait for pid %lld : %s", pid, strerror(errno));
#endif
      }
   }
   return;
}
