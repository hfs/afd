/*
 *  check_burst_2.c - Part of AFD, an automatic file distribution program.
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
 **   check_burst_2 - checks if FD still has jobs in the queue
 **
 ** SYNOPSIS
 **   int check_burst_2(char         *file_path,
 **                     int          *files_to_send,
 **                     unsigned int *values_changed)
 **
 ** DESCRIPTION
 **   The function check_burst_2() checks if FD has jobs in the queue
 **   for this host. If so it gets the new job name and if it is in
 **   the error directory via a fifo created by this function. The
 **   fifo will be removed once it has the data.
 **
 **   The structure of data send via the fifo will be as follows:
 **             char in_error_dir
 **             char msg_name[MAX_MSG_NAME_LENGTH]
 **
 ** RETURN VALUES
 **   Returns NO if FD does not have any job in queue or if an error
 **   has occured. If there is a job in queue YES will be returned
 **   and if the job_id of the current job is not the same it will
 **   fill up the structure job db with the new data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.05.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strlen(), strcpy()                 */
#include <stdlib.h>                /* atoi(), malloc()                   */
#include <sys/time.h>              /* struct timeval                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                /* read(), write(), close(), rmdir()  */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#ifdef _WITH_SCP1_SUPPORT
#include "scp1defs.h"
#endif
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables. */
extern int                        no_of_hosts;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*########################### check_burst_2() ###########################*/
int
check_burst_2(char         *file_path,
              int          *files_to_send,
              unsigned int *values_changed)
{
   int ret;

#ifdef _WITH_BURST_2
   /*
    * First check if there are any jobs queued for this host.
    */
retry:
   /* It could be that the FSA changed. */
   if (check_fsa() == YES)
   {
      if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                          no_of_hosts)) == INCORRECT)
      {
         /*
          * Host is no longer in FSA, so there is
          * no way we can communicate with FD.
          */
         return(NO);
      }
   }
   ret = NO;
   if ((fsa[db.fsa_pos].jobs_queued > 0) &&
       (fsa[db.fsa_pos].active_transfers >= fsa[db.fsa_pos].allowed_transfers))
   {
      struct job *p_new_db = NULL;
      int        fd;
      char       generic_fifo[MAX_PATH_LENGTH];

      (void)strcpy(generic_fifo, p_work_dir);
      (void)strcat(generic_fifo, FIFO_DIR);
      (void)strcat(generic_fifo, SF_FIN_FIFO);
      if ((fd = open(generic_fifo, O_RDWR)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to open() %s : %s",
                    generic_fifo, strerror(errno));
      }
      else
      {
         pid_t pid = -db.my_pid;

         fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[1] = '\0';
         fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[2] = 4;
         fsa[db.fsa_pos].job_status[(int)db.job_no].error_file = NO;
         if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
         else
         {
            unsigned long sleep_time = 0L;

            do
            {
               /* It could be that the FSA changed. */
               if (check_fsa() == YES)
               {
                  if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                      no_of_hosts)) == INCORRECT)
                  {
                     /*
                      * Host is no longer in FSA, so there is
                      * no way we can communicate with FD.
                      */
                     (void)close(fd);
                     return(NO);
                  }
               }
               if (fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[1] == '\0')
               {
                  my_usleep(50000L);
                  sleep_time += 50000L;
               }
               else
               {
                  break;
               }
            } while (sleep_time < 120000000L); /* Wait 120 seconds. */

            if ((fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[1] != '\0') &&
                (fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[0] != '\0'))
            {
               db.error_file = fsa[db.fsa_pos].job_status[(int)db.job_no].error_file;
               (void)memcpy(db.msg_name, 
                            fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name,
                            MAX_MSG_NAME_LENGTH);
               if (fsa[db.fsa_pos].job_status[(int)db.job_no].job_id != db.job_id)
               {
                  db.job_id = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
                  if ((p_new_db = malloc(sizeof(struct job))) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "malloc() error : %s", strerror(errno));
                  }
                  else
                  {
                     char msg_name[MAX_PATH_LENGTH];

                     p_new_db->transfer_mode        = DEFAULT_TRANSFER_MODE;
                     p_new_db->special_ptr          = NULL;
                     p_new_db->subject              = NULL;
#ifdef _WITH_TRANS_EXEC
                     p_new_db->trans_exec_cmd       = NULL;
#endif
                     p_new_db->special_flag         = 0;
                     p_new_db->mode_flag            = 0;
                     p_new_db->archive_time         = DEFAULT_ARCHIVE_TIME;
#ifdef _AGE_LIMIT
                     p_new_db->age_limit            = DEFAULT_AGE_LIMIT;
#endif
#ifdef _OUTPUT_LOG
                     p_new_db->output_log           = YES;
#endif
                     p_new_db->lock                 = DEFAULT_LOCK;
                     p_new_db->smtp_server[0]       = '\0';
                     p_new_db->chmod_str[0]         = '\0';
                     p_new_db->trans_rename_rule[0] = '\0';
                     p_new_db->user_rename_rule[0]  = '\0';
                     p_new_db->no_of_restart_files  = 0;
                     p_new_db->restart_file         = NULL;
                     p_new_db->user_id              = -1;
                     p_new_db->group_id             = -1;
                     if (db.protocol & FTP_FLAG)
                     {
                        p_new_db->port = DEFAULT_FTP_PORT;
                     }
#ifdef _WITH_SCP1_SUPPORT
                     else if (db.protocol & SCP1_FLAG)
                          {
                             p_new_db->port = DEFAULT_SSH_PORT;
                             p_new_db->chmod = FILE_MODE;
                          }
#endif
#ifdef _WITH_WMO_SUPPORT
                     else if (db.protocol & WMO_FLAG)
                          {
                             p_new_db->port = DEFAULT_WMO_PORT;
                          }
#endif
                     else if (db.protocol & SMTP_FLAG)
                          {
                             p_new_db->port = DEFAULT_SMTP_PORT;
                          }
                     (void)strcpy(p_new_db->lock_notation, DOT_NOTATION);
                     (void)sprintf(msg_name, "%s%s/%u",
                                   p_work_dir, AFD_MSG_DIR, db.job_id);

                     /*
                      * NOTE: We must set protocol for eval_message()
                      *       otherwise some values are NOT set!
                      */
                     p_new_db->protocol = db.protocol;
                     if (eval_message(msg_name, p_new_db) < 0)
                     {
                        free(p_new_db);
                        p_new_db = NULL;
                     }
                     else
                     {
                        if (p_new_db->mode_flag == 0)
                        {
                           p_new_db->mode_flag = PASSIVE_MODE;
                        }
                        else
                        {
                           p_new_db->mode_flag = ACTIVE_MODE;
                        }
                        ret = YES;
                     }
                  }
               }
               else
               {
                  ret = YES;
               }
            }
            else
            {
               if (sleep_time >= 120000000L)
               {
                  fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name[2] = 1;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, failed to get a message from FD for <%s> after 120 seconds!",
                             fsa[db.fsa_pos].host_alias);
               }
#ifdef _DEBUG_BURST2
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, FD had no message for <%s> [%lu msec]!",
                             fsa[db.fsa_pos].host_alias, sleep_time);
               }
#endif /* _DEBUG_BURST2 */
            }
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }

      if (ret == YES)
      {
         *files_to_send = init_sf_burst2(p_new_db, file_path, values_changed);
         if (*files_to_send < 1)
         {
            goto retry;
         }
      }
   }
#endif /* _WITH_BURST_2 */

   return(ret);
}
