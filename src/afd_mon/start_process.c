/*
 *  start_process.c - Part of AFD, an automatic file distribution program.
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
 **   start_process - starts a process
 **
 ** SYNOPSIS
 **   pid_t start_process(char *progname, int afd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <unistd.h>            /* fork(), execlp(), _exit()              */
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern int  sys_log_fd;
extern char *p_work_dir;


/*########################### start_process() ###########################*/
pid_t
start_process(char *progname, int afd)
{
   static pid_t pid;

   switch (pid = fork())
   {
      case -1 : /* Error creating process */

         /* Could not generate process */
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not create a new process : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         return(INCORRECT);

      case  0 : /* Child process */

         if (afd == -1)
         {
            (void)execlp(progname, progname, WORK_DIR_ID,
                         p_work_dir, (char *)0);
         }
         else
         {
            char str_num[MAX_INT_LENGTH];

            (void)sprintf(str_num, "%d", afd);
            (void)execlp(progname, progname, WORK_DIR_ID,
                         p_work_dir, str_num, (char *)0);
         }
         _exit(INCORRECT);

      default : /* Parent process */

         return(pid);
   }
}
