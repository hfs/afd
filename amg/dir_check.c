/*
 *  dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   dir_check - waits for files to appear in user directories to
 **               create a job for the FD
 **
 ** SYNOPSIS
 **   dir_check [--version]         - Show version.
 **             <work_dir>          - Working directory of AFD.
 **             <shared memory ID>  - ID of shared memory that contains
 **                                   all necessary data for instant
 **                                   jobs.
 **             <rescan time>       - The time interval when to check
 **                                   whether any directories have changed.
 **             <no of process>     - The maximum number that it may fork
 **                                   to copy files.
 **             <no_of_local_dirs>  - The number of 'user' directories
 **                                   specified in the DIR_CONFIG file
 **                                   and are local.
 **
 ** DESCRIPTION
 **   This program waits for files to appear in the user directory
 **   to create a job for the FD (File Distributor). A job always
 **   consists of a directory which holds all files to be send and
 **   a message which tells the FD what to to with the job.
 **
 **   If the user directory is not in the same file system as dir_check,
 **   it will fork to copy the files from the user directory to the
 **   local AFD directory. Thus slow copies will not slow down the
 **   process of generating new jobs for the FD. This is important
 **   when the user directory is mounted via NFS.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.10.1995 H.Kiehl Created
 **   02.01.1997 H.Kiehl Added logging of file names that are found by
 **                      by the AFD.
 **   01.02.1997 H.Kiehl Read renaming rule file.
 **   19.04.1997 H.Kiehl Add support for writing messages in one memory
 **                      mapped file.
 **   04.03.1999 H.Kiehl When copying files don't only limit the number
 **                      of files, also limit the size that may be copied.
 **   09.10.1999 H.Kiehl Added new feature of important directories.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), strcat(), strcmp(),      */
                                   /* memcpy(), strerror(), strlen()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit(), abort()                    */
#include <sys/types.h>
#include <sys/wait.h>              /* waitpid()                          */
#include <sys/stat.h>
#include <sys/time.h>              /* struct timeval                     */
#include <sys/ipc.h>
#include <sys/shm.h>               /* shmdt()                            */
#include <sys/mman.h>              /* munmap()                           */
#include <fcntl.h>
#include <unistd.h>                /* fork(), rmdir(), getuid(), getgid()*/
#include <signal.h>                /* signal()                           */
#include <errno.h>
#ifdef _WITH_PTHREAD
#include <pthread.h>
#endif
#include "amgdefs.h"
#include "version.h"


/* global variables */
int                        shm_id,         /* Shared memory ID of        */
                                           /* dir_check.                 */
                           fra_id,         /* ID of FRA.                 */
                           fra_fd = -1,    /* Needed by fra_attach()     */
                           fsa_id,         /* ID of FSA.                 */
                           fsa_fd = -1,    /* Needed by fsa_attach()     */
                           max_process = MAX_NO_OF_DIR_CHECKS,
                           msg_fifo_fd,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           no_of_process = 0,
                           *no_msg_buffered,
                           no_of_time_jobs,
                           mb_fd,
                           fd_cmd_fd,
                           max_copied_files,
                           one_dir_copy_timeout,
#ifndef _WITH_PTHREAD
                           dir_check_timeout,
#endif
                           no_of_jobs,
                           no_of_local_dirs,/* No. of directories in the */
                                            /* DIR_CONFIG file that are  */
                                            /* local.                    */
                           counter_fd,      /* File descriptor for AFD   */
                                            /* counter file.             */
                           write_fin_fd,
#ifdef _INPUT_LOG
                           il_fd,
#endif
                           no_of_rule_headers = 0,
                           amg_flag = YES,
                           receive_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,
                           *time_job_list = NULL;
size_t                     max_copied_file_size,
                           msg_fifo_buf_size;
#ifndef _NO_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
#ifdef _WITH_PTHREAD
pthread_mutex_t            fsa_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t                  *thread;
#else
off_t                      *file_size_pool;
#endif
uid_t                      afd_uid;
gid_t                      afd_gid;
char                       *p_dir_alias,
                           *p_work_dir,
                           *p_shm = NULL,
                           first_time = YES,
                           pause_flag = NO,
                           fin_fifo[MAX_PATH_LENGTH],
                           time_dir[MAX_PATH_LENGTH],
                           *p_time_dir,
#ifndef _WITH_PTHREAD
                           **file_name_pool,
#endif
                           afd_file_dir[MAX_PATH_LENGTH];
struct dc_proc_list        *dcpl;      /* Dir Check Process List */
struct directory_entry     *de;
struct instant_db          *db;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct afd_status          *p_afd_status;
struct rule                *rule;
struct message_buf         *mb;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
#ifdef _WITH_PTHREAD
struct data_t              *p_data;
#endif

/* local functions */
#ifdef _WITH_PTHREAD
static void                *do_one_dir(void *);
#endif
static void                sig_bus(int),
                           sig_segv(int);
static int                 zombie_check(int *, int),
                           check_fifo(int, int, time_t *, time_t *, int *),
#ifdef _WITH_PTHREAD
                           handle_dir(int, time_t, char *, int *, char *, off_t *, char **);
#else
                           handle_dir(int, time_t, char *, int *, char *);
#endif

/* #define _REPORT_DIR_CHECK_TIME 1 */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              del_time_job_fd,
                    i,
                    max_fd,               /* Largest file descriptor.    */
                    n,
#ifdef REPORT_DIR_TIME_INTERVAL
                    prev_diff_time = -1,
#endif /* REPORT_DIR_TIME_INTERVAL */
#ifdef _WITH_PTHREAD
                    rtn,
#endif
                    read_fd,
                    read_fin_fd,
                    status,
                    write_fd;
   time_t           fd_search_start_time,
                    next_time_check,
                    next_search_time,
