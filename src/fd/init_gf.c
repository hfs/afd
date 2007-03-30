/*
 *  init_gf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include "httpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables */
extern int                        fsa_fd,
                                  no_of_hosts,
                                  no_of_dirs,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  transfer_log_readfd,
#endif
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
   time_t     next_check_time;
   char       gbuf[MAX_PATH_LENGTH];      /* Generic buffer.         */
   struct job *p_db;

   /* Initialise variables */
   p_db = &db;
   (void)memset(&db, 0, sizeof(struct job));
   if (protocol & FTP_FLAG)
   {
      db.port = DEFAULT_FTP_PORT;
   }
   else if (protocol & HTTP_FLAG)
        {
           db.port = DEFAULT_HTTP_PORT;
        }
   else if (protocol & SFTP_FLAG)
        {
           db.port = DEFAULT_SSH_PORT;
        }
#ifdef _WITH_WMO_SUPPORT
   else if (protocol & WMO_FLAG)
        {
           db.port = DEFAULT_WMO_PORT;
        }
#endif
        else
        {
           db.port = -1;
        }

   db.fsa_pos = INCORRECT;
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.toggle_host = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
#ifdef WITH_SSH_FINGERPRINT
   db.ssh_fingerprint[0] = '\0';
   db.key_type = 0;
#endif
#ifdef WITH_SSL
   db.auth = NO;
#endif
   db.ssh_protocol = 0;
   db.sndbuf_size = 0;
   db.rcvbuf_size = 0;

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
   if (fra[db.fra_pos].keep_connected > 0)
   {
      db.keep_connected = fra[db.fra_pos].keep_connected;
   }
   else if (fsa->keep_connected > 0)
        {
           db.keep_connected = fsa->keep_connected;
        }
        else
        {
           db.keep_connected = 0;
        }
#ifdef WITH_SSL
   if ((fsa->protocol & HTTP_FLAG) && (fsa->protocol & SSL_FLAG))
   {
      db.port = DEFAULT_HTTPS_PORT;
   }
#endif
   if (fsa->protocol_options & FTP_IGNORE_BIN)
   {
      db.transfer_mode = 'N';
   }
   if (db.sndbuf_size <= 0)
   {
      db.sndbuf_size = fsa->socksnd_bufsize;
   }
   if (db.rcvbuf_size <= 0)
   {
      db.rcvbuf_size = fsa->sockrcv_bufsize;
   }

   if ((fsa->error_counter > 0) && (fra[db.fra_pos].time_option == YES))
   {
      next_check_time = fra[db.fra_pos].next_check_time;
   }
   else
   {
      next_check_time = 0;
   }
   if (eval_recipient(fra[db.fra_pos].url, &db, NULL, next_check_time) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to evaluate recipient for directory alias <%s>.",
                 fra[db.fra_pos].dir_alias);
      exit(INCORRECT);
   }
   if (protocol & FTP_FLAG)
   {
      if (fsa->protocol_options & FTP_PASSIVE_MODE)
      {
         db.mode_flag = PASSIVE_MODE;
         if (fsa->protocol_options & FTP_EXTENDED_MODE)
         {
            (void)strcpy(db.mode_str, "extended passive");
         }
         else
         {
            if (fsa->protocol_options & FTP_ALLOW_DATA_REDIRECT)
            {
               (void)strcpy(db.mode_str, "passive (with redirect)");
            }
            else
            {
               (void)strcpy(db.mode_str, "passive");
            }
         }
      }
      else
      {
         db.mode_flag = ACTIVE_MODE;
         if (fsa->protocol_options & FTP_EXTENDED_MODE)
         {
            (void)strcpy(db.mode_str, "extended active");
         }
         else
         {
            (void)strcpy(db.mode_str, "active");
         }
      }
      if (fsa->protocol_options & FTP_EXTENDED_MODE)
      {
         db.mode_flag |= EXTENDED_MODE;
      }
   }
   else
   {
      db.mode_str[0] = '\0';
   }

   /* Open/create log fifos */
   (void)strcpy(gbuf, p_work_dir);
   (void)strcat(gbuf, FIFO_DIR);
   (void)strcat(gbuf, TRANSFER_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(gbuf, &transfer_log_readfd, &transfer_log_fd) == -1)
#else
   if ((transfer_log_fd = open(gbuf, O_RDWR)) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         if ((make_fifo(gbuf) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
             (open_fifo_rw(gbuf, &transfer_log_readfd, &transfer_log_fd) == -1))
#else
             ((transfer_log_fd = open(gbuf, O_RDWR)) == -1))
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not open fifo `%s' : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open fifo `%s' : %s",
                    TRANSFER_LOG_FIFO, strerror(errno));
      }
   }

   (void)strcpy(tr_hostname, fsa->host_dsp_name);
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         tr_hostname[(int)fsa->toggle_pos] = fsa->host_toggle_str[HOST_TWO];
      }
      else
      {
         tr_hostname[(int)fsa->toggle_pos] = fsa->host_toggle_str[HOST_ONE];
      }
   }

   /* Set the transfer timeout value. */
   transfer_timeout = fsa->transfer_timeout;

   db.lock_offset = AFD_WORD_OFFSET +
                    (db.fsa_pos * sizeof(struct filetransfer_status));
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
#ifdef LOCK_DEBUG
      rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      rlock_region(fsa_fd, db.lock_offset);
#endif
      fsa->job_status[(int)db.job_no].file_size = 0;
      fsa->job_status[(int)db.job_no].file_size_done = 0;
      fsa->job_status[(int)db.job_no].connect_status = CONNECTING;
      fsa->job_status[(int)db.job_no].job_id = db.job_id;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset);
#endif
   }

   return;
}
