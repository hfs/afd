/*
 *  init_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000, 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_gf - initialises all variables for all gf_xxx (gf - get file)
 **
 ** SYNOPSIS
 **   void init_gf(int argc, char *argv[], int protocol)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. It will exit with INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* calloc() in RT_ARRAY()         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                    /* getpid()                       */
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"
#include "smtpdefs.h"
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables */
extern int                        no_of_hosts,
                                  no_of_dirs,
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       host_deleted,
                                  *p_work_dir,
                                  tr_hostname[];
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct job                 db;


/*############################# init_gf() ##############################*/
void
init_gf(int argc, char *argv[], int protocol)
{
   int        status;
   char       gbuf[MAX_PATH_LENGTH];      /* Generic buffer.         */
   struct job *p_db;

   /* Initialise variables */
   p_db = &db;
   memset(&db, 0, sizeof(struct job));
   if (protocol & FTP_FLAG)
   {
      db.port = DEFAULT_FTP_PORT;
   }
   else if (protocol & SMTP_FLAG)
        {
           db.port = DEFAULT_SMTP_PORT;
        }
#ifdef _WITH_WMO_SUPPORT
   else if (protocol & WMO_FLAG)
        {
           db.port = DEFAULT_WMO_PORT;
        }
#endif

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      exit(INCORRECT);
   }
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.toggle_host = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
   db.mode_flag = ACTIVE_MODE;  /* Lets first default to active mode. */

   if ((status = eval_input_gf(argc, argv, p_db)) < 0)
   {
      exit(-status);
   }
   if (fra_attach() < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }
   if ((db.fra_pos = get_dir_position(fra, db.dir_alias, no_of_dirs)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to locate dir_alias <%s> in the FRA.", db.dir_alias);
      exit(INCORRECT);          
   }
   if (fsa_attach() < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }
   if (eval_recipient(fra[db.fra_pos].url, &db, NULL) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to evaluate recipient for directory alias <%s>.",
                 fra[db.fra_pos].dir_alias);
      exit(INCORRECT);
   }

   /* Open/create log fifos */
   (void)strcpy(gbuf, p_work_dir);
   (void)strcat(gbuf, FIFO_DIR);
   (void)strcat(gbuf, TRANSFER_LOG_FIFO);
   if ((transfer_log_fd = open(gbuf, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         if ((make_fifo(gbuf) == SUCCESS) &&
             ((transfer_log_fd = open(gbuf, O_RDWR)) == -1))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo <%s> : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open fifo <%s> : %s",
                    TRANSFER_LOG_FIFO, strerror(errno));
      }
   }

   (void)strcpy(tr_hostname, fsa[db.fsa_pos].host_dsp_name);
   if (db.toggle_host == YES)
   {
      if (fsa[db.fsa_pos].host_toggle == HOST_ONE)
      {
         tr_hostname[(int)fsa[db.fsa_pos].toggle_pos] = fsa[db.fsa_pos].host_toggle_str[HOST_TWO];
      }
      else
      {
         tr_hostname[(int)fsa[db.fsa_pos].toggle_pos] = fsa[db.fsa_pos].host_toggle_str[HOST_ONE];
      }
   }

   /* Set the transfer timeout value. */
   transfer_timeout = fsa[db.fsa_pos].transfer_timeout;

   return;
}