#ifdef REPORT_DIR_TIME_INTERVAL
                    average_diff_time = 0L,
                    max_diff_time = 0L,
                    next_report_time,
#endif /* REPORT_DIR_TIME_INTERVAL */
#if defined (_REPORT_DIR_CHECK_TIME) || defined (REPORT_DIR_TIME_INTERVAL)
                    diff_time,
#endif
                    now,
                    rescan_time = DEFAULT_RESCAN_TIME,
                    read_rules_interval = 0L,
                    time_buf,
                    time_check_sleep_interval;
#ifdef REPORT_DIR_TIME_INTERVAL
   unsigned int     no_of_dir_searches = 0;
#endif /* REPORT_DIR_TIME_INTERVAL */
   char             buffer[MAX_FIFO_BUFFER],
#ifdef _FIFO_DEBUG
                    cmd[2],
#endif
#ifndef _WITH_PTHREAD
                    *p_paused_host,
#endif
                    rule_file[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   fd_set           rset;
#ifdef _WITH_PTHREAD
   void             *statusp;
#endif
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
   struct timeval   timeout;

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_dir_check(argc, argv, rule_file, &rescan_time, &read_fd,
                  &write_fd, &read_fin_fd, &del_time_job_fd);
   time_buf = rescan_time;

#ifdef SA_FULLDUMP
   /*
    * When dumping core ensure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "sigaction() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif

   if ((signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not set signal handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Find largest file descriptor */
   max_fd = del_time_job_fd;
   if (read_fd > max_fd)
   {
      max_fd = read_fd;
   }
   if (read_fin_fd > max_fd)
   {
      max_fd = read_fin_fd;
   }
   max_fd++;

   /* Set flag to indicate that the the dir_check is active. */
   if ((p_afd_status->amg_jobs & INST_JOB_ACTIVE) == 0)
   {
      p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
   }
   (void)time(&fd_search_start_time);

   next_time_check = (fd_search_start_time / TIME_CHECK_INTERVAL) *
                      TIME_CHECK_INTERVAL + TIME_CHECK_INTERVAL;
   next_search_time = (fd_search_start_time / OLD_FILE_SEARCH_INTERVAL) *
                      OLD_FILE_SEARCH_INTERVAL + OLD_FILE_SEARCH_INTERVAL;
#ifdef REPORT_DIR_TIME_INTERVAL
   next_report_time = (fd_search_start_time / REPORT_DIR_TIME_INTERVAL) *
                      REPORT_DIR_TIME_INTERVAL + REPORT_DIR_TIME_INTERVAL;
#endif /* REPORT_DIR_TIME_INTERVAL */

   /* Tell user we are starting dir_check. */
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting dir_check (PRE %d.%d.%d-%d)\n",
             MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting dir_check (%d.%d.%d)\n",
             MAJOR, MINOR, BUG_FIX);
#endif

   /*
    * The following loop checks all user directories for new
    * files to arrive. When we fork to copy files from directories
    * not in the same file system as the AFD, watch the read_fin_fd
    * to see when the child has done its job.
    */
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(read_fin_fd, &rset);
      FD_SET(read_fd, &rset);
      FD_SET(del_time_job_fd, &rset);
      timeout.tv_usec = 0;
      if (first_time == YES)
      {
         timeout.tv_sec = 0;
         first_time = NO;
      }
      else
      {
         if ((time_check_sleep_interval = (next_time_check - time(NULL))) < 0)
         {
            time_check_sleep_interval = 0;
         }
         if (time_check_sleep_interval > time_buf)
         {
            timeout.tv_sec = time_buf;
         }
         else
         {
            timeout.tv_sec = time_check_sleep_interval;
         }
      }

      /* Wait for message x seconds and then continue. */
      status = select(max_fd, &rset, NULL, NULL, &timeout);

      if (time(&now) >= next_time_check)
      {
         handle_time_jobs(&no_of_process, afd_file_dir);
         next_time_check = (time(&now) / TIME_CHECK_INTERVAL) *
                            TIME_CHECK_INTERVAL + TIME_CHECK_INTERVAL;
      }
#ifdef REPORT_DIR_TIME_INTERVAL
      if (now >= next_report_time)
      {
#ifdef MAX_DIFF_TIME
         if (max_diff_time > MAX_DIFF_TIME)
         {
#endif /* MAX_DIFF_TIME */
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Directory search times for %d dirs AVG: %ld MAX: %ld SEARCHES: %u\n",
                      no_of_local_dirs, average_diff_time,
                      max_diff_time, no_of_dir_searches);
#ifdef MAX_DIFF_TIME
         }
#endif /* MAX_DIFF_TIME */
         average_diff_time = max_diff_time = 0L;
         no_of_dir_searches = 0;
         next_report_time = (now / REPORT_DIR_TIME_INTERVAL) *
                            REPORT_DIR_TIME_INTERVAL +
                            REPORT_DIR_TIME_INTERVAL;
      }
