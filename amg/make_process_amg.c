/*
 *  make_process_amg.c - Part of AFD, an automatic file distribution program.
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
 **   make_process_amg - generates a new process
 **
 ** SYNOPSIS
 **   pid_t make_process_amg(char *work_dir,
 **                          char *prog_name,
 **                          int  shmid,
 **                          int  rescan_time,
 **                          int  max_process,
 **                          int  no_of_directories)
 **
 ** DESCRIPTION
 **   The function make_process_amg() allows the AMG to generate new
 **   processes.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when process creation was successful,
 **   otherwise INCORRECT is returned.
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

#include <stdio.h>              /* sprintf()                             */
#include <string.h>             /* strerror()                            */
#include <sys/types.h>
#include <unistd.h>             /* fork(), exit()                        */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int sys_log_fd;


/*########################## make_process_amg() #########################*/
pid_t
make_process_amg(char *work_dir,
                 char *prog_name,
                 int  shmid,
                 int  rescan_time,
                 int  max_process,
                 int  no_of_directories)
{
   static pid_t proc_id;
   char         shm_str[MAX_INT_LENGTH],
                rt_str[MAX_INT_LENGTH],
                mp_str[MAX_INT_LENGTH],
                nd_str[MAX_INT_LENGTH];

   /* First convert shmid into a char string */
   (void)sprintf(shm_str, "%d", shmid);
   (void)sprintf(rt_str, "%d", rescan_time);
   (void)sprintf(mp_str, "%d", max_process);
   (void)sprintf(nd_str, "%d", no_of_directories);

   switch(proc_id = fork())
   {
      case -1: /* Could not generate process */
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Could not create a new process : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);

      case  0: /* Child process */
               if (execlp(prog_name, prog_name, work_dir, shm_str,
                          rt_str, mp_str, nd_str, (char *)0) < 0)
               {                                                
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Failed to start process %s : %s (%s %d)\n",
                            prog_name, strerror(errno), __FILE__, __LINE__);
                  _exit(INCORRECT);
               }
               exit(SUCCESS);

      default: /* Parent process */
               return(proc_id);
   }

   return(INCORRECT);
}
