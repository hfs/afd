/*
 *  init_fifos_fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fifos_fd - creates and opens all fifos needed by FD
 **
 ** SYNOPSIS
 **   int init_fifos_fd(void)
 **
 ** DESCRIPTION
 **   Creates and opens all fifos that are needed by the FD to
 **   communicate with sf_xxx, AFD, etc.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.02.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcpy(), strcat(), strerror(),         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <errno.h>
#include "fddefs.h"

extern int  sys_log_fd,
            transfer_log_fd,
            fd_cmd_fd,
            fd_resp_fd,
            fd_wake_up_fd,
            msg_fifo_fd,
            read_fin_fd,
            retry_fd,
            delete_jobs_fd,
            delete_jobs_host_fd;
extern char *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fifos_fd() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
init_fifos_fd(void)
{
   char        transfer_log_fifo[MAX_PATH_LENGTH],
               fd_cmd_fifo[MAX_PATH_LENGTH],
               fd_resp_fifo[MAX_PATH_LENGTH],
               msg_fifo[MAX_PATH_LENGTH],
               fd_wake_up_fifo[MAX_PATH_LENGTH],
               retry_fifo[MAX_PATH_LENGTH],
               delete_jobs_fifo[MAX_PATH_LENGTH],
               delete_jobs_host_fifo[MAX_PATH_LENGTH],
               sf_fin_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Initialise fifo names */
   (void)strcpy(sf_fin_fifo, p_work_dir);
   (void)strcat(sf_fin_fifo, FIFO_DIR);
   (void)strcpy(transfer_log_fifo, sf_fin_fifo);
   (void)strcat(transfer_log_fifo, TRANSFER_LOG_FIFO);
   (void)strcpy(fd_cmd_fifo, sf_fin_fifo);
   (void)strcat(fd_cmd_fifo, FD_CMD_FIFO);
   (void)strcpy(fd_resp_fifo, sf_fin_fifo);
   (void)strcat(fd_resp_fifo, FD_RESP_FIFO);
   (void)strcpy(msg_fifo, sf_fin_fifo);
   (void)strcat(msg_fifo, MSG_FIFO);
   (void)strcpy(fd_wake_up_fifo, sf_fin_fifo);
   (void)strcat(fd_wake_up_fifo, FD_WAKE_UP_FIFO);
   (void)strcpy(retry_fifo, sf_fin_fifo);
   (void)strcat(retry_fifo, RETRY_FD_FIFO);
   (void)strcpy(delete_jobs_fifo, sf_fin_fifo);
   (void)strcat(delete_jobs_fifo, DELETE_JOBS_FIFO);
   (void)strcpy(delete_jobs_host_fifo, sf_fin_fifo);
   (void)strcat(delete_jobs_host_fifo, DELETE_JOBS_HOST_FIFO);
   (void)strcat(sf_fin_fifo, SF_FIN_FIFO);

   /* If the process AFD has not yet created these fifos */
   /* create them now.                                   */
   if ((stat(fd_cmd_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(fd_cmd_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   fd_cmd_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(fd_resp_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(fd_resp_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   fd_resp_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(transfer_log_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(transfer_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   transfer_log_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(sf_fin_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sf_fin_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   sf_fin_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(msg_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(msg_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   msg_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(fd_wake_up_fifo, &stat_buf) == -1) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(fd_wake_up_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   fd_wake_up_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(retry_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(retry_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   retry_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(delete_jobs_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(delete_jobs_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   delete_jobs_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if ((stat(delete_jobs_host_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(delete_jobs_host_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   delete_jobs_host_fifo, __FILE__, __LINE__);
         return(INCORRECT);
      }
   }

   /* Open fifo to FSA to acknowledge the command */
   if ((transfer_log_fd = coe_open(transfer_log_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                transfer_log_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* Open fifo to AFD to receive commands */
   if ((fd_cmd_fd = coe_open(fd_cmd_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* Open fifo to FSA to acknowledge the command */
   if ((fd_resp_fd = coe_open(fd_resp_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                fd_resp_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   /* Open fifo to receive message when a sf_xxx process is complete */
   if ((read_fin_fd = coe_open(sf_fin_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sf_fin_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if ((msg_fifo_fd = coe_open(msg_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                msg_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if ((fd_wake_up_fd = coe_open(fd_wake_up_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                fd_wake_up_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if ((retry_fd = coe_open(retry_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                retry_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if ((delete_jobs_fd = coe_open(delete_jobs_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                delete_jobs_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if ((delete_jobs_host_fd = coe_open(delete_jobs_host_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                delete_jobs_host_fifo, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}
