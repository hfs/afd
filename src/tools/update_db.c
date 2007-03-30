/*
 *  update_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Deutscher Wetterdienst (DWD),
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
const char        *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ update_db() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  db_update_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int  db_update_readfd;
#endif
   char db_update_fifo[MAX_PATH_LENGTH],
        user[MAX_FULL_USER_ID_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   /* Attach to the AFD Status Area */
   if (attach_afd_status(NULL) < 0)
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

   (void)sprintf(db_update_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, DB_UPDATE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(db_update_fifo, &db_update_readfd, &db_update_fd) == -1)
#else
   if ((db_update_fd = open(db_update_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)",
                    db_update_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   get_user(user, "");
   if (strcmp((argv[0] + strlen(argv[0]) - 3), "udc") == 0)
   {
      system_log(CONFIG_SIGN, NULL, 0,
                 "Rereading DIR_CONFIG initiated by %s [%s]", user, argv[0]);
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
      system_log(CONFIG_SIGN, NULL, 0,
                 "Rereading HOST_CONFIG initiated by %s [%s]",
                 user, argv[0]);
      if (send_cmd(REREAD_HOST_CONFIG, db_update_fd) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Unable to send reread command to %s. (%s %d)\n",
                       AMG, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (close(db_update_readfd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
#endif
   if (close(db_update_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   exit(SUCCESS);
}
