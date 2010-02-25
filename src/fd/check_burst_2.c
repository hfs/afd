/*
 *  check_burst_2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **                     int          *ol_fd,
 **                     unsigned int *total_append_count,
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
 **   22.01.2005 H.Kiehl Check that ports are the some, otherwise don't
 **                      burst.
 **   11.04.2006 H.Kiehl For protocol SCP we must also check if the
 **                      target directories are the same before we do
 **                      a burst.
 **   18.05.2006 H.Kiehl For protocol SFTP users must be the same,
 **                      otherwise a burst is not possible.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strlen(), strcpy()                 */
#include <stdlib.h>                /* atoi(), malloc()                   */
#include <signal.h>
#include <sys/time.h>              /* struct timeval                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                /* read(), write(), close()           */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif

/* External global variables. */
extern int                        no_of_hosts,
                                  *p_no_of_hosts,
                                  prev_no_of_files_done;
extern unsigned int               burst_2_counter;
extern u_off_t                    prev_file_size_done;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;

/* Local variables. */
static sig_atomic_t               alarm_triggered;

/* Local function prototypes. */
static void                       sig_alarm(int);

#define WAIT_FOR_FD_REPLY 40


/*########################### check_burst_2() ###########################*/
int
check_burst_2(char         *file_path,
              int          *files_to_send,
              int          move_flag,
#ifdef _WITH_INTERRUPT_JOB
              int          interrupt,
#endif
#ifdef _OUTPUT_LOG
              int          *ol_fd,
#endif
#ifndef AFDBENCH_CONFIG
              unsigned int *total_append_count,
#endif
              unsigned int *values_changed)
{
   int ret;

   if ((fsa->protocol_options & DISABLE_BURSTING) == 0)
   {
      int              in_keep_connected_loop;
      unsigned int     alarm_sleep_time,
                       prev_job_id;
      time_t           start_time;
      sigset_t         newmask,
                       oldmask,
                       pendmask,
                       suspmask;
      struct sigaction newact,
                       oldact_alrm,
                       oldact_usr1;
      struct job       *p_new_db;

      /*
       * First check if there are any jobs queued for this host.
       */
      in_keep_connected_loop = NO;
      if ((fsa->keep_connected > 0) &&
          ((fsa->special_flag & KEEP_CON_NO_SEND) == 0))
      {
         db.keep_connected = fsa->keep_connected;
         alarm_sleep_time = DEFAULT_NOOP_INTERVAL;
         start_time = time(NULL);
      }
      else
      {
         db.keep_connected = 0;
         alarm_sleep_time = WAIT_FOR_FD_REPLY;
      }
      do
      {
         ret = NO;
         prev_job_id = 0;

         /* It could be that the FSA changed. */
         if ((gsf_check_fsa() == YES) && (db.fsa_pos == INCORRECT))
         {
            /*
             * Host is no longer in FSA, so there is
             * no way we can communicate with FD.
             */
            return(NO);
         }
         if ((db.protocol != LOC_FLAG) &&
             (strcmp(db.hostname, fsa->real_hostname[(int)(fsa->host_toggle - 1)]) != 0))
         {
            /*
             * Hostname changed, either a switch host or the real
             * hostname has changed. Regardless what ever happened
             * we now need to disconnect.
             */
            fsa->job_status[(int)db.job_no].unique_name[2] = 0;
            return(NO);
         }

         fsa->job_status[(int)db.job_no].unique_name[1] = '\0';
         if (in_keep_connected_loop == YES)
         {
#ifndef AFDBENCH_CONFIG
            int diff_no_of_files_done;
#endif

            newact.sa_handler = sig_alarm;
            sigemptyset(&newact.sa_mask);
            newact.sa_flags = 0;
            if ((sigaction(SIGALRM, &newact, &oldact_alrm) < 0) ||
                (sigaction(SIGUSR1, &newact, &oldact_usr1) < 0))
            {
               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to establish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
               return(NO);
            }
            sigemptyset(&newmask);
            sigaddset(&newmask, SIGALRM);
            sigaddset(&newmask, SIGUSR1);
            if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }
            fsa->job_status[(int)db.job_no].unique_name[2] = 5;
#ifdef _WITH_INTERRUPT_JOB
            if (interrupt == YES)
            {
               fsa->job_status[(int)db.job_no].unique_name[3] = 4;
            }
#endif

#ifndef AFDBENCH_CONFIG
            diff_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done -
                                    prev_no_of_files_done;
            if (diff_no_of_files_done > 0)
            {
               int     length;
               char    how[13],
                       msg_str[MAX_PATH_LENGTH];
               u_off_t diff_file_size_done;

               if (db.protocol & LOC_FLAG)
               {
                  if ((move_flag & FILES_MOVED) &&
                      ((move_flag & FILES_COPIED) == 0))
                  {
                     how[0] = 'm'; how[1] = 'o'; how[2] = 'v';
                     how[3] = 'e'; how[4] = 'd'; how[5] = '\0';
                  }
                  else if (((move_flag & FILES_MOVED) == 0) &&
                           (move_flag & FILES_COPIED))
                       {
                          how[0] = 'c'; how[1] = 'o'; how[2] = 'p';
                          how[3] = 'i'; how[4] = 'e'; how[5] = 'd';
                          how[6] = '\0';
                       }
                       else
                       {
                          how[0] = 'c'; how[1] = 'o'; how[2] = 'p';
                          how[3] = 'i'; how[4] = 'e'; how[5] = 'd';
                          how[6] = '/'; how[7] = 'm'; how[8] = 'o';
                          how[9] = 'v'; how[10] = 'e'; how[11] = 'd';
                          how[12] = '\0';
                       }
               }
               else if (db.protocol & SMTP_FLAG)
                    {
                       how[0] = 'm'; how[1] = 'a'; how[2] = 'i';
                       how[3] = 'l'; how[4] = 'e'; how[5] = 'd';
                       how[6] = '\0';
                    }
                    else
                    {
                       how[0] = 's'; how[1] = 'e'; how[2] = 'n';
                       how[3] = 'd'; how[4] = '\0';
                    }
               diff_file_size_done = fsa->job_status[(int)db.job_no].file_size_done - prev_file_size_done;
               WHAT_DONE_BUFFER(length, msg_str, how,
                                diff_file_size_done, diff_no_of_files_done);
               prev_no_of_files_done = fsa->job_status[(int)db.job_no].no_of_files_done;
               prev_file_size_done = fsa->job_status[(int)db.job_no].file_size_done;
               if (total_append_count != NULL)
               {
                  if (*total_append_count == 1)
                  {
                     (void)strcpy(&msg_str[length], " [APPEND]");
                     length += 9;
                     *total_append_count = 0;
                  }
                  else if (*total_append_count > 1)
                       {
                          length += sprintf(&msg_str[length], " [APPEND * %u]",
                                            *total_append_count);
                          *total_append_count = 0;
                       }
               }
               if ((burst_2_counter - 1) == 1)
               {
                  (void)strcpy(&msg_str[length], " [BURST]");
                  burst_2_counter = 1;
               }
               else if ((burst_2_counter - 1) > 1)
                    {
                       (void)sprintf(&msg_str[length], " [BURST * %u]",
                                     (burst_2_counter - 1));
                       burst_2_counter = 1;
                    }
               trans_log(INFO_SIGN, NULL, 0, NULL, NULL, "%s", msg_str);
            }
#endif
            (void)alarm(alarm_sleep_time);
            suspmask = oldmask;
            sigdelset(&suspmask, SIGALRM);
            sigdelset(&suspmask, SIGUSR1);
            sigsuspend(&suspmask); /* Wait for SIGUSR1 or SIGALRM. */
            (void)alarm(0);
            if (gsf_check_fsa() != NEITHER)
            {
               if (fsa->job_status[(int)db.job_no].unique_name[2] == 5)
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               }
            }

            /*
             * Lets unblock any remaining signals before restoring
             * the original signal handler.
             */
            if (sigpending(&pendmask) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "sigpending() error : %s", strerror(errno));
            }
            else
            {
               if ((sigismember(&pendmask, SIGALRM)) ||
                   (sigismember(&pendmask, SIGUSR1)))
               {
                  if (sigprocmask(SIG_UNBLOCK, &newmask, NULL) < 0)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "sigprocmask() error : %s", strerror(errno));
                  }
               }
            }

            /* Restore the original signal mask. */
            if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to reastablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
            }
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }

            if (fsa->job_status[(int)db.job_no].unique_name[2] == 6)
            {
               /*
                * Another job is waiting that cannot use the current
                * connection.
                */
               fsa->job_status[(int)db.job_no].unique_name[2] = 0;
               return(NO);
            }
         }
         else
         {
            if ((fsa->jobs_queued > 0) &&
                (fsa->active_transfers == fsa->allowed_transfers))
            {
               int   fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
               int   readfd;
#endif
               pid_t pid;
               char  generic_fifo[MAX_PATH_LENGTH];

               (void)strcpy(generic_fifo, p_work_dir);
               (void)strcat(generic_fifo, FIFO_DIR);
               (void)strcat(generic_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(generic_fifo, &readfd, &fd) == -1)
#else
               if ((fd = open(generic_fifo, O_RDWR)) == -1)
#endif
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() %s : %s",
                             generic_fifo, strerror(errno));
                  return(NO);
               }
               alarm_triggered = NO;
               pid = -db.my_pid;

               newact.sa_handler = sig_alarm;
               sigemptyset(&newact.sa_mask);
               newact.sa_flags = 0;
               if ((sigaction(SIGALRM, &newact, &oldact_alrm) < 0) ||
                   (sigaction(SIGUSR1, &newact, &oldact_usr1) < 0))
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to establish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                             strerror(errno));
                  return(NO);
               }
               sigemptyset(&newmask);
               sigaddset(&newmask, SIGALRM);
               sigaddset(&newmask, SIGUSR1);
               if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "sigprocmask() error : %s", strerror(errno));
               }

               fsa->job_status[(int)db.job_no].unique_name[2] = 4;
