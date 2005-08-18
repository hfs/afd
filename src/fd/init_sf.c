/*
 *  init_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_sf - initialises all variables for all sf_xxx (sf - send file)
 **
 ** SYNOPSIS
 **   int init_sf(int argc, char *argv[], char *file_path, int protocol)
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
 **   14.12.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* calloc() in RT_ARRAY()         */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                     /* open()                         */
#include <unistd.h>                    /* getpid()                       */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#ifdef _WITH_SCP_SUPPORT
#include "scpdefs.h"
#endif
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables */
extern int                        exitflag,
                                  trans_rule_pos,
                                  user_rule_pos,
                                  fsa_fd,
                                  no_of_hosts,
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       *p_work_dir,
                                  tr_hostname[];
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Global variables */
int                               no_of_rule_headers;


/*############################# init_sf() ###############################*/
int
init_sf(int argc, char *argv[], char *file_path, int protocol)
{
   int        files_to_send = 0,
              status;
   off_t      file_size_to_send = 0;
   char       gbuf[MAX_PATH_LENGTH];      /* Generic buffer.         */
   struct job *p_db;

   /* Initialise variables */
   p_db = &db;
   (void)memset(&db, 0, sizeof(struct job));
   if (protocol & FTP_FLAG)
   {
      db.port = DEFAULT_FTP_PORT;
   }
   else if (protocol & SMTP_FLAG)
        {
           db.port = DEFAULT_SMTP_PORT;
           db.reply_to = NULL;
           db.from = NULL;
           db.charset = NULL;
        }
#ifdef _WITH_SCP_SUPPORT
   else if (protocol & SCP_FLAG)
        {
           db.port = DEFAULT_SSH_PORT;
           db.chmod = FILE_MODE;
        }
#endif
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
   db.resend = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
   db.subject = NULL;
#ifdef _WITH_TRANS_EXEC
   db.trans_exec_cmd = NULL;
   db.trans_exec_timeout = DEFAULT_EXEC_TIMEOUT;
#endif
   db.special_flag = 0;
   db.mode_flag = 0;
   db.archive_time = DEFAULT_ARCHIVE_TIME;
   db.age_limit = DEFAULT_AGE_LIMIT;
#ifdef _OUTPUT_LOG
   db.output_log = YES;
#endif
   db.lock = DEFAULT_LOCK;
   db.smtp_server[0] = '\0';
   db.chmod_str[0] = '\0';
   db.trans_rename_rule[0] = '\0';
   db.user_rename_rule[0] = '\0';
   db.lock_file_name = NULL;
   db.rename_file_busy = '\0';
   db.no_of_restart_files = 0;
   db.restart_file = NULL;
   db.user_id = -1;
   db.group_id = -1;
#ifdef WITH_DUP_CHECK
   db.dup_check_flag = 0;
   db.dup_check_timeout = 0L;
#endif
#ifdef WITH_SSL
   db.auth = NO;
#endif
   (void)strcpy(db.lock_notation, DOT_NOTATION);
#ifdef _DELETE_LOG
   dl.fd = -1;
#endif

   if ((status = eval_input_sf(argc, argv, p_db)) < 0)
   {
      exit(-status);
   }
   db.my_pid = getpid();
   if (db.mode_flag == 0)
   {
      if (fsa->protocol_options & FTP_PASSIVE_MODE)
      {
         db.mode_flag = PASSIVE_MODE;
      }
      else
      {
         db.mode_flag = ACTIVE_MODE;
      }
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
                       "Could not open fifo %s : %s",
                       TRANSFER_LOG_FIFO, strerror(errno));
         }
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not open fifo %s : %s",
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

   /*
    * Open the AFD counter file. This is needed when trans_rename is
    * used or user renaming (SMTP).
    */
   if ((db.trans_rename_rule[0] != '\0') ||
       (db.user_rename_rule[0] != '\0'))
   {
      (void)strcpy(gbuf, p_work_dir);
      (void)strcat(gbuf, ETC_DIR);
      (void)strcat(gbuf, RENAME_RULE_FILE);
      get_rename_rules(gbuf, NO);
      if (db.trans_rename_rule[0] != '\0')
      {
         if ((trans_rule_pos = get_rule(db.trans_rename_rule,
                                        no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                      "Could NOT find rule %s. Ignoring this option.",
                      db.trans_rename_rule);
            db.trans_rename_rule[0] = '\0';
         }
      }
      if (db.user_rename_rule[0] != '\0')
      {
         if ((user_rule_pos = get_rule(db.user_rename_rule,
                                       no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring this option.",
                       db.user_rename_rule);
            db.user_rename_rule[0] = '\0';
         }
      }
   }

   db.lock_offset = AFD_WORD_OFFSET +
                    (db.fsa_pos * sizeof(struct filetransfer_status));
   if ((files_to_send = get_file_names(file_path, &file_size_to_send)) < 1)
   {
      int ret;

      /*
       * It could be that all files where to old to be send. If this is
       * the case, no need to go on.
       */
      if ((ret = remove_dir(file_path)) < 0)
      {
         if (ret == FILE_IS_DIR)
         {
            if (rec_rmdir(file_path) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to rec_rmdir() %s", file_path);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Removed directory/directories in %s", file_path);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to remove directory %s", file_path);
         }
      }
      exitflag = 0;
      exit(NO_FILES_TO_SEND);
   }

   /* Do we want to display the status? */
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
#ifdef LOCK_DEBUG
      rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      rlock_region(fsa_fd, db.lock_offset);
#endif
      fsa->job_status[(int)db.job_no].file_size = file_size_to_send;
      fsa->job_status[(int)db.job_no].file_size_done = 0;
      fsa->job_status[(int)db.job_no].connect_status = CONNECTING;
      fsa->job_status[(int)db.job_no].job_id = db.job_id;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset);
#endif

      /* Set the timeout value. */
      transfer_timeout = fsa->transfer_timeout;
   }

   return(files_to_send);
}
