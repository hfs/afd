/*
 *  init_msg_buffer.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_msg_buffer - initialises the structures queue_buf and
 **                     msg_cache_buf
 **
 ** SYNOPSIS
 **   void init_msg_buffer(void)
 **
 ** DESCRIPTION
 **   The function init_msg_buffer() initialises the structures
 **   queue_buf and msg_cache_buf, and removes any old messages
 **   from them. Also any old message not in both buffers and
 **   older then the oldest output log file are removed.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <unistd.h>     /* lseek(), write(), unlink()                    */
#include <string.h>     /* strcpy(), strcat(), strerror()                */
#include <time.h>       /* time()                                        */
#include <stdlib.h>     /* malloc(), free()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>   /* mmap(), msync(), munmap()                     */
#include <signal.h>     /* kill(), SIGKILL                               */
#include <fcntl.h>
#include <dirent.h>     /* opendir(), closedir(), readdir(), DIR         */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int                        mdb_fd,
                                  qb_fd,
                                  *no_msg_cached,
                                  *no_msg_queued,
                                  no_of_hosts,
                                  sys_log_fd;
extern char                       file_dir[],
                                  err_file_dir[],
                                  msg_dir[],
                                  *p_err_file_dir,
                                  *p_file_dir,
                                  *p_msg_dir,
                                  *p_work_dir;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;

/* Local global variables */
static int                        *no_of_job_ids,
                                  removed_jobs,
                                  removed_messages,
                                  *rjl, /* removed job list */
                                  *rml; /* removed message list */

/* Local function prototypes */
static void                       read_current_msg_list(int **, int *),
                                  sort_array(int, int);
static int                        remove_job(int, int, struct job_id_data *,
                                             unsigned int);


/*########################## init_msg_buffer() ##########################*/
void
init_msg_buffer(void)
{
   register int       i;
   int                *cml,            /* Current message list array */
                      jd_fd,
                      sleep_counter = 0,
                      no_of_current_msg;
   time_t             current_time;
   off_t              jid_struct_size;
   char               *ptr,
                      job_id_data_file[MAX_PATH_LENGTH];
   struct stat        stat_buf;
   struct job_id_data *jd;
   DIR                *dp;

   removed_jobs = removed_messages = 0;
   rml = NULL;
   rjl = NULL;

   /* If necessary attach to the buffers. */
   if (mdb_fd == -1)
   {
      size_t new_size = (MSG_CACHE_BUF_SIZE * sizeof(struct msg_cache_buf)) +
                        AFD_WORD_OFFSET;
      char   fullname[MAX_PATH_LENGTH];

      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, MSG_CACHE_FILE);
      if ((ptr = attach_buf(fullname, &mdb_fd,
                            new_size, "FD")) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   fullname, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_msg_cached = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      mdb = (struct msg_cache_buf *)ptr;
   }

   if (qb_fd == -1)
   {
      size_t new_size = (MSG_QUE_BUF_SIZE * sizeof(struct queue_buf)) +
                        AFD_WORD_OFFSET;
      char   fullname[MAX_PATH_LENGTH];

      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, MSG_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &qb_fd,
                            new_size, "FD")) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   fullname, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_msg_queued = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      qb = (struct queue_buf *)ptr;
   }

   /* Attach to job_id_data structure, so we can remove any old data. */
   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((jd_fd = coe_open(job_id_data_file, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         do
         {
            (void)sleep(1);
            errno = 0;
            sleep_counter++;
            if (((jd_fd = coe_open(job_id_data_file, O_RDWR)) == -1) &&
                ((errno != ENOENT) || (sleep_counter > 10)))
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         job_id_data_file, strerror(errno),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
         } while (errno == ENOENT);
      }
      else
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   job_id_data_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   sleep_counter = 0;
   while (p_afd_status->start_time == 0)
   {
      (void)sleep(1);
      sleep_counter++;
      if (sleep_counter > 20)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Timeout arrived for waiting for AMG to finish writting to JID structure. (%s %d)\n",
                   __FILE__, __LINE__);
      }
   }