#ifdef _WITH_INTERRUPT_JOB
               if (interrupt == YES)
               {
                  fsa->job_status[(int)db.job_no].unique_name[3] = 4;
               }
#endif

               if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
               {
                  int tmp_errno = errno;

                  fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                      (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to reastablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                                strerror(errno));
                  }
                  if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "sigprocmask() error : %s", strerror(errno));
                  }
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(tmp_errno));
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  if ((close(readfd) == -1) || (close(fd) == -1))
#else
                  if (close(fd) == -1)
#endif
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "close() error : %s", strerror(errno));
                  }
                  return(NO);
               }
               (void)alarm(alarm_sleep_time);
               suspmask = oldmask;
               sigdelset(&suspmask, SIGALRM);
               sigdelset(&suspmask, SIGUSR1);
               sigsuspend(&suspmask);
               (void)alarm(0);
               if (gsf_check_fsa() != NEITHER)
               {
                  if (fsa->job_status[(int)db.job_no].unique_name[2] == 4)
                  {
                     fsa->job_status[(int)db.job_no].unique_name[2] = 0;
                  }
               }

               /*
                * Lets unblock any remaining signals before restoring
                * the original signal handler.
                */
               if (sigpending(&pendmask) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "sigpending() error : %s", strerror(errno));
               }
               else
               {
                  if ((sigismember(&pendmask, SIGALRM)) ||
                      (sigismember(&pendmask, SIGUSR1)))
                  {
                     if (sigprocmask(SIG_UNBLOCK, &newmask, NULL) < 0)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "sigprocmask() error : %s", strerror(errno));
                     }
                  }
               }

               /* Restore the original signal mask. */
               if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                   (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to reastablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                             strerror(errno));
               }
               if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "sigprocmask() error : %s", strerror(errno));
               }

