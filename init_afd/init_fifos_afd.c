/*
 *  init_fifos_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fifos_afd -
 **
 ** SYNOPSIS
 **   void init_fifos_afd(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.02.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>           /* strcpy(), strcat(), strerror(),         */
#include <stdlib.h>           /* exit()                                  */
#include <unistd.h>           /* unlink()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <errno.h>

extern int  afd_cmd_fd,
            afd_resp_fd,
            amg_cmd_fd,
            fd_cmd_fd,
            probe_only_fd;
extern char *p_work_dir,
            afd_cmd_fifo[MAX_PATH_LENGTH],
            amg_cmd_fifo[MAX_PATH_LENGTH],
            fd_cmd_fifo[MAX_PATH_LENGTH],
            probe_only_fifo[MAX_PATH_LENGTH];


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fifos_afd() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_fifos_afd(void)
{
   char        transfer_log_fifo[MAX_PATH_LENGTH],
               trans_db_log_fifo[MAX_PATH_LENGTH],
               afd_resp_fifo[MAX_PATH_LENGTH],
               fd_resp_fifo[MAX_PATH_LENGTH],
               ip_fin_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Initialise fifo names */
   (void)strcpy(transfer_log_fifo, p_work_dir);
   (void)strcat(transfer_log_fifo, FIFO_DIR);
   (void)strcpy(trans_db_log_fifo, transfer_log_fifo);
   (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);
   (void)strcpy(afd_resp_fifo, transfer_log_fifo);
   (void)strcat(afd_resp_fifo, AFD_RESP_FIFO);
   (void)strcpy(amg_cmd_fifo, transfer_log_fifo);
   (void)strcat(amg_cmd_fifo, AMG_CMD_FIFO);
   (void)strcpy(fd_cmd_fifo, transfer_log_fifo);
   (void)strcat(fd_cmd_fifo, FD_CMD_FIFO);
   (void)strcpy(fd_resp_fifo, transfer_log_fifo);
   (void)strcat(fd_resp_fifo, FD_RESP_FIFO);
   (void)strcpy(ip_fin_fifo, transfer_log_fifo);
   (void)strcat(ip_fin_fifo, IP_FIN_FIFO);

   (void)strcat(transfer_log_fifo, TRANSFER_LOG_FIFO);

   /* First remove any stale fifos. Maybe they still have       */
   /* some garbage. So lets remove it before it can do any harm. */
   (void)unlink(transfer_log_fifo);
   (void)unlink(trans_db_log_fifo);
   (void)unlink(afd_cmd_fifo);
   (void)unlink(afd_resp_fifo);
   (void)unlink(amg_cmd_fifo);
   (void)unlink(fd_cmd_fifo);
   (void)unlink(fd_resp_fifo);
   (void)unlink(ip_fin_fifo);

   /* OK. Now lets make all fifos */
   if (make_fifo(transfer_log_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    transfer_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(trans_db_log_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    trans_db_log_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(afd_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    afd_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(afd_resp_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    afd_resp_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(amg_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    amg_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(fd_cmd_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    fd_cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(fd_resp_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    fd_resp_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (make_fifo(ip_fin_fifo) < 0)
   {
      (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                    ip_fin_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((stat(probe_only_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(probe_only_fifo) < 0)
      {
         (void)fprintf(stderr, "Could not create fifo %s. (%s %d)\n",
                       probe_only_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Now lets open all fifos needed by the AFD */
   if ((afd_cmd_fd = coe_open(afd_cmd_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                    afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((afd_resp_fd = coe_open(afd_resp_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                    afd_resp_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((amg_cmd_fd = coe_open(amg_cmd_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                    amg_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((fd_cmd_fd = coe_open(fd_cmd_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                    fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((probe_only_fd = coe_open(probe_only_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                    probe_only_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