stat_again:
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   sleep_counter = 0;
   while (stat_buf.st_size == 0)
   {
      (void)sleep(1);
      if (fstat(jd_fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   job_id_data_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      sleep_counter++;
      if (sleep_counter > 10)
      {
         break;
      }
   }

   /*
    * If we lock the file to early init_job_data() of the AMG does not
    * get the time to fill all data into the JID structure.
    */
   sleep_counter = 0;
   while ((p_afd_status->amg_jobs & WRITTING_JID_STRUCT) == 1)
   {
      (void)sleep(1);
      sleep_counter++;
      if (sleep_counter > 10)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Timeout arrived for waiting for AMG to finish writting to JID structure. (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   lock_region_w(jd_fd, 1);
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to fstat() %s : %s (%s %d)\n",
                job_id_data_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   unlock_region(jd_fd, 1);

   if (stat_buf.st_size > 0)
   {
      if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   job_id_data_file, strerror(errno),
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "File %s is empty! Terminating, don't know what to do :-( (%s %d)\n",
                job_id_data_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > stat_buf.st_size)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Hmmmm. Size of %s is %d Bytes, but calculation says it should be %d Bytes! (%s %d)\n",
                JOB_ID_DATA_FILE, stat_buf.st_size,
                (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET,
                __FILE__, __LINE__);
      ptr = (char *)jd - AFD_WORD_OFFSET;
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      (void)sleep(1);
      if (sleep_counter > 20)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Something is really wrong here! Size of structure is not what it should be! (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
      sleep_counter++;
      goto stat_again;
   }
#ifdef _DEBUG
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "stat_buf.st_size = %d|no_of_job_ids = %d|struct size = %d|total plus word offset = %d (%s %d)\n",
             stat_buf.st_size, *no_of_job_ids, sizeof(struct job_id_data),
             (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET,
             __FILE__, __LINE__);
#endif /* DEBUG */
   jid_struct_size = stat_buf.st_size;

   /* Read and store current message list. */
   read_current_msg_list(&cml, &no_of_current_msg);

   /*
    * Compare the current message list with those in the cache. Each
    * message not found is read from the message file and appended to
    * the cache. All messages that are in the current message list
    * are marked.
    */
   for (i = 0; i < *no_msg_cached; i++)
   {
      mdb[i].in_current_fsa = NO;
   }
   if (*no_msg_cached > 0)
   {
      register int gotcha,
                   j;

      for (i = 0; i < no_of_current_msg; i++)
      {
         gotcha = NO;
         for (j = 0; j < *no_msg_cached; j++)
         {
            if (cml[i] == mdb[j].job_id)
            {
               int pos;

               /*
                * The position of this job might have changed
                * within the FSA, but the contents of the job is
                * still the same.
                */
               if ((pos = get_host_position(fsa, mdb[j].host_name,
                                            no_of_hosts)) != -1)
               {
                  mdb[j].in_current_fsa = YES;
                  mdb[j].fsa_pos = pos;
                  gotcha = YES;
                  break;
               }
               else
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Hmmm. Host %s no longer in FSA [JID = %d] (%s %d)\n",
                            mdb[j].host_name, mdb[j].job_id,
                            __FILE__, __LINE__);
               }
            }
         }
         if (gotcha == NO)
         {
            if (get_job_data(cml[i], -1) == SUCCESS)
            {
               mdb[*no_msg_cached - 1].in_current_fsa = YES;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Added job %d to cache. (%s %d)\n",
                         mdb[*no_msg_cached - 1].job_id,
                         __FILE__, __LINE__);
            }
         }
      }
   }
   else /* No messages cached. */
   {
      for (i = 0; i < no_of_current_msg; i++)
      {
         if (get_job_data(cml[i], -1) == SUCCESS)
         {
            mdb[*no_msg_cached - 1].in_current_fsa = YES;
         }
         else
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Added job %d to cache. (%s %d)\n",
                      mdb[*no_msg_cached - 1].job_id,
                      __FILE__, __LINE__);
         }
      }
   }
   free(cml);

