/*
 *  init_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
extern int                        counter_fd,
                                  exitflag,
                                  trans_rule_pos,
                                  user_rule_pos,
                                  fsa_fd,
                                  no_of_hosts,
                                  transfer_log_fd;
extern long                       transfer_timeout;
extern char                       host_deleted,
                                  *p_work_dir,
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
   off_t      lock_offset,
              file_size_to_send = 0;
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

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      exit(INCORRECT);
   }
   db.fsa_pos = -1; /* For exit handler. */
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.toggle_host = NO;
   db.protocol = protocol;
   db.special_ptr = NULL;
   db.subject = NULL;
#ifdef _WITH_TRANS_EXEC
   db.trans_exec_cmd = NULL;
#endif
   db.special_flag = 0;
   db.mode_flag = 0;
   db.archive_time = DEFAULT_ARCHIVE_TIME;
#ifdef _AGE_LIMIT
   db.age_limit = DEFAULT_AGE_LIMIT;
#endif
#ifdef _OUTPUT_LOG
   db.output_log = YES;
#endif
   db.lock = DEFAULT_LOCK;
   db.error_file = NO;
   db.smtp_server[0] = '\0';
   db.chmod_str[0] = '\0';
   db.trans_rename_rule[0] = '\0';
   db.user_rename_rule[0] = '\0';
   db.rename_file_busy = '\0';
   db.no_of_restart_files = 0;
   db.restart_file = NULL;
   db.user_id = -1;
   db.group_id = -1;
   (void)strcpy(db.lock_notation, DOT_NOTATION);
#ifdef _DELETE_LOG
   dl.fd = -1;
#endif

   if ((status = eval_input_sf(argc, argv, p_db)) < 0)
   {
      exit(-status);
   }
   if (*(unsigned char *)((char *)fsa - 3) & DISABLE_ARCHIVE)
   {
      db.archive_time = 0;
   }
   db.my_pid = getpid();
   if (db.mode_flag == 0)
   {
      if (fsa[db.fsa_pos].protocol & FTP_PASSIVE_MODE)
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

   /*
    * Open the AFD counter file. This is needed when trans_rename is
    * used or user renaming (SMTP).
    */
   if ((db.trans_rename_rule[0] != '\0') ||
       (db.user_rename_rule[0] != '\0'))
   {
      if ((counter_fd = open_counter_file(COUNTER_FILE)) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not open counter file : %s", strerror(errno));
         exit(INCORRECT);                            
      }

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

#ifdef _BURST_MODE
   if ((protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
       || (protocol & WMO_FLAG)
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_SCP_SUPPORT
       || (protocol & SCP_FLAG))
#else
       )
#endif
   {
      lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
   }
#endif /* _BURST_MODE */
   if ((files_to_send = get_file_names(file_path, &file_size_to_send)) > 0)
   {
#ifdef _BURST_MODE
      if ((protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
          || (protocol & WMO_FLAG)
#endif
#ifdef _WITH_SCP_SUPPORT
          || (protocol & SCP_FLAG))
#else
          )
#endif
      {
         if (fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter > 0)
         {
            fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter = 0;
         }
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
      }
#endif /* _BURST_MODE */
   }
   else
   {
      int ret;

      /*
       * It could be that all files where to old to be send. If this is
       * the case, no need to go on.
       */
      /* Remove file directory. */
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
   if (host_deleted == NO)
   {
      lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
      rlock_region(fsa_fd, lock_offset);

      if (check_fsa() == YES)
      {
         if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                              no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
         else
         {
            lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);
         }
      }
      if (host_deleted == NO)
      {
         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size = file_size_to_send;
         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done = 0;
         fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = CONNECTING;
#if defined (_BURST_MODE) && !defined (_OUTPUT_LOG)
         if ((protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
             || (protocol & WMO_FLAG)
#endif
#ifdef _WITH_SCP_SUPPORT
             || (protocol & SCP_FLAG))
#else
             )
#endif
         {
#endif
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
            fsa[db.fsa_pos].job_status[(int)db.job_no].job_id = db.job_id;
#endif
#if defined (_BURST_MODE) && !defined (_OUTPUT_LOG)
         }
         else
         {
            fsa[db.fsa_pos].job_status[(int)db.job_no].job_id = NO_ID;
         }
#endif
         unlock_region(fsa_fd, lock_offset);

         /* Set the timeout value. */
         transfer_timeout = fsa[db.fsa_pos].transfer_timeout;
      }
   }

   return(files_to_send);
}