#ifdef WITHOUT_FIFO_RW_SUPPORT
               if ((close(readfd) == -1) || (close(fd) == -1))
#else
               if (close(fd) == -1)
#endif
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "close() error : %s", strerror(errno));
               }

               if ((alarm_triggered == YES) &&
                   (fsa->job_status[(int)db.job_no].unique_name[1] == '\0'))
               {
                  if (gsf_check_fsa() != NEITHER)
                  {
                     fsa->job_status[(int)db.job_no].unique_name[2] = 1;
                  }
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                             "Hmmm, FD had no message for <%s> [%u sec] [job %d (%d)]!",
#else
                             "Hmmm, FD had no message for <%s> [%u sec] [job %d (%lld)]!",
#endif
                             fsa->host_alias, alarm_sleep_time,
                             (int)db.job_no, (pri_pid_t)-pid);
                  return(NO);
               }
            }
            else
            {
               if (db.keep_connected == 0)
               {
                  return(NO);
               }
               else
               {
                  ret = NO;
               }
            }
         }

         /* It could be that the FSA changed. */
         if ((gsf_check_fsa() == YES) && (db.fsa_pos == INCORRECT))
         {
            /*
             * Host is no longer in FSA, so there is
             * no way we can communicate with FD.
             */
            return(NO);
         }

         if ((fsa->job_status[(int)db.job_no].unique_name[1] != '\0') &&
             (fsa->job_status[(int)db.job_no].unique_name[0] != '\0') &&
             (fsa->job_status[(int)db.job_no].unique_name[2] != '\0'))
         {
            (void)memcpy(db.msg_name, 
                         fsa->job_status[(int)db.job_no].unique_name,
                         MAX_MSG_NAME_LENGTH);
            if (fsa->job_status[(int)db.job_no].job_id != db.job_id)
            {
               char msg_name[MAX_PATH_LENGTH];

               prev_job_id = db.job_id;
               db.job_id = fsa->job_status[(int)db.job_no].job_id;
               if ((p_new_db = malloc(sizeof(struct job))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "malloc() error : %s", strerror(errno));
                  return(NO);
               }

               if (fsa->protocol_options & FTP_IGNORE_BIN)
               {
                  p_new_db->transfer_mode     = 'N';
               }
               else
               {
                  p_new_db->transfer_mode     = DEFAULT_TRANSFER_MODE;
               }
               p_new_db->special_ptr          = NULL;
               p_new_db->subject              = NULL;
               p_new_db->from                 = NULL;
               p_new_db->reply_to             = NULL;
               p_new_db->charset              = NULL;
               p_new_db->lock_file_name       = NULL;
#ifdef _WITH_TRANS_EXEC
               p_new_db->trans_exec_cmd       = NULL;
               p_new_db->trans_exec_timeout   = DEFAULT_EXEC_TIMEOUT;
               p_new_db->set_trans_exec_lock  = NO;
#endif
               p_new_db->special_flag         = 0;
               p_new_db->mode_flag            = 0;
               p_new_db->archive_time         = DEFAULT_ARCHIVE_TIME;
               if ((fsa->job_status[(int)db.job_no].file_name_in_use[0] == '\0') &&
                   (fsa->job_status[(int)db.job_no].file_name_in_use[1] == 1))
               {
                  p_new_db->retries           = (unsigned int)atoi(&fsa->job_status[(int)db.job_no].file_name_in_use[2]);
                  if (p_new_db->retries > 0)
                  {
                     p_new_db->special_flag |= OLD_ERROR_JOB;
                  }
               }
               else
               {
                  p_new_db->retries           = 0;
               }
               p_new_db->age_limit            = DEFAULT_AGE_LIMIT;
#ifdef _OUTPUT_LOG
               p_new_db->output_log           = YES;
#endif
               p_new_db->lock                 = DEFAULT_LOCK;
               p_new_db->smtp_server[0]       = '\0';
               p_new_db->chmod_str[0]         = '\0';
               p_new_db->trans_rename_rule[0] = '\0';
               p_new_db->user_rename_rule[0]  = '\0';
               p_new_db->rename_file_busy     = '\0';
               p_new_db->group_list           = NULL;
               p_new_db->no_of_restart_files  = 0;
               p_new_db->restart_file         = NULL;
               p_new_db->user_id              = -1;
               p_new_db->group_id             = -1;
#ifdef WITH_DUP_CHECK
               p_new_db->dup_check_flag       = fsa->dup_check_flag;
               p_new_db->dup_check_timeout    = fsa->dup_check_timeout;
#endif
#ifdef WITH_SSL
               p_new_db->auth                 = NO;
#endif
               p_new_db->ssh_protocol         = 0;
               if (db.protocol & FTP_FLAG)
               {
                  p_new_db->port = DEFAULT_FTP_PORT;
               }
               else if (db.protocol & SFTP_FLAG)
                    {
                       p_new_db->port = DEFAULT_SSH_PORT;
                    }
#ifdef _WITH_SCP_SUPPORT
               else if (db.protocol & SCP_FLAG)
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
                    else
                    {
                       p_new_db->port = -1;
                    }
               if (fsa->protocol_options & USE_SEQUENCE_LOCKING)
               {
                  p_new_db->special_flag |= SEQUENCE_LOCKING;
               }
               (void)strcpy(p_new_db->lock_notation, DOT_NOTATION);
               (void)sprintf(msg_name, "%s%s/%x",
                             p_work_dir, AFD_MSG_DIR, db.job_id);
               if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & ENABLE_CREATE_TARGET_DIR)
               {
                  p_new_db->special_flag |= CREATE_TARGET_DIR;
               }

               /*
                * NOTE: We must set protocol for eval_message()
                *       otherwise some values are NOT set!
                */
               p_new_db->protocol = db.protocol;
               p_new_db->job_id = db.job_id;
               if (eval_message(msg_name, p_new_db) < 0)
               {
                  free(p_new_db);

                  return(NO);
               }

               /*
                * Ports must be the same!
                */
               if ((p_new_db->port != db.port) ||
#ifdef _WITH_SCP_SUPPORT
                   ((db.protocol & SCP_FLAG) &&
                    (strcmp(p_new_db->target_dir, db.target_dir) != 0)) ||
#endif
#ifdef WITH_SSL
                   ((db.auth == NO) && (p_new_db->auth != NO)) ||
                   ((db.auth != NO) && (p_new_db->auth == NO)) ||
#endif
                   ((db.protocol & SFTP_FLAG) &&
                    (strcmp(p_new_db->user, db.user) != 0)))
               {
                  free(p_new_db);
                  ret = NEITHER;
               }
               else
               {
                  if ((p_new_db->protocol & FTP_FLAG) &&
                      (p_new_db->mode_flag == 0))
                  {
                     if (fsa->protocol_options & FTP_PASSIVE_MODE)
                     {
                        p_new_db->mode_flag = PASSIVE_MODE;
                        if (fsa->protocol_options & FTP_EXTENDED_MODE)
                        {
                           (void)strcpy(p_new_db->mode_str, "extended passive");
                        }
                        else
                        {
                           if (fsa->protocol_options & FTP_ALLOW_DATA_REDIRECT)
                           {
                              (void)strcpy(p_new_db->mode_str,
                                           "passive (with redirect)");
                           }
                           else
                           {
                              (void)strcpy(p_new_db->mode_str, "passive");
                           }
                        }
                     }
                     else
                     {
                        p_new_db->mode_flag = ACTIVE_MODE;
                        if (fsa->protocol_options & FTP_EXTENDED_MODE)
                        {
                           (void)strcpy(p_new_db->mode_str, "extended active");
                        }
                        else
                        {
                           (void)strcpy(p_new_db->mode_str, "active");
                        }
                     }
                     if (fsa->protocol_options & FTP_EXTENDED_MODE)
                     {
                        p_new_db->mode_flag |= EXTENDED_MODE;
                     }
                  }
                  ret = YES;
               }
            }
            else
            {
               p_new_db = NULL;
               ret = YES;
            }
         }

         if (ret == YES)
         {
            *files_to_send = init_sf_burst2(p_new_db, file_path, values_changed,
                                            prev_job_id);
            if (*files_to_send < 1)
            {
               ret = RETRY;
            }
         }
         else if ((ret == NO) && (db.keep_connected > 0))
              {
                 if (time(NULL) < (start_time + db.keep_connected))
                 {
#ifdef _OUTPUT_LOG
                    if (*ol_fd > -1)
                    {
                       (void)close(*ol_fd);
                       *ol_fd = -2;
                    }
#endif
                    if (fsa->transfer_rate_limit > 0)
                    {
                       int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       int  readfd;
#endif
                       char trl_calc_fifo[MAX_PATH_LENGTH];

                       (void)strcpy(trl_calc_fifo, p_work_dir);
                       (void)strcat(trl_calc_fifo, FIFO_DIR);
                       (void)strcat(trl_calc_fifo, TRL_CALC_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                       if (open_fifo_rw(trl_calc_fifo, &readfd, &fd) == -1)
#else
                       if ((fd = open(trl_calc_fifo, O_RDWR)) == -1)
#endif
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to open() FIFO `%s' : %s",
                                     trl_calc_fifo, strerror(errno));
                       }
                       else
                       {
                          if (write(fd, &db.fsa_pos, sizeof(int)) != sizeof(int))
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to write() to FIFO `%s' : %s",
                                        trl_calc_fifo, strerror(errno));
                          }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                          if (close(readfd) == -1)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Failed to close() FIFO `%s' (read) : %s",
                                        trl_calc_fifo, strerror(errno));
                          }
#endif
                          if (close(fd) == -1)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Failed to close() FIFO `%s' (read) : %s",
                                        trl_calc_fifo, strerror(errno));
                          }
                       }
                    }
                    if ((db.protocol & FTP_FLAG) || (db.protocol & SFTP_FLAG) ||
                        (db.protocol & SMTP_FLAG))
                    {
                       if (noop_wrapper() == SUCCESS)
                       {
                          ret = RETRY;
                       }
                    }
                    else
                    {
                       ret = RETRY;
                    }

                    if (ret == RETRY)
                    {
                       time_t diff_time;

                       diff_time = time(NULL) - start_time;
                       if (diff_time < db.keep_connected)
                       {
                          if ((diff_time > DEFAULT_NOOP_INTERVAL) ||
                              (diff_time == 0))
                          {
                             alarm_sleep_time = DEFAULT_NOOP_INTERVAL;
                          }
                          else
                          {
                             alarm_sleep_time = diff_time;
                          }
                          if (alarm_sleep_time > db.keep_connected)
                          {
                             alarm_sleep_time = db.keep_connected;
                          }
                          if (alarm_sleep_time == 0)
                          {
                             ret = NO;
                          }
                          else
                          {
                             in_keep_connected_loop = YES;
                          }
                       }
                       else
                       {
                          ret = NO;
                       }
                    }
                 }
              }
      } while (ret == RETRY);
   }
   else
   {
      ret = NO;
   }

   return(ret);
}


/*+++++++++++++++++++++++++++++++ sig_alarm() +++++++++++++++++++++++++++*/
static void                                                                
sig_alarm(int signo)
{
   if (signo == SIGALRM)
   {
      alarm_triggered = YES;
   }

   return; /* Return to wakeup sigsuspend(). */
}