#endif /* REPORT_DIR_TIME_INTERVAL */


      if ((status > 0) && (FD_ISSET(read_fd, &rset)))
      {
         (void)check_fifo(read_fd, write_fd, &time_buf,
                          &rescan_time, &no_of_jobs);
      }
      else if ((status == 0) || (FD_ISSET(read_fin_fd, &rset)))
           {
              if (check_fsa() == YES)
              {
                 /*
                  * When edit_hc changes the order in the FSA it will also
                  * have to change it. Since the database of this program
                  * depends on the FSA we have reread the shared memory
                  * section. There should be no change such as a new host
                  * or a new directory entry.
                  */
                 if (create_db(shm_id) != no_of_jobs)
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN,
                              "Unexpected change in database! Terminating. (%s %d)\n",
                              __FILE__, __LINE__);
                    exit(INCORRECT);
                 }
              }

              if (FD_ISSET(read_fin_fd, &rset))
              {
                 if ((n = read(read_fin_fd, buffer, MAX_FIFO_BUFFER)) > 0)
                 {
#ifdef _FIFO_DEBUG
                    show_fifo_data('R', "ip_fin", buffer, n, __FILE__, __LINE__);
#endif
                    (void)zombie_check(&no_of_process, no_of_jobs);
                 }
              }
              else
              {
                 read_rules_interval += time_buf;
                 if (read_rules_interval > READ_RULES_INTERVAL)
                 {
                    read_rules_interval = 0;
                    get_rename_rules(rule_file);
                 }
              }


              /*
               * Check if FD is currently performing a directory
               * check of the file directory, to see if there are
               * any jobs without messages. This can only be done
               * safely when dir_check() is not currently inserting
               * new jobs.
               */
              if ((pause_flag == NO) &&
                  ((p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE) == 0))
              {
#ifndef _WITH_PTHREAD
                 time_t      dir_check_time,
                             start_dir_time,
                             start_time;
#endif
                 struct stat dir_stat_buf;

                 /* Set flag to indicate that the the dir_check is active. */
                 if ((p_afd_status->amg_jobs & INST_JOB_ACTIVE) == 0)
                 {
                    p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
                 }

                 /*
                  * If there are messages in the queue, check if we
                  * can pass them to the FD. If we don't do it here
                  * the messages will be stuck in the queue until
                  * a new file enters the system.
                  */
                 if ((p_afd_status->fd == ON) && (*no_msg_buffered > 0))
                 {
                    clear_msg_buffer();
                 }

#ifdef _WITH_PTHREAD
                 /*
                  * Create a threat for each directory we have to read.
                  * Lets use the peer model.
                  */
                 for (i = 0; i < no_of_local_dirs; i++)
                 {
                    if ((fra[de[i].fra_pos].dir_status != DISABLED) &&
                        ((fra[de[i].fra_pos].fsa_pos != -1) ||
                         (fra[de[i].fra_pos].time_option == NO) ||
                         (fra[de[i].fra_pos].next_check_time <= start_time)))
                    {
                       if ((rtn = pthread_create(&thread[i], NULL,
                                                 do_one_dir,
                                                 (void *)&p_data[i])) != 0)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    "pthread_create() error : %s (%s %d)\n",
                                    strerror(rtn), __FILE__, __LINE__);
                       }
                    }
                    else
                    {
                       thread[i] = NULL;
                    }
                 }

                 for (i = 0; i < no_of_local_dirs; i++)
                 {
                    if (tread[i] != NULL)
                    {
                       int j;

                       if ((rtn = pthread_join(thread[i], &statusp)) != 0)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    "pthread_join() error : %s (%s %d)\n",
                                    strerror(rtn), __FILE__, __LINE__);
                       }
                       if (statusp == PTHREAD_CANCELED)
                       {
                          (void)rec(sys_log_fd, INFO_SIGN,
                                    "Thread has been cancelled. (%s %d)\n",
                                    __FILE__, __LINE__);
                       }
                       for (j = 0; j < max_copied_files; j++)
                       {
                          p_data[i].file_name_pool[j][0] = '\0';
                       }

                       if ((fra[de[i].fra_pos].fsa_pos == -1) &&
                           (fra[de[i].fra_pos].time_option == YES))
                       {
                          fra[de[i].fra_pos].next_check_time = calc_next_time(&fra[de[i].fra_pos].te);
                       }
                    }
                 }

                 /* Check if any process is finished. */
                 if (no_of_process > 0)
                 {
                    (void)zombie_check(&no_of_process, no_of_jobs);
                 }

                 /*
                  * When starting and all directories are full with
                  * files, it will take far too long before the dir_check
                  * checks if it has to stop. So lets check the fifo
                  * every time we have checked a directory.
                  */
                 if (check_fifo(read_fd, write_fd, &time_buf,
                                &rescan_time, &no_of_jobs) > 0)
                 {
                    break;
                 }
