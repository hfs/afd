/*
 *  init_sf.c - Part of AFD, an automatic file distribution program.
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
 **   init_sf - initialises all variables for all sf_xxx (sf - send file)
 **
 ** SYNOPSIS
 **   void init_sf(int  argc,
 **                char *argv[],
 **                char *file_path,
 **                int  *blocksize,
 **                int  *files_to_send,
 **                int  protocol)
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
#include <fcntl.h>
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
                                  rule_pos,
                                  fsa_id,
                                  fsa_fd,
                                  no_of_hosts,
                                  sys_log_fd,
                                  transfer_log_fd,
                                  trans_db_log_fd;
extern char                       host_deleted,
                                  *p_work_dir,
                                  tr_hostname[MAX_HOSTNAME_LENGTH + 1];
extern struct filetransfer_status *fsa;
extern struct job                 db;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* #define _DO_NOT_ARCHIVE 1 */

/* Global variables */
int                               no_of_rule_headers;


/*############################# init_sf() ###############################*/
void
init_sf(int  argc,
        char *argv[],
        char *file_path,
        int  *blocksize,
        int  *files_to_send,
        int  protocol)
{
   int           length,
                 status;
   off_t         lock_offset,
                 file_size_to_send = 0;
   char          sys_log_fifo[MAX_PATH_LENGTH],
                 transfer_log_fifo[MAX_PATH_LENGTH],
                 trans_db_log_fifo[MAX_PATH_LENGTH];
   struct job    *p_db;
   struct stat   stat_buf;

   /* Initialise variables */
   *files_to_send = 0;
   p_db = &db;
   memset(&db, 0, sizeof(struct job));
   if (protocol == FTP)
   {
      db.port = DEFAULT_FTP_PORT;
   }
   else if (protocol == SMTP)
        {
           db.port = DEFAULT_SMTP_PORT;
        }
#ifdef _WITH_WMO_SUPPORT
   else if (protocol == WMO)
        {
           db.port = DEFAULT_WMO_PORT;
        }
#endif
   db.transfer_mode = DEFAULT_TRANSFER_MODE;
   db.archive_time = DEFAULT_ARCHIVE_TIME;
#ifdef _AGE_LIMIT
   db.age_limit = DEFAULT_AGE_LIMIT;
#endif
#ifdef _OUTPUT_LOG
   db.output_log = YES;
#endif
   db.special_flag = 0;
   db.lock = DEFAULT_LOCK;
   db.error_file = NO;
   db.toggle_host = NO;
   db.msg_type = protocol;
   db.smtp_server[0] = '\0';
   db.special_ptr = NULL;
   db.user_id = -1;
   db.group_id = -1;
   (void)strcpy(db.lock_notation, DOT_NOTATION);
   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if (blocksize != NULL)
   {
      *blocksize = DEFAULT_TRANSFER_BLOCKSIZE;
   }

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif

   /*
    * In the next function it might already be necessary to use the
    * fifo for the sys_log. So let's for now assume that this is the
    * working directory for now. So we do not write our output to
    * standard out.
    */
   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      (void)make_fifo(sys_log_fifo);
   }
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "WARNING : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
   }

   if ((status = eval_input_sf(argc, argv, p_db)) < 0)
   {
      exit(-status);
   }
#ifdef _DO_NOT_ARCHIVE
   db.archive_time = 0;
