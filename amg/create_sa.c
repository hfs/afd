/*
 *  create_sa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_sa - creates the FSA and FRA of the AFD
 **
 ** SYNOPSIS
 **   void create_sa(int no_of_dirs)
 **
 ** DESCRIPTION
 **   This function creates the FSA (Filetransfer Status Area) and
 **   FRA (File Retrieve Area). See the functions create_fsa() and
 **   create_fra() for more details.

 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strcpy(), strcat(), strerror()    */
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* close()                           */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables */
extern char       *p_work_dir;
extern int        sys_log_fd;

/* Global variables */
int               first_time = YES;


/*############################# create_sa() #############################*/
void
create_sa(int no_of_dirs)
{
   create_fsa();
   create_fra(no_of_dirs);

   /* If this is the first time that the FSA is */
   /* created, notify AFD that we are done.     */
   if (first_time == YES)
   {
      int         afd_cmd_fd;
      char        afd_cmd_fifo[MAX_PATH_LENGTH];
#ifdef _FIFO_DEBUG
      char        cmd[2];
#endif
      struct stat stat_buf;

      (void)strcpy(afd_cmd_fifo, p_work_dir);
      (void)strcat(afd_cmd_fifo, FIFO_DIR);
      (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);

      /*
       * Check if fifos have been created. If not create and open them.
       */
      if ((stat(afd_cmd_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
      {
         if (make_fifo(afd_cmd_fifo) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to create fifo %s. (%s %d)\n",
                      afd_cmd_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open fifo %s : %s (%s %d)\n",
                   afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef _FIFO_DEBUG
      cmd[0] = AMG_READY; cmd[1] = '\0';
      show_fifo_data('W', "afd_cmd", cmd, 1, __FILE__, __LINE__);
#endif
      if (send_cmd(AMG_READY, afd_cmd_fd) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Was not able to send AMG_READY to %s. (%s %d)\n",
                   AFD, __FILE__, __LINE__);
      }
      first_time = NO;
      if (close(afd_cmd_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