#ifdef _OUTPUT_LOG
   /*
    * Go through the message directory and check if any unmarked message
    * is NOT in the queue buffer and the age of the message file is not
    * older then the oldest output log file [They are kept for show_olog
    * which can resend files]. Remove such a message from the msg_cache_buf
    * structure AND from the message directory AND job_id_data structure.
    */
   current_time = time(NULL);
   *p_msg_dir = '\0';
   if ((dp = opendir(msg_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s. Thus unable to delete any old messages. : %s (%s %d)\n",
                msg_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char          *ck_ml;             /* Checked message list */
      struct dirent *p_dir;
      struct stat   stat_buf;

      if ((ck_ml = malloc(*no_msg_cached)) == NULL)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "malloc() error, cannot perform full message cache check : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         (void)memset(ck_ml, NO, *no_msg_cached);
      }

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         (void)strcpy(p_msg_dir, p_dir->d_name);
         if (stat(msg_dir, &stat_buf) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      msg_dir, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (current_time >
                (stat_buf.st_mtime + (SWITCH_FILE_TIME * MAX_OUTPUT_LOG_FILES)))
            {
               unsigned int job_id = (unsigned int)atoi(p_dir->d_name);
               int          gotcha = NO;

               for (i = 0; i < *no_msg_cached; i++)
               {
                  if (mdb[i].job_id == job_id)
                  {
                     if (ck_ml != NULL)
                     {
                        ck_ml[i] = YES;
                     }
                     if (mdb[i].in_current_fsa == YES)
                     {
                        gotcha = YES;
                     }
                     else if (current_time <
                              (mdb[i].last_transfer_time + (SWITCH_FILE_TIME * MAX_OUTPUT_LOG_FILES)))
                          {
                             /*
                              * Hmmm. Files have been transfered. Lets
                              * set gotcha so it does not delete this
                              * job, so show_olog can still resend files.
                              */
                             gotcha = YES;
                          }
                     break;
                  }
               } /* for (i = 0; i < *no_msg_cached; i++) */

               /*
                * Yup, we can remove this job.
                */
               if (gotcha == NO)
               {
                  (void)remove_job(i, jd_fd, jd, job_id);
               } /* if (gotcha == NO) */
            }
         }
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "readdir() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "closedir() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }

      if (removed_jobs > 0)
      {
         lock_region_w(jd_fd, 1);
         sort_array(0, removed_jobs - 1);

         /*
          * Always ensure that the JID structure size did NOT
          * change, else we might be moving things that do not
          * belong to us!
          */
         if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > jid_struct_size)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmmmm. Size of %s is %d Bytes, but calculation says it should be %d Bytes! (%s %d)\n",
                      JOB_ID_DATA_FILE, jid_struct_size,
                      (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET,
                      __FILE__, __LINE__);
            ptr = (char *)jd - AFD_WORD_OFFSET;
            if (munmap(ptr, jid_struct_size) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "munmap() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
            if (fstat(jd_fd, &stat_buf) == -1)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to fstat() %s : %s (%s %d)\n",
                         job_id_data_file, strerror(errno),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (stat_buf.st_size > 0)
            {
               if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Failed to mmap() to %s : %s (%s %d)\n",
                            job_id_data_file, strerror(errno),
                            __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               no_of_job_ids = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               jd = (struct job_id_data *)ptr;
               jid_struct_size = stat_buf.st_size;
            }
            else
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "File %s is empty! Terminating, don't know what to do :-( (%s %d)\n",
                         job_id_data_file, __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }

         for (i = 0; i < removed_jobs; i++)
         {
            int end_pos = i,
                j,
                jobs_deleted,
                start_pos = i;

            while ((++end_pos < removed_jobs) &&
                   ((rjl[end_pos - 1] + 1) == rjl[end_pos]))
            {
               /* Nothing to be done. */;
            }
            if (rjl[end_pos - 1] != (*no_of_job_ids - 1))
            {
               size_t move_size = (*no_of_job_ids - (rjl[end_pos - 1] + 1)) *
                                  sizeof(struct job_id_data);

#ifdef _DEBUG
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "no_of_job_ids = %d|struct size = %d|move_size = %d|&jd[%d] = %d|&jd[%d] = %d (%s %d)\n",
                         *no_of_job_ids, sizeof(struct job_id_data),
                         move_size, rjl[i], &jd[rjl[i]],
                         rjl[end_pos - 1] + 1, &jd[rjl[end_pos - 1] + 1],
                         __FILE__, __LINE__);
#endif /* _DEBUG */
               (void)memmove(&jd[rjl[i]], &jd[rjl[end_pos - 1] + 1], move_size);
            }
            jobs_deleted = end_pos - i;
            (*no_of_job_ids) -= jobs_deleted;
            i += (end_pos - i - 1);
            for (j = (i + 1); j < removed_jobs; j++)
            {
               if (rjl[j] > rjl[start_pos])
               {
                  rjl[j] -= jobs_deleted;
               }
            }
         } /* for (i = 0; i < removed_jobs; i++) */
         unlock_region(jd_fd, 1);
