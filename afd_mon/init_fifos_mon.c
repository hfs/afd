/*
 *  init_fifos_mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fifos_mon - creates and opens all fifos needed by AFD_MON
 **
 ** SYNOPSIS
 **   int init_fifos_mon(void)
 **
 ** DESCRIPTION
 **   Creates and opens all fifos that are needed by the AFD_MON to
 **   communicate with afdm, AFD, etc.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcpy(), strcat(), strerror(),         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <errno.h>
#include "mondefs.h"

extern int  mon_cmd_fd,
            mon_resp_fd,
            probe_only_fd,
            sys_log_fd;
extern char mon_cmd_fifo[],
            probe_only_fifo[],
            *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fifos_mon() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
init_fifos_mon(void)
{
   char        mon_log_fifo[MAX_PATH_LENGTH],
               mon_resp_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Initialise fifo names */
   (void)strcpy(mon_resp_fifo, p_work_dir);
   (void)strcat(mon_resp_fifo, FIFO_DIR);
   (void)strcpy(mon_cmd_fifo, mon_resp_fifo);
   (void)strcat(mon_cmd_fifo, MON_CMD_FIFO);
   (void)strcpy(mon_log_fifo, mon_resp_fifo);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);
   (void)strcpy(probe_only_fifo, mon_resp_fifo);
   (void)strcat(probe_only_fifo, MON_PROBE_ONLY_FIFO);
   (void)strcat(mon_resp_fifo, MON_RESP_FIFO);

   /* If the process AFD has not yet created these fifos */
   /* create them now.                                   */
   if ((stat(mon_cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(mon_cmd_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   mon_cmd_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(mon_resp_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(mon_resp_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   mon_resp_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(probe_only_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(probe_only_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   probe_only_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(mon_log_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(mon_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   mon_log_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }

   /* Open fifo to AFD to receive commands */
   if ((mon_cmd_fd = coe_open(mon_cmd_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                mon_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* Open fifo to AFD to acknowledge the command */
   if ((mon_resp_fd = coe_open(mon_resp_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                mon_resp_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if ((probe_only_fd = coe_open(probe_only_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                probe_only_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}
