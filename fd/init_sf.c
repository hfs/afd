/*
 *  init_sf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#include <unistd.h>                    /* remove(), getpid()             */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables */
extern int                        counter_fd,
                                  trans_rule_pos,
                                  user_rule_pos,
                                  fsa_fd,
                                  no_of_hosts,
                                  sys_log_fd,
                                  transfer_log_fd,
                                  trans_db_log_fd;
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
              length,
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
   db.user_id = -1;
   db.group_id = -1;
   (void)strcpy(db.lock_notation, DOT_NOTATION);
#ifdef _DELETE_LOG
   dl.fd = -1;
#endif

   /*
    * In the next function it might already be necessary to use the
    * fifo for the sys_log. So let's for now assume that this is the
    * working directory for now. So we do not write our output to
    * standard out.
    */
   (void)strcpy(gbuf, p_work_dir);
   (void)strcat(gbuf, FIFO_DIR);
   (void)strcat(gbuf, SYSTEM_LOG_FIFO);
   if ((sys_log_fd = open(gbuf, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         if ((make_fifo(gbuf) == SUCCESS) &&
             ((sys_log_fd = open(gbuf, O_RDWR)) == -1))
         {
            (void)fprintf(stderr,
                          "WARNING : Could not open fifo %s : %s (%s %d)\n",
                          SYSTEM_LOG_FIFO, strerror(errno),
                          __FILE__, __LINE__);
         }
      }
      else
      {
         (void)fprintf(stderr,
                       "WARNING : Could not open fifo %s : %s (%s %d)\n",
                       SYSTEM_LOG_FIFO, strerror(errno), __FILE__, __LINE__);
      }
   }

   if ((status = eval_input_sf(argc, argv, p_db)) < 0)
   {
      exit(-status);
   }
   if (*(unsigned char *)((char *)fsa - 3) & DISABLE_ARCHIVE)
   {
      db.archive_time = 0;
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
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not open fifo %s : %s (%s %d)\n",
                      TRANSFER_LOG_FIFO, strerror(errno), __FILE__, __LINE__);
         }
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not open fifo %s : %s (%s %d)\n",
                   TRANSFER_LOG_FIFO, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (fsa[db.fsa_pos].debug == YES)
   {
      (void)strcpy(gbuf, p_work_dir);
      (void)strcat(gbuf, FIFO_DIR);
      (void)strcat(gbuf, TRANS_DEBUG_LOG_FIFO);
      if ((trans_db_log_fd = open(gbuf, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(gbuf) == SUCCESS) &&
                ((trans_db_log_fd = open(gbuf, O_RDWR)) == -1))
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not open fifo %s : %s (%s %d)\n",
                         TRANS_DEBUG_LOG_FIFO, strerror(errno),
                         __FILE__, __LINE__);
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not open fifo %s : %s (%s %d)\n",
                      TRANS_DEBUG_LOG_FIFO, strerror(errno),
                      __FILE__, __LINE__);
         }
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

   /* Initialise file source directory. */
   if (db.error_file == YES)
   {
      length = sprintf(file_path, "%s%s%s/%s/%s", p_work_dir, AFD_FILE_DIR,
                       ERROR_DIR, db.host_alias, db.msg_name) + 1;
   }
   else
   {
      length = sprintf(file_path, "%s%s/%s",
                       p_work_dir, AFD_FILE_DIR, db.msg_name) + 1;
   }
   RT_ARRAY(p_db->filename, 1, length, char);
   (void)strcpy(db.filename[0], file_path);

   /*
    * Open the AFD counter file. This is needed when trans_rename is
    * used or user renaming (SMTP).
    */
   if ((db.trans_rename_rule[0] != '\0') ||
       (db.user_rename_rule[0] != '\0'))
   {
      if ((counter_fd = open_counter_file(COUNTER_FILE)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open counter file : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);                            
      }

      (void)strcpy(gbuf, p_work_dir);
      (void)strcat(gbuf, ETC_DIR);
      (void)strcat(gbuf, RENAME_RULE_FILE);
      get_rename_rules(gbuf);
      if (db.trans_rename_rule[0] != '\0')
      {
         if ((trans_rule_pos = get_rule(db.trans_rename_rule,
                                        no_of_rule_headers)) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Could NOT find rule %s. Ignoring this option. (%s %d)\n",
                      db.trans_rename_rule, __FILE__, __LINE__);
            db.trans_rename_rule[0] = '\0';
         }
      }
      if (db.user_rename_rule[0] != '\0')
      {
         if ((user_rule_pos = get_rule(db.user_rename_rule,
                                       no_of_rule_headers)) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Could NOT find rule %s. Ignoring this option. (%s %d)\n",
                      db.user_rename_rule, __FILE__, __LINE__);
            db.user_rename_rule[0] = '\0';
         }
      }
   }

#ifdef _BURST_MODE
#ifdef _WITH_WMO_SUPPORT
   if ((protocol & FTP_FLAG) || (protocol & WMO_FLAG))
#else
   if (protocol & FTP_FLAG)
#endif /* _WITH_WMO_SUPPORT */
   {
      lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
   }
#endif /* _BURST_MODE */
   if ((files_to_send = get_file_names(file_path, &file_size_to_send)) > 0)
   {
#ifdef _BURST_MODE
#ifdef _WITH_WMO_SUPPORT
      if ((protocol & FTP_FLAG) || (protocol & WMO_FLAG))
#else
      if (protocol & FTP_FLAG)
#endif
      {
         if (fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter > 0)
         {
            fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter = 0;
         }
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
      }
#endif
   }
   else
   {
      /*
       * It could be that all files where to old to be send. If this is
       * the case, no need to go on.
       */
      /* Remove file directory. */
      if (remove_dir(file_path) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to remove directory %s (%s %d)\n",
                   file_path, __FILE__, __LINE__);
      }
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
#ifdef _WITH_WMO_SUPPORT
         if ((protocol & FTP_FLAG) || (protocol & WMO_FLAG))
#else
         if (protocol & FTP_FLAG)
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