#ifdef _DEBUG
         (void)rec(sys_log_fd, DEBUG_SIGN, "no_of_job_ids = %d (%s %d)\n",
                   *no_of_job_ids, __FILE__, __LINE__);
#endif /* _DEBUG */
         free(rjl);
         rjl = NULL;
         removed_jobs = 0;
      }

      /*
       * Lets go through the message cache again and locate any jobs
       * no longer of interest. This is necessary when messages
       * in the message directory are deleted by hand.
       * At the same time lets check if the host for the cached
       * message is still in the FSA.
       */
      if (ck_ml != NULL)
      {
         int remove_flag;

         for (i = 0; i < *no_msg_cached; i++)
         {
            remove_flag = NO;

            if (ck_ml[i] == NO)
            {
               if ((mdb[i].in_current_fsa != YES) &&
                   (current_time > (mdb[i].last_transfer_time + (SWITCH_FILE_TIME * MAX_OUTPUT_LOG_FILES))))
               {
                  remove_flag = YES;
               }
            }

            if ((mdb[i].fsa_pos < no_of_hosts) && (mdb[i].fsa_pos > -1))
            {
               if (strcmp(mdb[i].host_name, fsa[mdb[i].fsa_pos].host_alias) != 0)
               {
                  int new_pos;

                  if ((new_pos = get_host_position(fsa, mdb[i].host_name, no_of_hosts)) == INCORRECT)
                  {
                     (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Hmm. Host %s is no longer in the FSA. Removed it from cache. (%s %d)\n",
                               mdb[i].host_name, __FILE__, __LINE__);
                     mdb[i].fsa_pos = -1; /* So remove_files() does NOT write to FSA! */
                     remove_flag = YES;
                  }
                  else
                  {
#ifdef _DEBUG
                     (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Hmm. Host position for %s is incorrect!? Correcting %d->%d (%s %d)\n",
                               mdb[i].host_name, mdb[i].fsa_pos,
                               new_pos, __FILE__, __LINE__);
#endif /* _DEBUG */
                     mdb[i].fsa_pos = new_pos;
                  }
               }
            }
            else
            {
               int new_pos;

               if ((new_pos = get_host_position(fsa, mdb[i].host_name,
                                                no_of_hosts)) == INCORRECT)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Hmm. Host %s is no longer in the FSA. Removed it from cache. (%s %d)\n",
                            mdb[i].host_name, __FILE__, __LINE__);
                  mdb[i].fsa_pos = -1; /* So remove_files() does NOT write to FSA! */
                  remove_flag = YES;
               }
               else
               {
#ifdef _DEBUG
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Hmm. Host position for %s is incorrect!? Correcting %d->%d (%s %d)\n",
                            mdb[i].host_name, mdb[i].fsa_pos,
                            new_pos, __FILE__, __LINE__);
#endif /* _DEBUG */
                  mdb[i].fsa_pos = new_pos;
               }
            }

            if (remove_flag == YES)
            {
               (void)sprintf(p_msg_dir, "%d", mdb[i].job_id);
               if (remove_job(i, jd_fd, jd, mdb[i].job_id) == SUCCESS)
               {
                  i--;
               }
               *p_msg_dir = '\0';
            }
         } /* for (i = 0; i < *no_msg_cached; i++) */

         free(ck_ml);
      }

      if (removed_jobs > 0)
      {
         lock_region_w(jd_fd, 1);
         sort_array(0, removed_jobs - 1);

         /*
          * Always ensure that the JID structure size did NOT
          * change, else we might be moving things that do not
          * belong to us!
          */
         if (((*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET) > jid_struct_size)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmmmm. Size of %s is %d Bytes, but calculation says it should be %d Bytes! (%s %d)\n",
                      JOB_ID_DATA_FILE, jid_struct_size,
                      (*no_of_job_ids * sizeof(struct job_id_data)) + AFD_WORD_OFFSET,
                      __FILE__, __LINE__);
            ptr = (char *)jd - AFD_WORD_OFFSET;
            if (munmap(ptr, jid_struct_size) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "munmap() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
            if (fstat(jd_fd, &stat_buf) == -1)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to fstat() %s : %s (%s %d)\n",
                         job_id_data_file, strerror(errno),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (stat_buf.st_size > 0)
            {
               if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Failed to mmap() to %s : %s (%s %d)\n",
                            job_id_data_file, strerror(errno),
                            __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               no_of_job_ids = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               jd = (struct job_id_data *)ptr;
               jid_struct_size = stat_buf.st_size;
            }
            else
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "File %s is empty! Terminating, don't know what to do :-( (%s %d)\n",
                         job_id_data_file, __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }

         for (i = 0; i < removed_jobs; i++)
         {
            int end_pos = i,
                j,
                jobs_deleted,
                start_pos = i;

            while ((++end_pos < removed_jobs) &&
                   ((rjl[end_pos - 1] + 1) == rjl[end_pos]))
            {
               /* Nothing to be done. */;
            }
            if (rjl[end_pos - 1] != (*no_of_job_ids - 1))
            {
               size_t move_size = (*no_of_job_ids - (rjl[end_pos - 1] + 1)) *
                                  sizeof(struct job_id_data);

#ifdef _DEBUG
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "no_of_job_ids = %d|struct size = %d|move_size = %d|&jd[%d] = %d|&jd[%d] = %d (%s %d)\n",
                         *no_of_job_ids, sizeof(struct job_id_data),
                         move_size, rjl[i], &jd[rjl[i]],
                         rjl[end_pos - 1] + 1, &jd[rjl[end_pos - 1] + 1],
                         __FILE__, __LINE__);
#endif /* _DEBUG */
               (void)memmove(&jd[rjl[i]], &jd[rjl[end_pos - 1] + 1], move_size);
            }
            jobs_deleted = end_pos - i;
            (*no_of_job_ids) -= jobs_deleted;
            i += (end_pos - i - 1);
            for (j = (i + 1); j < removed_jobs; j++)
            {
               if (rjl[j] > rjl[start_pos])
               {
                  rjl[j] -= jobs_deleted;
               }
            }
         } /* for (i = 0; i < removed_jobs; i++) */
         unlock_region(jd_fd, 1);