#else
                 /*
                  * Since it can take very long until we hace travelled
                  * through all directories lets always check the time
                  * and ensure we do not take too long.
                  */
                 start_time = dir_check_time = time(&start_dir_time);
                 for (i = 0; i < no_of_local_dirs; i++)
                 {
                    if ((fra[de[i].fra_pos].dir_status != DISABLED) &&
                        ((fra[de[i].fra_pos].fsa_pos != -1) ||
                         (fra[de[i].fra_pos].time_option == NO) ||
                         (fra[de[i].fra_pos].next_check_time <= start_time)))
                    {
                       start_time = time(NULL);

                       if (stat(de[i].dir, &dir_stat_buf) < 0)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    "Can't access directory %s : %s (%s %d)\n",
                                    de[i].dir, strerror(errno),
                                    __FILE__, __LINE__);
                       }
                       else
                       {
                          /*
                           * Handle any new files that have arrived.
                           */
                          if ((fra[de[i].fra_pos].force_reread == YES) ||
                              (dir_stat_buf.st_mtime >= de[i].search_time))
                          {
                             now = start_time;

                             /* The directory time has changed. New files */
                             /* have arrived!                             */
                             while (handle_dir(i, now, NULL, &no_of_process,
                                               afd_file_dir) == YES)
                             {
                                (void)time(&now);
                                if ((now - dir_check_time) > dir_check_timeout)
                                {
                                   int ii;

                                   for (ii = 0; ii < no_of_local_dirs; ii++)
                                   {
                                       if (fra[de[ii].fra_pos].important_dir == YES)
                                       {
                                          if (handle_dir(ii, now, NULL,
                                                         &no_of_process,
                                                         afd_file_dir) == YES)
                                          {
                                             first_time = YES;
                                          }
                                       }
                                   }
                                   now = time(&dir_check_time);
                                }

                                /*
                                 * There are still files left in the directory
                                 * lets continue handling files until we reach
                                 * some timeout.
                                 */
                                if ((now - start_time) > one_dir_copy_timeout)
                                {
                                   /*
                                    * Since there are still files left in the
                                    * directory lets manipulate the variable
                                    * first_time so we rescan the directories
                                    * immediately.
                                    */
                                   first_time = YES;
                                   break;
                                }
                             }
                          }

                          /* Check if any process is finished. */
                          if (no_of_process > 0)
                          {
                             (void)zombie_check(&no_of_process, no_of_jobs);
                          }

                          /*
                           * Handle any paused hosts in this directory.
                           */
                          if (dir_stat_buf.st_nlink > 2)
                          {
                             int dest_count = 0,
                                 nfg = 0;

                             while ((p_paused_host = check_paused_dir(&de[i],
                                                                      &nfg,
                                                                      &dest_count)) != NULL)
                             {
                                now = time(NULL);
                                while (handle_dir(i, now, p_paused_host,
                                                  &no_of_process,
                                                  afd_file_dir) == YES)
                                {
                                   /*
                                    * There are still files left in the directory
                                    * lets continue handling files until we reach
                                    * some timeout.
                                    */
                                   if ((time(&now) - start_time) > one_dir_copy_timeout)
                                   {
                                      /*
                                       * Since there are still files left in the
                                       * directory lets manipulate the variable
                                       * first_time so we rescan the directories
                                       * immediately.
                                       */
                                      first_time = YES;
                                      break;
                                   }
                                }
                             }
                          }
                       }

                       /* Check if any process is finished. */
                       if (no_of_process > 0)
                       {
                          (void)zombie_check(&no_of_process, no_of_jobs);
                       }

                       /*
                        * When starting and all directories are full with
                        * files, it will take far too long before the dir_check
                        * checks if it has to stop. So lets check the fifo
                        * every time we have checked a directory.
                        */
                       if (check_fifo(read_fd, write_fd, &time_buf,
                                      &rescan_time, &no_of_jobs) > 0)
                       {
                          break;
                       }

                       if ((fra[de[i].fra_pos].fsa_pos == -1) &&
                           (fra[de[i].fra_pos].time_option == YES))
                       {
                          fra[de[i].fra_pos].next_check_time = calc_next_time(&fra[de[i].fra_pos].te);
                       }
                    }
                 } /* for (i = 0; i < no_of_local_dirs; i++) */

#if defined (_REPORT_DIR_CHECK_TIME) || defined (REPORT_DIR_TIME_INTERVAL)
                 (void)time(&now);
                 diff_time = now - start_dir_time;
#endif
#ifdef _REPORT_DIR_CHECK_TIME
                 if (diff_time > 60)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "Took %ld seconds to check %d directories. (%s %d)\n",
                              diff_time, no_of_local_dirs,
                              __FILE__, __LINE__);
                 }
#endif /* _REPORT_DIR_CHECK_TIME */
#ifdef REPORT_DIR_TIME_INTERVAL
                 if (diff_time > max_diff_time)
                 {
                    max_diff_time = diff_time;
                 }
                 if (prev_diff_time == -1)
                 {
                    average_diff_time = diff_time;
                 }
                 else
                 {
                    average_diff_time = (prev_diff_time + diff_time) / 2;
                 }
                 prev_diff_time = diff_time;
                 no_of_dir_searches++;
#endif /* REPORT_DIR_TIME_INTERVAL */
#endif /* _WITH_PTHREAD */
              }
              else
              {
                 if (p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE)
                 {
                    /*
                     * NOTE: We may only indicate that dir_check is NOT
                     *       active when none of its children are active!
                     *       Else the function check_file_dir() from
                     *       the FD will generate a new job from it.
                     */
                    if (no_of_process == 0)
                    {
                       /*
                        * Show FD that we are waiting for it to finish it's
                        * file directory check by deactivating the
                        * INST_JOB_ACTIVE flag. Remember the time when the
                        * flag has been deactivated, otherwise when the FD
                        * crashes we will wait forever!
                        */
                       if (p_afd_status->amg_jobs & INST_JOB_ACTIVE)
                       {
                          p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
                          (void)time(&fd_search_start_time);
                       }
                    }
                 }
                 else
                 {
                    if (((p_afd_status->amg_jobs & INST_JOB_ACTIVE) == 0) &&
                        ((time(NULL) - fd_search_start_time) > 30))
                    {
                       (void)rec(sys_log_fd, WARN_SIGN,
                                 "Reseting FD_DIR_CHECK_ACTIVE flag due to timeout! (%s %d)\n",
                                 __FILE__, __LINE__);
                       p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
                       p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
                    }

                    /* While pausing the AMG process does not answer */
                    if ((pause_flag != NO) && (status == 0))
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Have waited %d sec and have received no reply from AMG, so I die! (%s %d)\n",
                                 (int)time_buf, __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                 }
              }
           }
      else if ((status > 0) && (FD_ISSET(del_time_job_fd, &rset)))
           {
              /*
               * User disabled a a host, all time jobs must be removed
               * for this host.
               */
              if ((n = read(del_time_job_fd, buffer, MAX_FIFO_BUFFER)) > 0)
              {
                 int  bytes_done = 0;
                 char *p_host_name = buffer;

                 do
                 {
                    for (i = 0; i < no_of_time_jobs; i++)
                    {
                       if (strcmp(p_host_name, db[time_job_list[i]].host_alias) == 0)
                       {
                          (void)strcpy(p_time_dir,
                                       db[time_job_list[i]].str_job_id);
                          remove_time_dir(p_host_name,
                                          db[time_job_list[i]].job_id,
                                          USER_DEL);
                          *p_time_dir = '\0';
                       }
                    }

                    while ((*p_host_name != '\0') && (bytes_done < n))
                    {
                       p_host_name++;
                       bytes_done++;
                    }
                    if ((*p_host_name == '\0') && (bytes_done < n))
                    {
                       p_host_name++;
                       bytes_done++;
                    }
                 } while (n > bytes_done);
              }
           }
           else /* ERROR */
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "select() error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }

      if (time(&now) > next_search_time)
      {
         search_old_files();
         next_search_time = (now / OLD_FILE_SEARCH_INTERVAL) *
                            OLD_FILE_SEARCH_INTERVAL +
                            OLD_FILE_SEARCH_INTERVAL;
      }

      /* Check if any process is finished. */
      if (no_of_process > 0)
      {
         (void)zombie_check(&no_of_process, no_of_jobs);
      }
   } /* for (;;) */

   /* free memory for pid array */
   free(dcpl);
   for (i = 0; i < no_of_local_dirs; i++)
   {
      int j;

      for (j = 0; j < de[i].nfg; j++)
      {
         free(de[i].fme[j].pos);
         free(de[i].fme[j].file_mask);
      }
      free(de[i].fme);
   }
   free(de);

   exit(SUCCESS);
}


