/*
 *  init_sf_burst2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001, 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_sf_burst2 - initialises all variables for sf_xxx for a burst
 **
 ** SYNOPSIS
 **   int init_sf_burst2(struct job   *p_new_db,
 **                      char         *file_path,
 **                      unsigned int *values_changed)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   If successfull it will return the number of files to be send. It
 **   will exit with INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.05.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                     /* sprintf()                      */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* malloc()                       */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int                        counter_fd,
                                  trans_rule_pos,
                                  user_rule_pos,
                                  fsa_fd,
                                  no_of_hosts,
                                  no_of_rule_headers,
                                  sys_log_fd,
                                  transfer_log_fd,
                                  trans_db_log_fd;
extern long                       transfer_timeout;
extern char                       host_deleted,
                                  *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*########################## init_sf_burst2() ###########################*/
int
init_sf_burst2(struct job   *p_new_db,
               char         *file_path,
               unsigned int *values_changed)
{
   int   files_to_send = 0;
#ifdef _WITH_BURST_2
   off_t file_size_to_send = 0;

   /* Initialise variables */
   if (p_new_db != NULL)
   {
      db.archive_time = p_new_db->archive_time;
      db.port         = p_new_db->port;
#ifdef _AGE_LIMIT
      db.age_limit    = p_new_db->age_limit;
#endif
      db.chmod        = p_new_db->chmod;
      db.chmod_str[0] = p_new_db->chmod_str[0];
      if (db.chmod_str[0] != '\0')
      {
         db.chmod_str[1] = p_new_db->chmod_str[1];
         db.chmod_str[2] = p_new_db->chmod_str[2];
         db.chmod_str[3] = p_new_db->chmod_str[3];
         db.chmod_str[4] = p_new_db->chmod_str[4];
      }
      db.user_id      = p_new_db->user_id;
      db.group_id     = p_new_db->group_id;
      if (values_changed != NULL)
      {
         *values_changed = 0;
         if (CHECK_STRCMP(db.user, p_new_db->user) != 0)
         {
            *values_changed |= USER_CHANGED;
         }
         if (CHECK_STRCMP(db.target_dir, p_new_db->target_dir) != 0)
         {
            *values_changed |= TARGET_DIR_CHANGED;
         }
         if (db.transfer_mode != p_new_db->transfer_mode)
         {
            *values_changed |= TYPE_CHANGED;
         }
      }
      (void)strcpy(db.user, p_new_db->user);
      (void)strcpy(db.password, p_new_db->password);
      (void)strcpy(db.target_dir, p_new_db->target_dir);
      if (p_new_db->smtp_server[0] == '\0')
      {
         db.smtp_server[0] = '\0';
      }
      else
      {
         (void)strcpy(db.smtp_server, p_new_db->smtp_server);
      }
      if (db.group_list != NULL)
      {
         FREE_RT_ARRAY(db.group_list);
      }
      db.group_list = p_new_db->group_list;
      db.no_listed = p_new_db->no_listed;
      if (db.no_of_restart_files > 0)
      {
         FREE_RT_ARRAY(db.restart_file);
         db.restart_file = NULL;
      }
      db.no_of_restart_files = p_new_db->no_of_restart_files;
      if (db.no_of_restart_files > 0)
      {
         db.restart_file = p_new_db->restart_file;
      }
      if (p_new_db->trans_rename_rule[0] == '\0')
      {
         db.trans_rename_rule[0] = '\0';
      }
      else
      {
         (void)strcpy(db.trans_rename_rule, p_new_db->trans_rename_rule);
      }
      if (p_new_db->user_rename_rule[0] == '\0')
      {
         db.user_rename_rule[0] = '\0';
      }
      else
      {
         (void)strcpy(db.user_rename_rule, p_new_db->user_rename_rule);
      }
      (void)strcpy(db.lock_notation, p_new_db->lock_notation);
      db.archive_dir[0]      = '\0';
      db.transfer_mode       = p_new_db->transfer_mode;
      db.lock                = p_new_db->lock;
      if (db.subject != NULL)
      {
         free(db.subject);
      }
      db.subject = p_new_db->subject;
#ifdef _WITH_TRANS_EXEC
      if (db.trans_exec_cmd != NULL)
      {
         free(db.trans_exec_cmd);
      }
      db.trans_exec_cmd = p_new_db->trans_exec_cmd;
#endif /* _WITH_TRANS_EXEC */
      if (db.special_ptr != NULL)
      {
         free(db.special_ptr);
      }
      db.special_ptr  = p_new_db->special_ptr;
      db.special_flag = p_new_db->special_flag;
#ifdef _OUTPUT_LOG
      db.output_log   = p_new_db->output_log;
#endif
      db.mode_flag    = p_new_db->mode_flag;
      free(p_new_db);
      p_new_db = NULL;
   }

   if (*(unsigned char *)((char *)fsa - 3) & DISABLE_ARCHIVE)
   {
      db.archive_time = 0;
   }

   /*
    * Open the AFD counter file. This is needed when trans_rename is
    * used or user renaming (SMTP).
    */
   if ((db.trans_rename_rule[0] != '\0') || (db.user_rename_rule[0] != '\0'))
   {
      char gbuf[MAX_PATH_LENGTH];

      if (counter_fd == -1)
      {
         if ((counter_fd = open_counter_file(COUNTER_FILE)) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not open counter file : %s", strerror(errno));
            exit(INCORRECT);                            
         }
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
   if ((db.protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
       || (db.protocol & WMO_FLAG)
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_SCP1_SUPPORT
       || (db.protocol & SCP1_FLAG))
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
      if ((db.protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
          || (db.protocol & WMO_FLAG)
#endif
#ifdef _WITH_SCP1_SUPPORT
          || (db.protocol & SCP1_FLAG))
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

      /* Do we want to display the status? */
      if (host_deleted == NO)
      {
         off_t lock_offset;

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
            if (db.protocol & FTP_FLAG)
            {
               fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = FTP_BURST2_TRANSFER_ACTIVE;
            }
            else if (db.protocol & LOC_FLAG)
                 {
                    fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = LOC_BURST_TRANSFER_ACTIVE;
                 }
            fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done + files_to_send;
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size = fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done + file_size_to_send;
#if defined (_BURST_MODE) && !defined (_OUTPUT_LOG)
            if ((db.protocol & FTP_FLAG)
#ifdef _WITH_WMO_SUPPORT
                || (db.protocol & WMO_FLAG)
#endif
#ifdef _WITH_SCP1_SUPPORT
                || (db.protocol & SCP1_FLAG))
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
   }
   else
   {
      int ret;

      /*
       * It could be that all files where to old to be send.
       */
      /* Remove empty file directory. */
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
   }
#endif /* _WITH_BURST_2 */

   return(files_to_send);
}
