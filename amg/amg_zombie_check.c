/*
 *  amg_zombie_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
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
 **   amg_zombie_check - checks if any process terminated that was
 **                      started by the AMG
 **
 ** SYNOPSIS
 **   int amg_zombie_check(pid_t *proc_id, int option)
 **
 ** DESCRIPTION
 **   The function amg_zombie_check() checks if any process is finished
 **   (zombie), if this is the case it is killed with waitpid().
 **
 ** RETURN VALUES
 **   Returns YES when the status of a process has changed (except
 **   when it has been put to sleep). Otherwise NO is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   27.03.1998 H.Kiehl Put this function into a separate file.
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <sys/wait.h>                 /* waitpid()                       */
#include "amgdefs.h"


/*########################## amg_zombie_check() #########################*/
int
amg_zombie_check(pid_t *proc_id, int option)
{
   int   status,
         table_changed = NO;

   /* Is process a zombie? */
   if (waitpid(*proc_id, &status, option) > 0)
   {
      int exit_status = 0;

      if (WIFEXITED(status))
      {
         if (WEXITSTATUS(status) == 0)
         {
            exit_status = NOT_RUNNING;
         }
         else
         {
            exit_status = DIED;
         }
      }
      else  if (WIFSIGNALED(status))
            {
               /* abnormal termination */
               exit_status = DIED;
            }
            else  if (WIFSTOPPED(status))
                  {
                     /* Child stopped */
                     exit_status = STOPPED;
                  }

      /* update table */
      if (exit_status < STOPPED)
      {
         table_changed = YES;
         *proc_id = exit_status;
      }
   }

   return(table_changed);
}