#ifdef _WITH_PTHREAD
/*++++++++++++++++++++++++++++ do_one_dir() +++++++++++++++++++++++++++++*/
void *
do_one_dir(void *arg)
{
   time_t        now,
                 start_time;
   char          *p_paused_host;
   struct data_t *data = (struct data_t *)arg;
   struct stat   dir_stat_buf;

   if (stat(de[data->i].dir, &dir_stat_buf) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                de[data->i].dir, strerror(errno),
                __FILE__, __LINE__);
      return(NO);
   }

   start_time = time(now);

   /*
    * Handle any new files that have arrived.
    */
   if ((fra[de[data->i].fra_pos].force_reread == YES) ||
       (dir_stat_buf.st_mtime >= de[data->i].search_time))
   {
      /* The directory time has changed. New files */
      /* have arrived!                             */
      while (handle_dir(data->i,
                        now,
                        NULL,
                        &no_of_process,
                        data->afd_file_dir,
                        data->file_size_pool,
                        data->file_name_pool) == YES)
      {
         /*
          * There are still files left in the directory
          * lets continue handling files until we reach
          * some timeout.
          */
         if ((time(&now) - start_time) > one_dir_copy_timeout)
         {
            /*
             * Since there are still files left in the
             * directory lets manipulate the variable
             * first_time so we rescan the directories
             * immediately.
             */
            first_time = YES;
            break;
         }
      }
   }

   /*
    * Handle any paused hosts in this directory.
    */
   if (dir_stat_buf.st_nlink > 2)
   {
      int dest_count = 0,
          nfg = 0;

      if ((p_paused_host = check_paused_dir(&de[data->i],
                                            &nfg,
                                            &dest_count)) != NULL)
      {
         now = time(NULL);
         while (handle_dir(data->i,
                           now,
                           p_paused_host,
                           &no_of_process,
                           data->afd_file_dir,
                           data->file_size_pool,
                           data->file_name_pool) == YES)
         {
            /*
             * There are still files left in the directory
             * lets continue handling files until we reach
             * some timeout.
             */
            if ((time(&now) - start_time) > one_dir_copy_timeout)
            {
               /*
                * Since there are still files left in the
                * directory lets manipulate the variable
                * first_time so we rescan the directories
                * immediately.
                */
               first_time = YES;
               break;
            }
         }
      }
   }

   return(NULL);
}
#endif


/*+++++++++++++++++++++++++++ handle_dir() ++++++++++++++++++++++++++++++*/
static int
#ifdef _WITH_PTHREAD
handle_dir(int    dir_no,
           time_t current_time,
           char   *host_name,
           int    *no_of_process,
           char   *afd_file_dir,
           off_t  *file_size_pool,
           char   **file_name_pool)
#else
handle_dir(int   dir_no,
           time_t current_time,
           char  *host_name,
           int   *no_of_process,
           char  *afd_file_dir)
