/*
 *  update_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   uhc - update HOST_CONFIG
 **   udc - update DIR_CONFIG
 **
 ** SYNOPSIS
 **   uhc [-w working directory]
 **   udc [-w working directory]
 **
 ** DESCRIPTION
 **   Program to update the DIR_CONFIG or HOST_CONFIG after they have
 **   been changed. Only possible if AMG is up running.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcmp(), strerror()         */
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "version.h"

/* Global variables */
int               sys_log_fd = STDERR_FILENO;
char              *p_work_dir;
struct afd_status *p_afd_status;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ update_db() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         db_update_fd;
   char        db_update_fifo[MAX_PATH_LENGTH],
               sys_log_fifo[MAX_PATH_LENGTH],
               user[MAX_FULL_USER_ID_LENGTH],
               work_dir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   set_afd_euid(work_dir);

   /* Attach to the AFD Status Area */
   if (attach_afd_status() < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to map to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (p_afd_status->amg != 1)
   {
      (void)fprintf(stderr,
                    "Database can only be updated if AMG is running.\n");
      exit(INCORRECT);
   }

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   (void)sprintf(sys_log_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open system log fifo */
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)sprintf(db_update_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, DB_UPDATE_FIFO);
   if ((db_update_fd = open(db_update_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)",
                    db_update_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   get_user(user, "");
   if (strcmp((argv[0] + strlen(argv[0]) - 3), "udc") == 0)
   {
      (void)rec(sys_log_fd, INFO_SIGN,
                "Rereading DIR_CONFIG initiated by %s [%s]\n",
                user, argv[0]);
      if (send_cmd(REREAD_DIR_CONFIG, db_update_fd) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Unable to send reread command to %s. (%s %d)\n",
                       AMG, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      (void)rec(sys_log_fd, INFO_SIGN,
                "Rereading HOST_CONFIG initiated by %s [%s]\n",
                user, argv[0]);
      if (send_cmd(REREAD_HOST_CONFIG, db_update_fd) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Unable to send reread command to %s. (%s %d)\n",
                       AMG, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if (close(db_update_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   exit(SUCCESS);
}
