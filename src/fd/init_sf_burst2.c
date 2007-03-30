/*
 *  init_sf_burst2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
extern int                        *p_no_of_hosts,
                                  fsa_fd,
                                  no_of_hosts,
                                  no_of_rule_headers;
extern long                       transfer_timeout;
extern char                       *p_work_dir;
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
      db.age_limit    = p_new_db->age_limit;
      db.retries      = p_new_db->retries;
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
#ifdef WITH_SSL
         if (db.auth != p_new_db->auth)
         {
            *values_changed |= AUTH_CHANGED;
         }
#endif
      }
      (void)strcpy(db.user, p_new_db->user);
      (void)strcpy(db.password, p_new_db->password);
      (void)strcpy(db.target_dir, p_new_db->target_dir);
#ifdef WITH_SSL
      db.auth = p_new_db->auth;
#endif
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
      if (((db.transfer_mode == 'A') || (db.transfer_mode == 'D')) &&
          (p_new_db->transfer_mode == 'N'))
      {
         db.transfer_mode    = 'I';
      }
      else
      {
         db.transfer_mode    = p_new_db->transfer_mode;
      }
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
      db.special_ptr       = p_new_db->special_ptr;
      db.special_flag      = p_new_db->special_flag;
#ifdef _OUTPUT_LOG
      db.output_log        = p_new_db->output_log;
#endif
      db.mode_flag         = p_new_db->mode_flag;
#ifdef WITH_DUP_CHECK
      db.dup_check_flag    = p_new_db->dup_check_flag;
      db.dup_check_timeout = p_new_db->dup_check_timeout;
      db.crc_id            = p_new_db->crc_id;
#endif
      free(p_new_db);
      p_new_db = NULL;
   }

   if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & DISABLE_ARCHIVE)
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

      (void)strcpy(gbuf, p_work_dir);
      (void)strcat(gbuf, ETC_DIR);
      (void)strcat(gbuf, RENAME_RULE_FILE);
      get_rename_rules(gbuf, NO);
      if (db.trans_rename_rule[0] != '\0')
      {
         if ((db.trans_rule_pos = get_rule(db.trans_rename_rule,
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
         if ((db.user_rule_pos = get_rule(db.user_rename_rule,
                                          no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring this option.",
                       db.user_rename_rule);
            db.user_rename_rule[0] = '\0';
         }
      }
      if (db.subject_rename_rule[0] != '\0')
      {
         if ((db.subject_rule_pos = get_rule(db.subject_rename_rule,
                                             no_of_rule_headers)) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could NOT find rule %s. Ignoring this option.",
                       db.subject_rename_rule);
            db.subject_rename_rule[0] = '\0';
         }
      }
   }

   if ((files_to_send = get_file_names(file_path, &file_size_to_send)) > 0)
   {
      /* Do we want to display the status? */
      (void)gsf_check_fsa();
      if (db.fsa_pos != INCORRECT)
      {
#ifdef LOCK_DEBUG
         rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
         rlock_region(fsa_fd, db.lock_offset);
#endif
         if (db.protocol & FTP_FLAG)
         {
            fsa->job_status[(int)db.job_no].connect_status = FTP_BURST2_TRANSFER_ACTIVE;
         }
         else if (db.protocol & LOC_FLAG)
              {
                 fsa->job_status[(int)db.job_no].connect_status = LOC_BURST_TRANSFER_ACTIVE;
              }
         else if (db.protocol & SFTP_FLAG)
              {
                 fsa->job_status[(int)db.job_no].connect_status = SFTP_BURST_TRANSFER_ACTIVE;
              }
         else if (db.protocol & SCP_FLAG)
              {
                 fsa->job_status[(int)db.job_no].connect_status = SCP_BURST_TRANSFER_ACTIVE;
              }
#ifdef _WITH_WMO_SUPPORT
         else if (db.protocol & WMO_FLAG)
              {
                 fsa->job_status[(int)db.job_no].connect_status = WMO_BURST_TRANSFER_ACTIVE;
              }
#endif
         fsa->job_status[(int)db.job_no].no_of_files = fsa->job_status[(int)db.job_no].no_of_files_done + files_to_send;
         fsa->job_status[(int)db.job_no].file_size = fsa->job_status[(int)db.job_no].file_size_done + file_size_to_send;
         fsa->job_status[(int)db.job_no].job_id = db.job_id;
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset);
#endif

         /* Set the timeout value. */
         transfer_timeout = fsa->transfer_timeout;
      }
   }
   else
   {
      int ret;

      /*
       * It could be that all files where to old to be send.
       */
      /* Remove empty file directory. */
#ifdef WITH_UNLINK_DELAY
      if ((ret = remove_dir(file_path, 0)) < 0)
#else
      if ((ret = remove_dir(file_path)) < 0)
#endif
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