#endif
{
   if ((*no_of_process < max_process) &&
       (fra[de[dir_no].fra_pos].no_of_process < fra[de[dir_no].fra_pos].max_process))
   {
      int        j,
                 k,
                 files_moved,
                 files_linked;
      off_t      file_size_linked;
      size_t     total_file_size;
      char       orig_file_path[MAX_PATH_LENGTH],
                 src_file_dir[MAX_PATH_LENGTH],
                 unique_name[MAX_FILENAME_LENGTH];
#ifdef _WITH_PTHREAD
                 time_dir[MAX_PATH_LENGTH],
                 *p_time_dir,
#endif
#ifdef _DEBUG_THREAD
      char       debug_file[MAX_FILENAME_LENGTH];
      FILE       *p_debug_file;

      (void)sprintf(debug_file, "thread-debug.%d", (int)&thread[dir_no]);
#endif

#ifdef _WITH_PTHREAD
      (void)strcpy(time_dir, afd_file_dir);
      (void)strcat(time_dir, AFD_TIME_DIR);
      (void)strcat(time_dir, "/");
      p_time_dir = time_dir + strlen(time_dir);
#endif
      total_file_size = 0;
      (void)strcpy(src_file_dir, de[dir_no].dir);

      if (host_name == NULL)
      {
         de[dir_no].search_time = time(NULL);
      }
      else
      {
         (void)strcat(src_file_dir, "/.");
         (void)strcat(src_file_dir, host_name);
      }
      p_dir_alias = de[dir_no].alias;

      fra[de[dir_no].fra_pos].dir_status = DIRECTORY_ACTIVE;

      if ((files_moved = check_files(&de[dir_no],
                                     src_file_dir,
                                     afd_file_dir,
                                     orig_file_path,
#ifdef _WITH_PTHREAD
                                     file_size_pool,
                                     file_name_pool,
#endif
                                     &total_file_size)) > 0)
      {
         time_t         creation_time;
         unsigned short unique_number;

#ifdef _DEBUG_THREAD
         if ((p_debug_file = fopen(debug_file, "a")) == NULL)
         {
            (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                          debug_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         (void)fprintf(p_debug_file, "%d %s\n", current_time, de[dir_no].dir);
         (void)fprintf(p_debug_file, "<%s> <%s> <%s> <%s>\n", file_name_pool[0], file_name_pool[1], file_name_pool[2], file_name_pool[3]);
         (void)fprintf(p_debug_file, "files_moved = %d\n", files_moved);

         (void)fclose(p_debug_file);
#endif
         for (j = 0; j < de[dir_no].nfg; j++)
         {
            for (k = 0; k < de[dir_no].fme[j].dest_count; k++)
            {
               if ((host_name == NULL) ||
                   (strcmp(host_name, db[de[dir_no].fme[j].pos[k]].host_alias) == 0))
               {
                  if ((fsa[db[de[dir_no].fme[j].pos[k]].position].host_status < 2) &&
                      ((fsa[db[de[dir_no].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0))
                  {
                     if ((db[de[dir_no].fme[j].pos[k]].time_option_type == NO_TIME) ||
                         ((db[de[dir_no].fme[j].pos[k]].time_option_type == SEND_COLLECT_TIME) &&
                          (db[de[dir_no].fme[j].pos[k]].next_start_time <= current_time)) ||
                         ((db[de[dir_no].fme[j].pos[k]].time_option_type == SEND_NO_COLLECT_TIME) &&
                          (in_time(current_time, &db[de[dir_no].fme[j].pos[k]].te) == YES)))
                     {
                        if ((files_linked = link_files(orig_file_path,
                                                       afd_file_dir,
#ifdef _WITH_PTHREAD
                                                       file_size_pool,
                                                       file_name_pool,
#endif
                                                       &de[dir_no],
                                                       &db[de[dir_no].fme[j].pos[k]],
                                                       &creation_time,
                                                       &unique_number,
                                                       j,
                                                       files_moved,
                                                       unique_name,
                                                       &file_size_linked)) > 0)
                        {
#ifndef _WITH_PTHREAD
                           if ((db[de[dir_no].fme[j].pos[k]].lfs & GO_PARALLEL) &&
                               (*no_of_process < max_process) &&
                               (fsa[db[de[dir_no].fme[j].pos[k]].position].host_status < 2) &&
                               ((fsa[db[de[dir_no].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0))
                           {
                              switch(dcpl[*no_of_process].pid = fork())
                              {
                                 case -1 : /* ERROR, process creation not possible */

                                    (void)rec(sys_log_fd, ERROR_SIGN,
                                              "Could not fork() : %s (%s %d)\n",
                                              strerror(errno), __FILE__, __LINE__);

                                    /*
                                     * Exiting is not the right thing to do here!
                                     * Better just do what we would do if we do not
                                     * fork. This will make the handling of files
                                     * a lot slower, but better then exiting.
                                     */
                                    send_message(afd_file_dir,
                                                 unique_name,
                                                 unique_number,
                                                 creation_time,
                                                 de[dir_no].fme[j].pos[k],
                                                 files_linked,
                                                 file_size_linked);
                                    break;

                                 case  0 : /* Child process */

                                    if ((db[de[dir_no].fme[j].pos[k]].lfs & SPLIT_FILE_LIST) &&
                                        (files_linked > MAX_FILES_TO_PROCESS))
                                    {
                                       int            ii,
                                                      split_files_renamed,
                                                      loops = files_linked / MAX_FILES_TO_PROCESS;
                                       off_t          split_file_size_renamed;
                                       time_t         tmp_creation_time;
                                       unsigned short tmp_unique_number;
                                       char           tmp_unique_name[MAX_FILENAME_LENGTH],
                                                      src_file_path[MAX_PATH_LENGTH];

                                       (void)strcpy(src_file_path, afd_file_dir);
                                       (void)strcat(src_file_path, unique_name);
                                       (void)strcat(src_file_path, "/");

                                       /*
                                        * If there are lots of files in the directory,
                                        * it can take quit a while before any files get
                                        * distributed. So lets only do MAX_FILES_TO_PROCESS
                                        * at one time. The distribution of files is in
                                        * most cases what takes most the time. So always
                                        * try to start the distribution as early as
                                        * possible.
                                        */
                                       for (ii = 0; ii < loops; ii++)
                                       {
                                          if ((split_files_renamed = rename_files(src_file_path,
                                                                                  afd_file_dir,
                                                                                  &de[dir_no],
                                                                                  &db[de[dir_no].fme[j].pos[k]],
                                                                                  &tmp_creation_time,
                                                                                  &tmp_unique_number,
                                                                                  j,
                                                                                  MAX_FILES_TO_PROCESS,
                                                                                  tmp_unique_name,
                                                                                  &split_file_size_renamed,
                                                                                  ii * MAX_FILES_TO_PROCESS)) > 0)
                                          {
                                             send_message(afd_file_dir,
                                                          tmp_unique_name,
                                                          tmp_unique_number,
                                                          tmp_creation_time,
                                                          de[dir_no].fme[j].pos[k],
                                                          split_files_renamed,
                                                          split_file_size_renamed);
                                          } /* if (split_files_linked > 0) */

                                          file_size_linked -= split_file_size_renamed;
                                          files_linked -= split_files_renamed;
                                       } /* for (ii = 0; ii < loops; ii++) */

                                       if (files_linked > 0)
                                       {
                                          send_message(afd_file_dir,
                                                       unique_name,
                                                       unique_number,
                                                       creation_time,
                                                       de[dir_no].fme[j].pos[k],
                                                       files_linked,
                                                       file_size_linked);
                                       }
                                    }
                                    else
                                    {
                                       send_message(afd_file_dir,
                                                    unique_name,
                                                    unique_number,
                                                    creation_time,
                                                    de[dir_no].fme[j].pos[k],
                                                    files_linked,
                                                    file_size_linked);
                                    }

                                    /*
                                     * Tell parent process we have completed
                                     * the task.
                                     */
#ifdef _FIFO_DEBUG
                                    cmd[0] = ACKN; cmd[1] = '\0';
                                    show_fifo_data('W', "ip_fin", cmd, 1, __FILE__, __LINE__);
#endif
                                    if (send_cmd(ACKN, write_fin_fd) < 0)
                                    {
                                       (void)rec(sys_log_fd, ERROR_SIGN,
                                                 "Could not write to fifo %s : %s (%s %d)\n",
                                                 fin_fifo, strerror(errno),
                                                 __FILE__, __LINE__);
                                    }
                                    exit(SUCCESS);

                                 default : /* Parent process */

                                    dcpl[*no_of_process].fra_pos = de[dir_no].fra_pos;
                                    fra[de[dir_no].fra_pos].no_of_process++;
                                    (*no_of_process)++;
                                    break;
                              } /* switch() */
                           }
                           else
                           {
#endif
                              /*
                               * No need to fork() since files are in same
                               * file system and it is not necessary to split
                               * large number of files to smaller ones.
                               */
                              send_message(afd_file_dir,
                                           unique_name,
                                           unique_number,
                                           creation_time,
                                           de[dir_no].fme[j].pos[k],
                                           files_linked,
                                           file_size_linked);
#ifndef _WITH_PTHREAD
                           }
#endif
                        } /* if (files_linked > 0) */
                     }
                     else /* Oueue files since it they are to be sent later. */
                     {
                        if ((db[de[dir_no].fme[j].pos[k]].time_option_type == SEND_COLLECT_TIME) &&
                            ((fsa[db[de[dir_no].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0))
                        {
                           (void)strcpy(p_time_dir, db[de[dir_no].fme[j].pos[k]].str_job_id);
                           if (save_files(orig_file_path,
                                          time_dir,
#ifdef _WITH_PTHREAD
                                          file_name_pool,
#endif
                                          &de[dir_no],
                                          j,
                                          files_moved,
                                          db[de[dir_no].fme[j].pos[k]].lfs) < 0)
                           {
                              (void)rec(sys_log_fd, ERROR_SIGN,
                                        "Failed to queue files for host %s (%s %d)\n",
                                        db[de[dir_no].fme[j].pos[k]].host_alias,
                                        __FILE__, __LINE__);
                           }
                           *p_time_dir = '\0';
                        }
                     }
                  }
                  else /* Queue is stopped, so queue data. */
                  {
                     if ((fsa[db[de[dir_no].fme[j].pos[k]].position].special_flag & HOST_DISABLED) == 0)
                     {
                        /* Queue is paused for this host, so lets save the */
                        /* files somewhere snug and save.                  */
                        if (save_files(orig_file_path,
                                       db[de[dir_no].fme[j].pos[k]].paused_dir,
#ifdef _WITH_PTHREAD
                                       file_name_pool,
#endif
                                       &de[dir_no],
                                       j,
                                       files_moved,
                                       db[de[dir_no].fme[j].pos[k]].lfs) < 0)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "Failed to queue files for host %s (%s %d)\n",
                                     db[de[dir_no].fme[j].pos[k]].host_alias,
                                     __FILE__, __LINE__);
                        }
                     }
                  }
               }
            }
         }

         /* Time to remove all files in orig_file_path */
         if (rec_rmdir(orig_file_path) < 0)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to remove %s (%s %d)\n",
                      orig_file_path, __FILE__, __LINE__);
         }

         if (host_name == NULL)
         {
            /*
             * Save modification time so we know when
             * we last searched in this directory.
             */
            de[dir_no].mod_time = de[dir_no].search_time;
         }
      } /* if (files_moved > 0) */

      /* In case of an empty directory, remove it! */
      if (host_name != NULL)
      {
         if (rmdir(src_file_dir) == -1)
         {
            if ((errno != EEXIST) && (errno != ENOTEMPTY))
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to rmdir() %s : %s (%s %d)\n",
                         src_file_dir, strerror(errno),
                         __FILE__, __LINE__);
            }
         }
         else
         {
            /*
             * We have to return NO even if we have copied 'max_copied_files'
             * since there are no files left!
             */
            return(NO);
         }
      }

      if ((fra[de[dir_no].fra_pos].no_of_process == 0) &&
          (fra[de[dir_no].fra_pos].dir_status == DIRECTORY_ACTIVE) &&
          (fra[de[dir_no].fra_pos].dir_status != DISABLED))
      {
         fra[de[dir_no].fra_pos].dir_status = NORMAL_STATUS;
      }

      if ((files_moved >= max_copied_files) ||
          (total_file_size >= max_copied_file_size))
      {
         return(YES);
      }
      else
      {
         return(NO);
      }
   }
   else
   {
      return(NO);
   }
}


/*+++++++++++++++++++++++++++++ zombie_check() ++++++++++++++++++++++++++*/
/*
 * Description : Checks if any process is finished (zombie), if this
 *               is the case it is killed with waitpid().
 */
static int
zombie_check(int *no_of_process, int no_of_jobs)
{
   int   zombies_killed = 0,
         i,
         j = 0,
         status;

   for (i = 0; i < *no_of_process; i++)
   {
      /* Is process a zombie? */
      if (waitpid(dcpl[i].pid, &status, WNOHANG) > 0)
      {
         if (WIFEXITED(status))
         {
            switch(WEXITSTATUS(status))
            {
               case 0 : /* Ordinary end of process */
               case 1 : /* No files found */
                        break;

               default: /* Unknown error. */
                        (void)rec(sys_log_fd, ERROR_SIGN,
                                  "Unknown return status (%d) of process dir_check. (%s %d)\n",
                                  WEXITSTATUS(status), __FILE__, __LINE__);
                        break;
            }
         }
         else  if (WIFSIGNALED(status))
               {
                  /* abnormal termination */;
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Abnormal termination of process dir_check (%d) (%s %d)\n",
                            dcpl[i].pid, __FILE__, __LINE__);
               }
               else  if (WIFSTOPPED(status))
                     {
                        /* Child stopped */;
                        (void)rec(sys_log_fd, ERROR_SIGN,
                                  "Process dir_check (%d) has been put to sleep. (%s %d)\n",
                                  dcpl[i].pid, __FILE__, __LINE__);
                        continue;
                     }

         /* update table */
         fra[dcpl[i].fra_pos].no_of_process--;
         (*no_of_process)--;
         if ((fra[dcpl[i].fra_pos].no_of_process == 0) &&
             (fra[dcpl[i].fra_pos].dir_status == DIRECTORY_ACTIVE) &&
             (fra[dcpl[i].fra_pos].dir_status != DISABLED))
         {
            fra[dcpl[i].fra_pos].dir_status = NORMAL_STATUS;
         }
         for (j = i; j < *no_of_process; j++)
         {
            dcpl[j].pid = dcpl[j + 1].pid;
            dcpl[j].fra_pos = dcpl[j + 1].fra_pos;
         }
         dcpl[j].pid = -1;
         dcpl[j].fra_pos = -1;
         i--;
         zombies_killed++;
      }
   }

   return(zombies_killed);
}


/*+++++++++++++++++++++++++++ check_fifo() ++++++++++++++++++++++++++++++*/
static int
check_fifo(int    read_fd,
           int    write_fd,
           time_t *time_buf,
           time_t *rescan_time,
           int    *no_of_jobs)
{
   int  n;
   char buffer[MAX_FIFO_BUFFER];

   /* Read the message */
   if ((n = read(read_fd, buffer, MAX_FIFO_BUFFER)) > 0)
   {
      int  count = 0,
           i;

#ifdef _FIFO_DEBUG
      show_fifo_data('R', "ip_cmd", buffer, n, __FILE__, __LINE__);
#endif
      while (count < n)
      {
         switch(buffer[count])
         {
            case STOP  :
               /* Detach shared memory regions */
               if (shmdt(p_shm) < 0)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Could not detach shared memory region %d : %s (%s %d)\n",
                            shm_id, strerror(errno), __FILE__, __LINE__);
               }

               /* free memory for pid, time and file name array */
               free(dcpl);
               for (i = 0; i < no_of_local_dirs; i++)
               {
                  int j;

                  for (j = 0; j < de[i].nfg; j++)
                  {
                     free(de[i].fme[j].pos);
                     free(de[i].fme[j].file_mask);
                     de[i].fme[j].file_mask = NULL;
                  }
                  free(de[i].fme);
                  de[i].fme = NULL;
               }
               free(de);
               FREE_RT_ARRAY(file_name_pool);
               if (munmap((void *)db, *no_of_jobs * sizeof(struct instant_db)) == -1)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "munmap() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
               if (time_job_list != NULL)
               {
                  free(time_job_list);
                  time_job_list = NULL;
               }
#ifdef _WITH_PTHREAD
               free(thread);
               free(p_data);
               for (i = 0; i < no_of_local_dirs; i++)
               {
                  FREE_RT_ARRAY(p_data[i].file_name_pool);
               }
#endif

               /* Set flag to indicate that the the dir_check is NOT active. */
               if (p_afd_status->amg_jobs & INST_JOB_ACTIVE)
               {
                  p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
               }

               /* Unmap from AFD status area. */
               {
                  char        afd_status_file[MAX_PATH_LENGTH];
                  struct stat stat_buf;

                  (void)strcpy(afd_status_file, p_work_dir);
                  (void)strcat(afd_status_file, FIFO_DIR);
                  (void)strcat(afd_status_file, STATUS_SHMID_FILE);
                  if (stat(afd_status_file, &stat_buf) == -1)
                  {
                     (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Failed to stat() %s : %s (%s %d)\n",
                               afd_status_file, strerror(errno),
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     if (munmap((void *)p_afd_status, stat_buf.st_size) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to munmap() from %s : %s (%s %d)\n",
                                  afd_status_file, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
#ifdef _FIFO_DEBUG
               cmd[0] = ACKN; cmd[1] = '\0';
               show_fifo_data('W', "ip_resp", cmd, 1, __FILE__, __LINE__);
#endif
               if (send_cmd(ACKN, write_fd) < 0)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not write to fifo %s : %s (%s %d)\n",
                            DC_CMD_FIFO, strerror(errno),
                            __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               (void)close(counter_fd);

               exit(SUCCESS);

            default    : /* Most properly we are reading garbage */
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Reading garbage (%d) on fifo %s. (%s %d)\n",
                         buffer[0], DC_CMD_FIFO, __FILE__, __LINE__);
               exit(INCORRECT);
         }
      } /* while (count < n) */
   }

   return(0);
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   /* Set flag to indicate that the the dir_check is NOT active. */
   if (p_afd_status->amg_jobs & INST_JOB_ACTIVE)
   {
      p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
   }

   /* Detach shared memory regions */
   if (p_shm != NULL)
   {
      if (shmdt(p_shm) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not detach shared memory region %d : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   }

   (void)rec(sys_log_fd, FATAL_SIGN,
             "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   /* Set flag to indicate that the the dir_check is NOT active. */
   if (p_afd_status->amg_jobs & INST_JOB_ACTIVE)
   {
      p_afd_status->amg_jobs ^= INST_JOB_ACTIVE;
   }

   /* Detach shared memory regions */
   if (p_shm != NULL)
   {
      if (shmdt(p_shm) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not detach shared memory region %d : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   }

   (void)rec(sys_log_fd, FATAL_SIGN,
             "Uuurrrggh! Received SIGBUS. Dump programmers! (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}