#ifdef _DEBUG
         (void)rec(sys_log_fd, DEBUG_SIGN, "no_of_job_ids = %d (%s %d)\n",
                   *no_of_job_ids, __FILE__, __LINE__);
#endif /* _DEBUG */
         free(rjl);
      }
   }
#endif /* _OUTPUT_LOG */

   /* Don't forget to unmap from job_id_data structure. */
   ptr = (char *)jd - AFD_WORD_OFFSET;
   if (msync(ptr, jid_struct_size, MS_ASYNC) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "msync() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (munmap(ptr, jid_struct_size) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (close(jd_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   if (removed_messages > 0)
   {
      size_t length = 0;
      char   buffer[80];

      (void)rec(sys_log_fd, INFO_SIGN,
                "Removed %d old message(s).\n", removed_messages);

      /* Show which messages have been removed. */
      for (i = 0; i < removed_messages; i++)
      {
         length += sprintf(&buffer[length], "%d ", rml[i]);
         if (length > 51)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "%s\n", buffer);
            length = 0;
         }
      }
      if (length != 0)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "%s\n", buffer);
      }
      free(rml);
   }

   return;
}


/*++++++++++++++++++++++++ read_current_msg_list() ++++++++++++++++++++++*/
static void
read_current_msg_list(int **cml, int *no_of_current_msg)
{
   int    fd;
   size_t size;
   char   current_msg_list_file[MAX_PATH_LENGTH];

   (void)strcpy(current_msg_list_file, p_work_dir);
   (void)strcat(current_msg_list_file, FIFO_DIR);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);
   if ((fd = open(current_msg_list_file, O_RDWR)) == -1) /* WR for locking */
   {
      if (errno == ENOENT)
      {
         int sleep_counter = 0;

         do
         {
            (void)sleep(1);
            errno = 0;
            sleep_counter++;
            if (((fd = open(current_msg_list_file, O_RDWR)) == -1) &&
                ((errno != ENOENT) || (sleep_counter > 10)))
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         current_msg_list_file, strerror(errno),
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
         } while (errno == ENOENT);
      }
      else
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   current_msg_list_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   lock_region_w(fd, 0);
   if (read(fd, no_of_current_msg, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to read() from %s [%d] : %s (%s %d)\n",
                current_msg_list_file, fd,
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   size = *no_of_current_msg * sizeof(int);
   if ((*cml = malloc(size)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (read(fd, *cml, size) != size)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to read() from %s : %s (%s %d)\n",
                current_msg_list_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*++++++++++++++++++++++++++++ remove_job() +++++++++++++++++++++++++++++*/
static int
remove_job(int                cache_pos,
           int                jd_fd,
           struct job_id_data *jd,
           unsigned int       job_id)
{
   int dir_id_pos = -1,
       j,
       removed_job_pos = -1;

   /*
    * Before we remove anything, make sure that this job is NOT
    * in the queue and sending data.
    */
   for (j = 0; j < *no_msg_queued; j++)
   {
      if (qb[j].pos == cache_pos)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Job %u is currently in the queue! (%s %d)\n",
                   job_id, __FILE__, __LINE__);

         if (qb[j].pid > 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "AND process %d is currently distributing files! Will terminate this process.\n",
                      qb[j].pid);
            if (kill(qb[j].pid, SIGKILL) < 0)
            {
               if (errno != ESRCH)
               {
                  char *p_host_alias;

                  if (qb[j].msg_name[0] == '\0')
                  {
                     p_host_alias = fra[qb[j].pos].host_alias;
                  }
                  else
                  {
                     p_host_alias = mdb[qb[j].pos].host_name;
                  }
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to kill transfer job to %s (%d) : %s (%s %d)\n",
                            p_host_alias, qb[j].pid,
                            strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         /*
          * NOOOO. There may not be any message in the queue.
          * Remove it if there is one.
          */
         if (qb[j].msg_name[0] != '\0')
         {
            char *ptr,
                 *p_job_dir;

            if (qb[j].in_error_dir == YES)
            {
               p_job_dir = err_file_dir;
               ptr = p_err_file_dir;
               (void)sprintf(p_err_file_dir, "%s/%s",
                             mdb[qb[j].pos].host_name, qb[j].msg_name);
            }
            else
            {
               p_job_dir = file_dir;
               ptr = p_file_dir;
               (void)strcpy(p_file_dir, qb[j].msg_name);
            }
#ifdef _DELETE_LOG
            remove_files(p_job_dir, mdb[qb[j].pos].fsa_pos,
                         mdb[qb[j].pos].job_id, USER_DEL);
#else
            remove_files(p_job_dir, -1);
#endif
            *ptr = '\0';
         }
         remove_msg(j);
         j--;
      }
   }

   if (cache_pos < *no_msg_cached)
   {
      /* Remove it from job_id_data structure. */
      for (j = 0; j < *no_of_job_ids; j++)
      {
         if (jd[j].job_id == mdb[cache_pos].job_id)
         {
            /*
             * Before we remove it, lets remember the
             * directory ID from where it came from.
             * So we can remove any stale directory
             * entries in the dir_name_buf structure.
             */
            dir_id_pos = jd[j].dir_id_pos;

            if ((removed_jobs % 10) == 0)
            {
               size_t new_size = ((removed_jobs / 10) + 1) * 10 * sizeof(int);

               if (removed_jobs == 0)
               {
                  if ((rjl = malloc(new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Could not malloc() memory : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               else
               {
                  if ((rjl = realloc(rjl, new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Could not realloc() memory : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
            }
            removed_job_pos = rjl[removed_jobs] = j;
            removed_jobs++;
            break;
         }
      }
      if (dir_id_pos != -1)
      {
         /*
          * Go through job list and make sure no other job
          * has the same dir_id_pos, ie is using this directory
          * as its source directory.
          */
         for (j = 0; j < *no_of_job_ids; j++)
         {
            if ((removed_job_pos != j) && (dir_id_pos == jd[j].dir_id_pos))
            {
               dir_id_pos = -1;
               break;
            }
         }
      }
   } /* if (cache_pos < *no_msg_cached) */
   else
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "How can this be?! cache_pos [%d] is not less then no_msg_cached [%d]. (%s %d)\n",
                cache_pos, *no_msg_cached, __FILE__, __LINE__);
   }

   /* Remove message from message directory. */
   if (unlink(msg_dir) == -1)
   {
      if (errno != ENOENT)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to unlink() %s : %s (%s %d)\n",
                   msg_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      if ((removed_messages % 10) == 0)
      {
         size_t new_size = ((removed_messages / 10) + 1) * 10 * sizeof(int);

         if (removed_messages == 0)
         {
            if ((rml = malloc(new_size)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Could not malloc() memory : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else
         {
            if ((rml = realloc(rml, new_size)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Could not realloc() memory : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
      }
      rml[removed_messages] = job_id;
      removed_messages++;
   }

   /*
    * Only remove from cache if it has the same job_id.
    * When there is no data in the fifodir but the
    * messages are still there, we would delete the
    * current message cache.
    */
   if ((cache_pos != *no_msg_cached) &&
       (mdb[cache_pos].job_id == job_id) &&
       (mdb[cache_pos].in_current_fsa != YES))
   {
      /* Remove message from msg_cache_buf structure. */
      if (cache_pos != (*no_msg_cached - 1))
      {
         int    j;
         size_t move_size = (*no_msg_cached - 1 - cache_pos) *
                            sizeof(struct msg_cache_buf);

         /*
          * The position in struct queue_buf is no longer
          * correct. Update all positions that are larger
          * then i.
          */
         for (j = 0; j < *no_msg_queued; j++)
         {
            if (qb[j].pos > cache_pos)
            {
               qb[j].pos--;
            }
            else if ((qb[j].pos == cache_pos) && (qb[j].pid < 1))
                 {
                    /*
                     * NOOOO. There may not be any message in the queue.
                     * Remove it if there is one.
                     */
                    if (qb[j].msg_name[0] != '\0')
                    {
                       char *ptr,
                            *p_job_dir;

                       if (qb[j].in_error_dir == YES)
                       {
                          p_job_dir = err_file_dir;
                          ptr = p_err_file_dir;
                          (void)sprintf(p_err_file_dir, "%s/%s",
                                        mdb[qb[j].pos].host_name,
                                        qb[j].msg_name);
                       }
                       else
                       {
                          p_job_dir = file_dir;
                          ptr = p_file_dir;
                          (void)strcpy(p_file_dir, qb[j].msg_name);
                       }
#ifdef _DELETE_LOG
                       remove_files(p_job_dir, mdb[qb[j].pos].fsa_pos,
                                    mdb[qb[j].pos].job_id, USER_DEL);
#else
                       remove_files(p_job_dir, -1);
#endif
                       *ptr = '\0';
                    }
                    remove_msg(j);
                    j--;
                 }
         } /* for (j = 0; j < *no_msg_queued; j++) */

         /*
          * Remove message data from cache.
          */
         (void)memmove(&mdb[cache_pos], &mdb[cache_pos + 1], move_size);
      }
      else
      {
         /*
          * Don't forget to throw away any messages in the queue
          * for which we want to remove the data in the cache.
          */
         for (j = 0; j < *no_msg_queued; j++)
         {
            if ((qb[j].pos == cache_pos) && (qb[j].pid < 1))
            {
               /*
                * NOOOO. There may not be any message in the queue.
                * Remove it if there is one.
                */
               if (qb[j].msg_name[0] != '\0')
               {
                  char *ptr,
                       *p_job_dir;

                  if (qb[j].in_error_dir == YES)
                  {
                     p_job_dir = err_file_dir;
                     ptr = p_err_file_dir;
                     (void)sprintf(p_err_file_dir, "%s/%s",
                                   mdb[qb[j].pos].host_name, qb[j].msg_name);
                  }
                  else
                  {
                     p_job_dir = file_dir;
                     ptr = p_file_dir;
                     (void)strcpy(p_file_dir, qb[j].msg_name);
                  }
#ifdef _DELETE_LOG
                  remove_files(p_job_dir, mdb[qb[j].pos].fsa_pos,
                               mdb[qb[j].pos].job_id, USER_DEL);
#else
                  remove_files(p_job_dir, -1);
#endif
                  *ptr = '\0';
               }
               remove_msg(j);
               j--;
            }
         }
      }
      (*no_msg_cached)--;
   }

   /*
    * If the directory is not used anymore, remove it
    * from the DIR_NAME_FILE database.
    */
   if (dir_id_pos != -1)
   {
      int         fd;
      char        file[MAX_PATH_LENGTH];
      struct stat stat_buf;

      (void)strcpy(file, p_work_dir);
      (void)strcat(file, FIFO_DIR);
      (void)strcat(file, DIR_NAME_FILE);
      if ((fd = open(file, O_RDWR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (fstat(fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         exit(INCORRECT);
      }

      if (stat_buf.st_size != 0)
      {
         int                 *no_of_dir_names;
         char                *ptr;
         struct dir_name_buf *dnb;

         if ((ptr = mmap(0, stat_buf.st_size,
                         (PROT_READ | PROT_WRITE),
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "mmap() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            exit(INCORRECT);
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;

         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Removing %s [%d] from dir_name_buf structure.\n",
                   dnb[dir_id_pos].dir_name, dnb[dir_id_pos].dir_id);
         if (dir_id_pos != (*no_of_dir_names - 1))
         {
            int    k;
            size_t move_size = (*no_of_dir_names - 1 - dir_id_pos) *
                               sizeof(struct dir_name_buf);

            (void)memmove(&dnb[dir_id_pos], &dnb[dir_id_pos + 1], move_size);

            /*
             * Correct position dir_id_pos in struct job_id_data.
             */
            lock_region_w(jd_fd, 1);
            for (k = 0; k < *no_of_job_ids; k++)
            {
               if ((removed_job_pos != k) &&
                   (jd[k].dir_id_pos > dir_id_pos))
               {
                  jd[k].dir_id_pos--;
               }
            }
            unlock_region(jd_fd, 1);
         }
         (*no_of_dir_names)--;

         ptr -= AFD_WORD_OFFSET;
         if (munmap(ptr, stat_buf.st_size) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "munmap() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      } /* if (stat_buf.st_size != 0) */
      
      if (close(fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   } /* if (dir_id_pos != -1) */

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ sort_array() ++++++++++++++++++++++++++++*/
static void
sort_array(int start, int end)
{
   int i, j,
       center,
       temp;

   i = start; j = end;
   center = rjl[(start + end)/2];
   do
   {
      while ((i < end) && (rjl[i] < center))
      {
         i++;
      }
      while ((j > start) && (rjl[j] > center))
      {
         j--;
      }
      if (i <= j)
      {
         temp = rjl[i];
         rjl[i] = rjl[j];
         rjl[j] = temp;
         i++; j--;
      }
   } while (i <= j);
   if (start < j)
   {
      sort_array(start, j);
   }
   if (i < end)
   {
      sort_array(i, end);
   }

   return;
}
