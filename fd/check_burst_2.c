/*
 *  check_burst_2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int check_burst_2(struct job   **p_new_db,
 **                     char         *file_path,
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
 **   allocate memory for p_new_db and fill it up with data from
 **   the message.
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

/* External global variables. */
extern int                        fsa_fd;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*########################### check_burst_2() ###########################*/
int
check_burst_2(struct job   **p_new_db,
              char         *file_path,
              int          *files_to_send,
              unsigned int *values_changed)
{
   int ret = NO;

   *p_new_db = NULL;
#ifdef _WITH_BURST_2
   /*
    * First check if there are any jobs queued for this host.
    * But before we do so lets check if we have to hold a lock on
    * jobs_queued which may save us the creation of a fifo.
    */
retry:
   if (fsa[db.fsa_pos].jobs_queued > 0)
   {
      int have_lock = NO;

      if (fsa[db.fsa_pos].jobs_queued < fsa[db.fsa_pos].allowed_transfers)
      {
         lock_region_w(fsa_fd,
                       (char *)&fsa[db.fsa_pos].jobs_queued - (char *)fsa);
         have_lock = YES;
      }

      /*
       * Make really shure that jobs are queued. Things might have
       * changed during the time we tried to get the lock.
       */
      if (fsa[db.fsa_pos].jobs_queued > 0)
      {
         int  fd;
         char generic_fifo[MAX_PATH_LENGTH],
              *p_generic_fifo;

         (void)strcpy(generic_fifo, p_work_dir);
         (void)strcat(generic_fifo, FIFO_DIR);
         p_generic_fifo = generic_fifo + strlen(generic_fifo);
         (void)strcpy(p_generic_fifo, SF_FIN_FIFO);
         if ((fd = open(generic_fifo, O_RDWR)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       generic_fifo, strerror(errno));
         }
         else
         {
            (void)sprintf(p_generic_fifo, "%s%d", MORE_DATA_FIFO, db.my_pid);
            if (make_fifo(generic_fifo) == SUCCESS)
            {
               int mdfd;

               if ((mdfd = open(generic_fifo, O_RDWR)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() %s : %s",
                             generic_fifo, strerror(errno));
               }
               else
               {
                  pid_t pid = -db.my_pid;

                  if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
                  else
                  {
                     int            status;
                     fd_set         rset;
                     struct timeval timeout;

                     FD_ZERO(&rset);
                     FD_SET(mdfd, &rset);
                     timeout.tv_usec = 0L;
                     timeout.tv_sec = 60L;

                     /* Wait for FD to reply. */
                     status = select(mdfd + 1, &rset, NULL, NULL, &timeout);

                     if ((status > 0) && (FD_ISSET(mdfd, &rset)))
                     {
                        char receive_buffer[MAX_MSG_NAME_LENGTH + 1];

                        if ((status = read(mdfd, &receive_buffer,
                                           (MAX_MSG_NAME_LENGTH + 1))) != (MAX_MSG_NAME_LENGTH + 1))
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "read() error [%d] : %s",
                                      status, strerror(errno));
                        }
                        else
                        {
                           if (receive_buffer[1] != '\0')
                           {
                              char *ptr;

                              ptr = &receive_buffer[1] + strlen(&receive_buffer[1]);
                              while ((*ptr != '_') && (ptr != &receive_buffer[1]))
                              {
                                 ptr--;
                              }
                              if (*ptr != '_')
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            "Failed to locate job ID in message name %s",
                                            &receive_buffer[1]);
                              }
                              else
                              {
                                 int job_id;

                                 ptr++;
                                 job_id = atoi(ptr);
                                 db.error_file = receive_buffer[0];
                                 (void)strcpy(db.msg_name, &receive_buffer[1]);
                                 if (job_id != db.job_id)
                                 {
                                    db.job_id = job_id;
                                    if ((*p_new_db = malloc(sizeof(struct job))) == NULL)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "malloc() error : %s", strerror(errno));
                                    }
                                    else
                                    {
                                       char msg_name[MAX_PATH_LENGTH];

                                       (*p_new_db)->transfer_mode  = DEFAULT_TRANSFER_MODE;
                                       (*p_new_db)->special_ptr    = NULL;
                                       (*p_new_db)->subject        = NULL;
#ifdef _WITH_TRANS_EXEC
                                       (*p_new_db)->trans_exec_cmd = NULL;
#endif
                                       (*p_new_db)->special_flag   = 0;
                                       (*p_new_db)->mode_flag      = ACTIVE_MODE;
                                       (*p_new_db)->archive_time   = DEFAULT_ARCHIVE_TIME;
#ifdef _AGE_LIMIT
                                       (*p_new_db)->age_limit      = DEFAULT_AGE_LIMIT;
#endif
#ifdef _OUTPUT_LOG
                                       (*p_new_db)->output_log     = YES;
#endif
                                       (*p_new_db)->lock           = DEFAULT_LOCK;
                                       (*p_new_db)->smtp_server[0] = '\0';
#ifdef _WITH_SCP1_SUPPORT
                                       (*p_new_db)->chmod          = FILE_MODE;
#endif
                                       (*p_new_db)->chmod_str[0]   = '\0';
                                       (*p_new_db)->trans_rename_rule[0] = '\0';
                                       (*p_new_db)->user_rename_rule[0] = '\0';
                                       (*p_new_db)->no_of_restart_files = 0;
                                       (*p_new_db)->restart_file   = NULL;
                                       (*p_new_db)->user_id        = -1;
                                       (*p_new_db)->group_id       = -1;
                                       (void)strcpy((*p_new_db)->lock_notation, DOT_NOTATION);
                                       (void)sprintf(msg_name, "%s%s/%s",
                                                     p_work_dir, AFD_MSG_DIR, ptr);
                                       if (eval_message(msg_name, *p_new_db) < 0)
                                       {
                                          free(*p_new_db);
                                          *p_new_db = NULL;
                                       }
                                       else
                                       {
                                          ret = YES;
                                       }
                                    }
                                 }
                                 else
                                 {
                                    ret = YES;
                                 }
                              }
                           }
                        }
                     }
                     else if (status == 0)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "No responce from FD while trying to get a new job.");
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "select() error : %s", strerror(errno));
                          }

                     if (close(mdfd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "close() error : %s", strerror(errno));
                     }
                  }
               }
               if (unlink(generic_fifo) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to unlink() %s : %s",
                             generic_fifo, strerror(errno));
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to create fifo %s.", generic_fifo);
            }
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "close() error : %s", strerror(errno));
            }
         }
      }

      if (have_lock == YES)
      {
         unlock_region(fsa_fd,
                       (char *)&fsa[db.fsa_pos].jobs_queued - (char *)fsa);
      }

      if (ret == YES)
      {
         *files_to_send = init_sf_burst2(*p_new_db, file_path, values_changed);
         if (*files_to_send == 0)
         {
            goto retry;
         }
      }
   }
#endif /* _WITH_BURST_2 */

   return(ret);
}