#endif /* _DO_NOT_ARCHIVE */
   (void)strcpy(trans_db_log_fifo, p_work_dir);
   (void)strcat(trans_db_log_fifo, FIFO_DIR);
   (void)strcpy(transfer_log_fifo, trans_db_log_fifo);
   (void)strcat(transfer_log_fifo, TRANSFER_LOG_FIFO);
   (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);

   /* Open/create log fifos */
   if ((stat(transfer_log_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      (void)make_fifo(transfer_log_fifo);
   }
   if ((transfer_log_fd = open(transfer_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                transfer_log_fifo, strerror(errno), __FILE__, __LINE__);
   }
   if ((stat(trans_db_log_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      (void)make_fifo(trans_db_log_fifo);
   }
   if ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                trans_db_log_fifo, strerror(errno), __FILE__, __LINE__);
   }

   /* Initialise file source directory. */
   if (db.error_file == YES)
   {
      length = sprintf(file_path, "%s%s%s/%s/%s",
                       p_work_dir, AFD_FILE_DIR,
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
   if ((db.trans_rename_rule[0] != '\0') || (db.user_rename_rule[0] != '\0'))
   {
      if ((counter_fd = open_counter_file(COUNTER_FILE)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open counter file : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);

         reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                              NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                              FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                              FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                              FILE_NAME_IN_USE_VAR));
         exit(INCORRECT);                            
      }
   }

   /* Do we need to do remote renaming? */
   if ((db.trans_rename_rule[0] != '\0') || (db.user_rename_rule[0] != '\0'))
   {
      char rule_file[MAX_PATH_LENGTH];

      (void)strcpy(rule_file, p_work_dir);
      (void)strcat(rule_file, ETC_DIR);
      (void)strcat(rule_file, RENAME_RULE_FILE);
      get_rename_rules(rule_file);
      if (db.trans_rename_rule[0] != '\0')
      {
         if ((rule_pos = get_rule(db.trans_rename_rule, no_of_rule_headers)) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Could NOT find rule %s. Ignoring this option. (%s %d)\n",
                      db.trans_rename_rule, __FILE__, __LINE__);
            db.trans_rename_rule[0] = '\0';
         }
      }
      else
      {
         if ((rule_pos = get_rule(db.user_rename_rule, no_of_rule_headers)) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Could NOT find rule %s. Ignoring this option. (%s %d)\n",
                      db.user_rename_rule, __FILE__, __LINE__);
            db.user_rename_rule[0] = '\0';
         }
      }
   }

   (void)strcpy(tr_hostname, fsa[(int)db.position].host_dsp_name);
   if (db.toggle_host == YES)
   {
      if (fsa[(int)db.position].host_toggle == HOST_ONE)
      {
         tr_hostname[(int)fsa[(int)db.position].toggle_pos] = fsa[(int)db.position].host_toggle_str[HOST_TWO];
      }
      else
      {
         tr_hostname[(int)fsa[(int)db.position].toggle_pos] = fsa[(int)db.position].host_toggle_str[HOST_ONE];
      }
   }

   /*
    * Create directory in which we can find the files for this job.
    */
#ifdef _BURST_MODE
#ifdef _WITH_WMO_SUPPORT
   if ((protocol == FTP) || (protocol == WMO))
#else
   if (protocol == FTP)
#endif
   {
      lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].job_status[(int)db.job_no].job_id - (char *)fsa);
   }
#endif
   if ((*files_to_send = get_file_names(file_path, &file_size_to_send)) > 0)
   {
#ifdef _BURST_MODE
#ifdef _WITH_WMO_SUPPORT
      if ((protocol == FTP) || (protocol == WMO))
#else
      if (protocol == FTP)
#endif
      {
         if (fsa[(int)db.position].job_status[(int)db.job_no].burst_counter > 0)
         {
            fsa[(int)db.position].job_status[(int)db.job_no].burst_counter = 0;
         }
         unlock_region(fsa_fd, (char *)&fsa[(int)db.position].job_status[(int)db.job_no].job_id - (char *)fsa);
      }
#endif

      /* Do we want to display the status? */
      if (host_deleted == NO)
      {
         lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);

         if (check_fsa() == YES)
         {
            if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
            }
            else
            {
               lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
            }
         }
         if (host_deleted == NO)
         {
            fsa[(int)db.position].job_status[(int)db.job_no].file_size = file_size_to_send;
            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done = 0;
            fsa[(int)db.position].job_status[(int)db.job_no].connect_status = CONNECTING;
#if defined (_BURST_MODE) && !defined (_OUTPUT_LOG)
#ifdef _WITH_WMO_SUPPORT
            if ((protocol == FTP) || (protocol == WMO))
#else
            if (protocol == FTP)
#endif
            {
#endif
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
               fsa[(int)db.position].job_status[(int)db.job_no].job_id = db.job_id;
#endif
#if defined (_BURST_MODE) && !defined (_OUTPUT_LOG)
            }
            else
            {
               fsa[(int)db.position].job_status[(int)db.job_no].job_id = NO_ID;
            }
#endif
            unlock_region(fsa_fd, lock_offset);
         }

         if (blocksize != NULL)
         {
            *blocksize = fsa[(int)db.position].block_size;
         }
      }
   }
   else
   {
      /*
       * It could be that all files where to old to be send. If this is
       * the case, no need to go on.
       */
      /* Remove file directory. */
      if (rec_rmdir(file_path) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to remove directory %s (%s %d)\n",
                   file_path, __FILE__, __LINE__);
      }

      exit(NO_FILES_TO_SEND);
   }

   return;
}
