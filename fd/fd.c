/*
 *  fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fd - creates transfer jobs and manages them
 **
 ** SYNOPSIS
 **   fd [--version] [-w <AFD working directory>]
 **
 ** DESCRIPTION
 **   This program starts at the most MAX_DEFAULT_CONNECTIONS jobs
 **   (parallel), to send files to certain destinations. It always
 **   waits for these process to finish and will generate an
 **   appropriate message when one has finished.
 **   To start a new job it always looks every FD_RESCAN_TIME seconds
 **   in the message directory to see whether a new message has arrived.
 **   This message will then be moved to the transmitting directory
 **   and will then start sf_xxx. If sf_xxx ends successfully it will
 **   remove this message (or archive it) else the fd will move the
 **   message and the appropriate files into the error directories.
 **
 **   The FD communicates with the AFD via the to fifos FD_CMD_FIFO
 **   and FD_RESP_FIFO.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.1995 H.Kiehl Created
 **   07.01.1997 H.Kiehl Implemented automatic switching when secondary
 **                      host is available.
 **   25.01.1997 H.Kiehl Add support for output logging.
 **   30.01.1997 H.Kiehl Added burst mode.
 **   02.05.1997 H.Kiehl Include support for MAP.
 **   15.01.1998 H.Kiehl Complete redesign to accommodate new messages.
 **   09.07.1998 H.Kiehl Option to delete all jobs from a specific host.
 **   10.08.2000 H.Kiehl Added support to fetch files from a remote host.
 **   14.12.2000 H.Kiehl Decrease priority of a job in queue on certain
 **                      errors of sf_xxx.
 **   16.04.2002 H.Kiehl Improve speed when inserting new jobs in queue.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strcpy(), strcat(), strerror(),         */
                              /* memset(), memmove()                     */
#include <stdlib.h>           /* getenv(), atexit(), exit()              */
#include <unistd.h>           /* unlink(), mkdir(), fpathconf()          */
#include <signal.h>           /* kill(), signal()                        */
#include <limits.h>           /* LINK_MAX                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>         /* msync(), munmap()                       */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/wait.h>         /* waitpid()                               */
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <errno.h>
#include "fddefs.h"
#include "logdefs.h"
#include "version.h"

/* #define WITH_MULTI_FSA_CHECKS */

/* Global variables */
int                        amg_flag = NO,
                           default_age_limit = 0,
                           delete_jobs_fd,
                           delete_jobs_host_fd,
                           fd_cmd_fd,
                           fd_resp_fd,
                           fd_wake_up_fd,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           loop_counter = 0,
                           max_connections = MAX_DEFAULT_CONNECTIONS,
                           max_output_log_files = MAX_OUTPUT_LOG_FILES,
                           mdb_fd = -1,
                           msg_fifo_fd,
                           *no_msg_queued,
                           *no_msg_cached,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           no_of_retrieves = 0,
                           qb_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           read_fin_fd,
                           remote_file_check_interval = DEFAULT_REMOTE_FILE_CHECK_INTERVAL,
                           *retrieve_list = NULL,
                           retry_fd,
                           transfer_log_fd = STDERR_FILENO;
#ifndef _NO_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
off_t                      *buf_file_size = NULL;
char                       stop_flag = 0,
                           *p_work_dir,
                           **p_buf_name = NULL,
                           *file_buffer = NULL,
                           *p_err_file_dir,
                           *p_file_dir,
                           *p_msg_dir,
#ifdef _BURST_MODE
                           *p_src_dir,
                           src_dir[MAX_PATH_LENGTH],
#endif
                           file_dir[MAX_PATH_LENGTH],
                           err_file_dir[MAX_PATH_LENGTH],
                           msg_dir[MAX_PATH_LENGTH];
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct afd_status          *p_afd_status;
struct connection          *connection = NULL;
struct queue_buf           *qb;
struct msg_cache_buf       *mdb;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

/* Local global variables */
static time_t              now,
                           next_dir_check_time;

#if defined _BURST_MODE || defined _AGE_LIMIT
#define START_PROCESS()                                       \
        {                                                     \
           int fsa_pos;                                       \
                                                              \
           /* Try handle any pending jobs. */                 \
           for (i = 0; ((i < *no_msg_queued) && (p_afd_status->no_of_transfers < max_connections)); i++) \
           {                                                  \
              if (qb[i].pid == PENDING)                       \
              {                                               \
                 if (qb[i].msg_name[0] != '\0')               \
                 {                                            \
                    fsa_pos = mdb[qb[i].pos].fsa_pos;         \
                 }                                            \
                 else                                         \
                 {                                            \
                    fsa_pos = fra[qb[i].pos].fsa_pos;         \
                 }                                            \
                 if ((qb[i].pid = start_process(fsa_pos, i, now, NO)) == REMOVED) \
                 {                                            \
                    /*                                        \
                     * The message can be removed because the \
                     * files are queued in another message    \
                     * or have been removed due to age.       \
                     */                                       \
                    remove_msg(i);                            \
                    if (i < *no_msg_queued)                   \
                    {                                         \
                       i--;                                   \
                    }                                         \
                 }                                            \
              }                                               \
           }                                                  \
        }
#else
#define START_PROCESS()                                       \
        {                                                     \
           int fsa_pos;                                       \
                                                              \
           /* Try handle any pending jobs. */                 \
           for (i = 0; ((i < *no_msg_queued) && (p_afd_status->no_of_transfers < max_connections)); i++) \
           {                                                  \
              if (qb[i].pid == PENDING)                       \
              {                                               \
                 if (qb[i].msg_name[0] != '\0')               \
                 {                                            \
                    fsa_pos = mdb[qb[i].pos].fsa_pos;         \
                 }                                            \
                 else                                         \
                 {                                            \
                    fsa_pos = fra[qb[i].pos].fsa_pos;         \
                 }                                            \
                 qb[i].pid = start_process(fsa_pos, i, now, NO); \
              }                                               \
           }                                                  \
        }
#endif
#define ABS_REDUCE(value)                                     \
        {                                                     \
           unsigned int tmp_value;                            \
                                                              \
           tmp_value = fsa[(value)].jobs_queued;              \
           fsa[(value)].jobs_queued = fsa[(value)].jobs_queued - 1; \
           if (fsa[(value)].jobs_queued > tmp_value)          \
           {                                                  \
              (void)rec(sys_log_fd, DEBUG_SIGN,               \
                        "Overflow from <%u> for %s. Trying to correct. (%s %d)\n", \
                        tmp_value, fsa[(value)].host_dsp_name,\
                        __FILE__, __LINE__);                  \
              fsa[(value)].jobs_queued = recount_jobs_queued((value)); \
           }                                                  \
        }

/* Local functions prototypes */
static void  to_error_dir(int),
             get_afd_config_value(void),
             fd_exit(void),
             qb_pos_fsa(int, int *),
             qb_pos_pid(pid_t, int *),
             sig_exit(int),
             sig_segv(int),
             sig_bus(int);
static int   get_free_connection(void),
             get_free_disp_pos(int),
             zombie_check(struct connection *, time_t, int *, int),
             recount_jobs_queued(int);
static pid_t make_process(struct connection *),
             start_process(int, int, time_t, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int               fifo_full_counter = 0,
                     force_check = NO,
                     i,
                     *job_id,
                     status,
                     max_fd;
   unsigned short    *unique_number;
   time_t            *creation_time,
                     abnormal_term_check_time,
#ifdef _WITH_INTERRUPT_JOB
                     interrupt_check_time,
#endif
                     remote_file_check_time;
   size_t            fifo_size,
                     msg_fifo_buf_size;
   char              *fifo_buffer,
                     *msg_buffer,
                     *msg_priority,
                     work_dir[MAX_PATH_LENGTH],
                     name_buffer[MAX_PATH_LENGTH];
   fd_set            rset;
   struct timeval    timeout;
#ifdef SA_FULLDUMP
   struct sigaction  sact;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char *ptr;

      p_work_dir = work_dir;

      /*
       * Lock FD so no other FD can be started!
       */
      if ((ptr = lock_proc(FD_LOCK_ID, NO)) != NULL)
      {
         (void)fprintf(stderr,
                       "Process FD already started by %s : (%s %d)\n",
                       ptr, __FILE__, __LINE__);
	 system_log(ERROR_SIGN, __FILE__, __LINE__,
	            "Process FD already started by %s", ptr);
         exit(INCORRECT);
      }
   }

   /* Initialise variables */
   (void)strcpy(name_buffer, work_dir);
   (void)strcat(name_buffer, FIFO_DIR);
   (void)strcat(name_buffer, SYSTEM_LOG_FIFO);
   (void)strcpy(msg_dir, work_dir);
   (void)strcat(msg_dir, AFD_MSG_DIR);
   (void)strcat(msg_dir, "/");
   (void)strcpy(file_dir, work_dir);
   (void)strcat(file_dir, AFD_FILE_DIR);
   (void)strcpy(err_file_dir, file_dir);
   (void)strcat(err_file_dir, ERROR_DIR);
   (void)strcat(err_file_dir, "/");
   (void)strcat(file_dir, "/");
   p_msg_dir = msg_dir + strlen(msg_dir);
   p_file_dir = file_dir + strlen(file_dir);
   p_err_file_dir = err_file_dir + strlen(err_file_dir);
#ifdef _BURST_MODE
   (void)strcpy(src_dir, file_dir);
   p_src_dir = src_dir + strlen(src_dir);
#endif

   init_msg_ptrs(&msg_fifo_buf_size,
                 &creation_time,
                 &job_id,
                 &unique_number,
                 &msg_priority,
                 &msg_buffer);

   /* Open system log fifo. */
   if ((sys_log_fd = coe_open(name_buffer, O_RDWR)) < 0)
   {
      if (errno == ENOENT)
      {
         if (make_fifo(name_buffer) == SUCCESS)
         {
            if ((sys_log_fd = coe_open(name_buffer, O_RDWR)) < 0)
            {
               (void)fprintf(stderr,
                             "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                             name_buffer, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not create fifo %s. (%s %d)\n",
                          name_buffer, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                       name_buffer, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open and create all fifos */
   if (init_fifos_fd() == INCORRECT)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to initialize fifos. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get the fra_id and no of directories of the FRA */
   if (fra_attach() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to attach to FRA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if (fra[i].queued == YES)
      {
         fra[i].queued = NO;
      }
   }
   init_fra_data();

   /* Get the fsa_id and no of host of the FSA */
   if (fsa_attach() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to attach to FSA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Attach to the AFD Status Area */
   if (attach_afd_status() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to map to AFD status area. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialize all connections in case FD crashes. */
   p_afd_status->no_of_transfers = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      int j;

      fsa[i].active_transfers = 0;
      for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
      {
         fsa[i].job_status[j].no_of_files = 0;
         fsa[i].job_status[j].proc_id = -1;
         fsa[i].job_status[j].connect_status = DISCONNECT;
         fsa[i].job_status[j].file_name_in_use[0] = '\0';
      }
   }

   if (p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE)
   {
      p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
   }

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif /* _DELETE_LOG */

   /* Get value from AFD_CONFIG file */
   get_afd_config_value();

   /* Attach/create memory area for message data and queue. */
   init_msg_buffer();

   /*
    * Initialize the queue and remove any queued retrieve job from
    * it. Because at this point we do not have the necessary information
    * to check if this job still does exists.
    */
   for (i = 0; i < *no_msg_queued; i++)
   {
      qb[i].pid = PENDING;
      if (qb[i].msg_name[0] == '\0')
      {
         ABS_REDUCE(fra[qb[i].pos].fsa_pos);
         remove_msg(i);
      }
   }

   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((i = (int)fpathconf(delete_jobs_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   else
   {
      fifo_size = i;
   }

   /* Allocate a buffer for reading data from FIFO's. */
   if ((fifo_buffer = malloc(fifo_size)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

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

   /* Do some cleanups when we exit */
   if (atexit(fd_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not set signal handlers : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Find largest file descriptor */
   max_fd = read_fin_fd;
   if (fd_cmd_fd > max_fd)
   {
      max_fd = fd_cmd_fd;
   }
   if (msg_fifo_fd > max_fd)
   {
      max_fd = msg_fifo_fd;
   }
   if (fd_wake_up_fd > max_fd)
   {
      max_fd = fd_wake_up_fd;
   }
   if (retry_fd > max_fd)
   {
      max_fd = retry_fd;
   }
   if (delete_jobs_fd > max_fd)
   {
      max_fd = delete_jobs_fd;
   }
   if (delete_jobs_host_fd > max_fd)
   {
      max_fd = delete_jobs_host_fd;
   }
   max_fd++;

   /* Allocate memory for connection structure. */
   if ((connection = malloc((max_connections * sizeof(struct connection)))) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise structure connection */
   (void)memset(connection, 0, (max_connections * sizeof(struct connection)));

   /* Tell user we are starting the FD */
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d-pre%d)\n",
             FD, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             FD, MAJOR, MINOR, BUG_FIX);
#endif
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "FD configuration: Max. connections           %d\n",
             max_connections);
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "FD configuration: Remote file check interval %d (sec)\n",
             remote_file_check_interval);
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "FD configuration: FD rescan interval         %d (sec)\n",
             FD_RESCAN_TIME);
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "FD configuration: Default age limit          %d (sec)\n",
             default_age_limit);
   abnormal_term_check_time = (time(&now) / 45) * 45 + 45;
   next_dir_check_time = ((now / DIR_CHECK_TIME) * DIR_CHECK_TIME) +
                         DIR_CHECK_TIME;
   remote_file_check_time = ((now / remote_file_check_interval) *
                             remote_file_check_interval) +
                            remote_file_check_interval;
#ifdef _WITH_INTERRUPT_JOB
   interrupt_check_time = ((now / PRIORITY_INTERRUPT_CHECK_TIME) *
                          PRIORITY_INTERRUPT_CHECK_TIME) +
                          PRIORITY_INTERRUPT_CHECK_TIME;
#endif
   FD_ZERO(&rset);

   /* Now watch and start transfer jobs */
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_SET(fd_cmd_fd, &rset);
      FD_SET(read_fin_fd, &rset);
      FD_SET(msg_fifo_fd, &rset);
      FD_SET(fd_wake_up_fd, &rset);
      FD_SET(retry_fd, &rset);
      FD_SET(delete_jobs_fd, &rset);
      FD_SET(delete_jobs_host_fd, &rset);
      if ((p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE) ||
          (force_check == YES))
      {
         timeout.tv_usec = 100000L;
         timeout.tv_sec = 0L;
      }
      else
      {
         timeout.tv_usec = 0L;
         timeout.tv_sec = FD_RESCAN_TIME;
      }

      if (*no_msg_queued > p_afd_status->max_queue_length)
      {
         p_afd_status->max_queue_length = *no_msg_queued;
      }

      /*
       * Always check in 45 second intervals if a process has
       * terminated abnormally, ie where we do not get a
       * message via the READ_FIN_FIFO (eg. someone killed it with
       * a kill -9). Also check if the content of any message has
       * changed since the last check.
       */
      if (time(&now) > abnormal_term_check_time)
      {
         if (p_afd_status->no_of_transfers > 0)
         {
            for (i = 0; i < max_connections; i++)
            {
               if (connection[i].pid > 0)
               {
                  int faulty,
                      qb_pos;

                  qb_pos_pid(connection[i].pid, &qb_pos);
                  if (qb_pos != -1)
                  {
                     if ((faulty = zombie_check(&connection[i], now, &qb_pos,
                                                WNOHANG)) == YES)
                     {
                        qb[qb_pos].pid = PENDING;
                        if (qb[qb_pos].msg_name[0] != '\0')
                        {
                           fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                           if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter == 1)
                           {
                              int j;

                              /* Move all jobs from this host to the error directory. */
                              for (j = 0; j < *no_msg_queued; j++)
                              {
                                 if ((qb[j].in_error_dir != YES) &&
                                     (qb[j].pid == PENDING) &&
                                     (mdb[qb[j].pos].fsa_pos == mdb[qb[qb_pos].pos].fsa_pos))
                                 {
                                    to_error_dir(j);
                                 }
                              }
                           }
                        }
                        else
                        {
                           fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                        }
                     }
                     else if (faulty == NO)
                          {
                             remove_msg(qb_pos);
                          }
                     else if (faulty == NONE)
                          {
                             if (qb[qb_pos].msg_name[0] != '\0')
                             {
                                fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                             }
                             else
                             {
                                fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                             }
                             qb[qb_pos].pid = PENDING;
                          }

                     if ((stop_flag == 0) && (faulty != NEITHER) &&
                         (*no_msg_queued > 0))
                     {
                        START_PROCESS();
                     }
                  } /* if (qb_pos != -1) */
               } /* if (connection[i].pid > 0) */
            } /* for (i = 0; i < max_connections; i++) */
         } /* if (p_afd_status->no_of_transfers > 0) */
         else if (p_afd_status->no_of_transfers == 0)
              {
                 pid_t ret;

                 while ((ret = waitpid(-1, NULL, WNOHANG)) > 0)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "GOTCHA! Caught some unknown zombie with pid %d (%s %d)\n",
                              ret, __FILE__, __LINE__);
                 }
                 if ((ret == -1) && (errno != ECHILD))
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN,
                              "waitpid() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                 }
              }

         /*
          * Check if the content of any message has changed since
          * we have last checked.
          */
         check_msg_time();

         /*
          * Check jobs_queued counter in the FSA is still correct. If not
          * reset it to the correct value.
          */
         if (*no_msg_queued == 0)
         {
            for (i = 0; i < no_of_hosts; i++)
            {
               if (fsa[i].jobs_queued != 0)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Jobs queued for %s is %u and not zero. Reset to zero. (%s %d)\n",
                            fsa[i].host_dsp_name, fsa[i].jobs_queued,
                            __FILE__, __LINE__);
                  fsa[i].jobs_queued = 0;
               }
            }
         }

         abnormal_term_check_time = ((now / 45) * 45) + 45;
      }

#ifdef _WITH_INTERRUPT_JOB
      if (now > interrupt_check_time)
      {
         if (*no_msg_queued > 0)
         {
            int *pos_list;

            if ((pos_list = malloc(no_of_hosts * sizeof(int))) == NULL)
            {
               (void)rec(sys_log_fd, WARN_SIGN, "malloc() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               int full_hosts = 0,
                   hosts_done = 0;

               for (i = 0; i < no_of_hosts; i++)
               {
                  if (fsa[i].active_transfers >= fsa[i].allowed_transfers)
                  {
                     pos_list[full_hosts] = i;
                     full_hosts++;
                  }
               }
               if (full_hosts > 0)
               {
                  for (i = 0; ((i < *no_msg_queued) && (full_hosts > hosts_done)); i++)
                  {
                     if (qb[i].msg_name[0] != '\0')
                     {
                        if (qb[i].msg_name[0] > '8')
                        {
                           break;
                        }
                        else
                        {
                           if (qb[i].pid == PENDING)
                           {
                              for (j = 0; j < full_hosts; j++)
                              {
                                 if ((pos_list[j] != -1) &&
                                     (pos_list[j] == connection[qb[i].connect_pos].fsa_pos))
                                 {
                                    int  k,
                                         pos = -1;
                                    char largest_priority = '0';

                                    for (k = 0; k < fsa[pos_list[j]].allowed_transfers; k++)
                                    {
                                       if ((fsa[pos_list[j]].job_status[k].unique_name[0] > largest_priority) &&
                                           ((fsa[pos_list[j]].job_status[k].special_flag & INTERRUPT_JOB) == 0) &&
                                           ((fsa[pos_list[j]].job_status[k].no_of_files - fsa[pos_list[j]].job_status[k].no_of_files_done) > 1))
                                       {
                                          largest_priority = fsa[pos_list[j]].job_status[k].unique_name[0];
                                          pos = k;
                                       }
                                    }
                                    if (pos > -1)
                                    {
                                       if (qb[i].msg_name[0] > largest_priority)
                                       {
                                          fsa[pos_list[j]].job_status[k].special_flag ^= INTERRUPT_JOB;
(void)rec(sys_log_fd, DEBUG_SIGN, "Setting INTERRUPT_JOB for host %s in position %d\n", fsa[pos_list[j]].host_dsp_name, k);
                                       }
                                    }
                                    hosts_done++;
                                    pos_list[j] = -1;
                                 }
                              }
                           }
                        }
                     }
                  }
               }
               (void)free(pos_list);
            }
         }
         interrupt_check_time = ((now / PRIORITY_INTERRUPT_CHECK_TIME) *
                                PRIORITY_INTERRUPT_CHECK_TIME) +
                                PRIORITY_INTERRUPT_CHECK_TIME;
      }
#endif /* _WITH_INTERRUPT_JOB */

      /*
       * Check if we must check for files on any remote system.
       */
      if (((*(unsigned char *)((char *)fsa - 3) & DISABLE_RETRIEVE) == 0) &&
          (p_afd_status->no_of_transfers < max_connections) &&
          (no_of_retrieves > 0) && (now > remote_file_check_time))
      {
         for (i = 0; i < no_of_retrieves; i++)
         {
            if ((fra[retrieve_list[i]].queued == NO) &&
                (fra[retrieve_list[i]].dir_status != DISABLED) &&
                ((fsa[fra[retrieve_list[i]].fsa_pos].special_flag & HOST_DISABLED) == 0) &&
                ((fsa[fra[retrieve_list[i]].fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
                ((fra[retrieve_list[i]].time_option == NO) ||
                 (fra[retrieve_list[i]].next_check_time <= now)))
            {
               int    qb_pos;
               double msg_number = ((double)fra[retrieve_list[i]].priority - 47.0) *
                                   ((double)now * 10000.0);

               check_queue_space();
               if (*no_msg_queued > 0)
               {
                  if (*no_msg_queued == 1)
                  {
                     if (qb[0].msg_number < msg_number)
                     {
                        qb_pos = 1;
                     }
                     else
                     {
                        size_t move_size;

                        move_size = *no_msg_queued * sizeof(struct queue_buf);
                        (void)memmove(&qb[1], &qb[0], move_size);
                        qb_pos = 0;
                     }
                  }
                  else
                  {
                     if (msg_number < qb[0].msg_number)
                     {
                        size_t move_size;

                        move_size = *no_msg_queued * sizeof(struct queue_buf);
                        (void)memmove(&qb[1], &qb[0], move_size);
                        qb_pos = 0;
                     }
                     else if (msg_number > qb[*no_msg_queued - 1].msg_number)
                          {
                             qb_pos = *no_msg_queued;
                          }
                          else
                          {
                             int center,
                                 end = *no_msg_queued - 1,
                                 start = 0;

                             for (;;)
                             {
                                center = (end - start) / 2;
                                if (center == 0)
                                {
                                   size_t move_size;

                                   move_size = (*no_msg_queued - (start + 1)) *
                                               sizeof(struct queue_buf);
                                   (void)memmove(&qb[start + 2], &qb[start + 1],
                                                 move_size);
                                   qb_pos = start + 1;
                                   break;
                                }
                                if (msg_number < qb[start + center].msg_number)
                                {
                                   end = start + center;
                                }
                                else
                                {
                                   start = start + center;
                                }
                             }
                          }
                  }
               }
               else
               {
                  qb_pos = 0;
               }

               /* Put data in queue. */
               qb[qb_pos].msg_name[0] = '\0';
               qb[qb_pos].msg_number = msg_number;
               qb[qb_pos].creation_time = now;
               qb[qb_pos].pos = retrieve_list[i];
               qb[qb_pos].connect_pos = -1;
               qb[qb_pos].in_error_dir = NO;
               (*no_msg_queued)++;
               fsa[fra[retrieve_list[i]].fsa_pos].jobs_queued++;
               fra[retrieve_list[i]].queued = YES;

               if ((fsa[fra[retrieve_list[i]].fsa_pos].error_counter == 0) &&
                   (stop_flag == 0))
               {
                  qb[qb_pos].pid = start_process(fra[retrieve_list[i]].fsa_pos,
                                                 qb_pos, now, NO);
               }
               else
               {
                  qb[qb_pos].pid = PENDING;
               }
            }
            else if (((fsa[fra[retrieve_list[i]].fsa_pos].special_flag & HOST_DISABLED) ||
                      (fsa[fra[retrieve_list[i]].fsa_pos].host_status & STOP_TRANSFER_STAT)) &&
                     (fra[retrieve_list[i]].time_option == YES) &&
                     (fra[retrieve_list[i]].next_check_time <= now))
                 {
                    fra[retrieve_list[i]].next_check_time = calc_next_time(&fra[retrieve_list[i]].te);
                 }
         }
         remote_file_check_time = ((now / remote_file_check_interval) *
                                   remote_file_check_interval) +
                                  remote_file_check_interval;
      }

      /* Check if we have to stop and we have */
      /* no more running jobs.                */
      if ((stop_flag > 0) && (p_afd_status->no_of_transfers < 1))
      {
         break;
      }

      /*
       * Search file directory for any job that does not have a
       * message. Do this in a DIR_CHECK_TIME (25 min) interval
       * and only when there are less then 150 jobs in the file
       * directory, otherwise if there are more it will take to
       * long to check all directories.
       * NOTE: There may NOT be any message in the fifo, otherwise
       *       we will duplicate messages!
       */
      if (now > next_dir_check_time)
      {
         int gotcha = NO,
             loops = 0;

         /*
          * It only makes sense to check the file directory when
          * the AMG is currently NOT creating new jobs. So lets
          * indicate that by setting the FD_DIR_CHECK_ACTIVE flag.
          * We then have to wait for all jobs of the AMG to stop
          * creating new jobs.
          */
         if ((p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE) == 0)
         {
            p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
         }
         do
         {
            if ((p_afd_status->amg_jobs & DIR_CHECK_ACTIVE) == 0)
            {
               gotcha = YES;
               break;
            }
            (void)my_usleep(100000L);
         } while (loops++ < WAIT_LOOPS);
         if (gotcha == NO)
         {
            next_dir_check_time = ((now / DIR_CHECK_TIME) * DIR_CHECK_TIME) +
                                  DIR_CHECK_TIME;

            /* Deactivate directory check flag. */
            if (p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE)
            {
               p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
            }
         }
      }

      /* Wait for message x seconds and then continue. */
      status = select(max_fd, &rset, NULL, NULL, &timeout);

      if ((p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE) && (status == 0))
      {
         if (check_file_dir(force_check) == YES)
         {
            check_fsa_entries();
         }
         next_dir_check_time = ((now / DIR_CHECK_TIME) * DIR_CHECK_TIME) +
                               DIR_CHECK_TIME;
         force_check = NO;

         /* Deactivate directory check flag. */
         if (p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE)
         {
            p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
         }
      }

      /*
       * MESSAGE FROM COMMAND FIFO ARRIVED
       * =================================
       */
      if ((status > 0) && (FD_ISSET(fd_cmd_fd, &rset)))
      {
         char buffer;

         /* Read the message */
         if (read(fd_cmd_fd, &buffer, 1) > 0)
         {
            switch(buffer)
            {
               case CHECK_FILE_DIR :
                  next_dir_check_time = 0;
                  force_check = YES;
                  break;

               case FSA_ABOUT_TO_CHANGE :
                  {
                     int         fd;
                     char        fifo[MAX_PATH_LENGTH];
                     struct stat stat_buf;

                     (void)sprintf(fifo, "%s%s%s", p_work_dir, FIFO_DIR, FD_READY_FIFO);
                     if ((stat(fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
                     {
                        if (make_fifo(fifo) < 0)
                        {
                           (void)rec(sys_log_fd, FATAL_SIGN,
                                     "Could not create fifo %s. (%s %d)\n",
                                     fifo, __FILE__, __LINE__);
                           exit(INCORRECT);
                        }
                     }
                     if ((fd = open(fifo, O_RDWR)) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Could not open() fifo %s : %s (%s %d)\n",
                                  fifo, strerror(errno), __FILE__, __LINE__);
                     }
                     else
                     {
                        if (write(fd, "", 1) != 1)
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Could not write() to fifo %s : %s (%s %d)\n",
                                     fifo, strerror(errno), __FILE__, __LINE__);
                        }
                        if (close(fd) == -1)
                        {
                           (void)rec(sys_log_fd, DEBUG_SIGN,
                                     "close() error : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                        }
                     }
                     if (fd_check_fsa() == YES)
                     {
                        if (check_fra_fd() == YES)
                        {
                           init_fra_data();
                        }
                        get_new_positions();
                        init_msg_buffer();
                     }
                  }
                  break;

               case SAVE_STOP :
                  /* Here all running transfers are */
                  /* completed and no new jobs will */
                  /* be started.                    */
                  if (stop_flag == SAVE_STOP)
                  {
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "%s is already shutting down. Please be patient.\n",
                               FD);
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Maximum shutdown time for %s is %d seconds.\n",
                               FD, FD_TIMEOUT);
                  }
                  else
                  {
                     (void)rec(sys_log_fd, INFO_SIGN, "FD shutting down ...\n");
                     stop_flag = SAVE_STOP;
                  }
                  break;

               case STOP :
               case QUICK_STOP :
                  /* All transfers are aborted and  */
                  /* we do a shutdown as quick as   */
                  /* possible by killing all jobs.  */
                  stop_flag = (int)buffer;
                  loop_counter = 0;
                  (void)rec(sys_log_fd, INFO_SIGN, "FD shutting down ...\n");

                  exit(SUCCESS);

               default :
                  /* Most properly we are reading garbage. */
                  (void)rec(sys_log_fd, WARN_SIGN, 
                            "Reading garbage (%d) on fifo %s. (%s %d)\n",
                            (int)buffer, FD_CMD_FIFO, __FILE__, __LINE__);
                  break;
            } /* switch(buffer) */
         }
      }

           /*
            * sf_xxx or gf_xxx PROCESS TERMINATED
            * ===================================
            *
            * Everytime any child terminates it sends its PID via a well
            * known FIFO to this process. If the PID is negative the child
            * asks the parent fo more data to process.
            *
            * This communication could be down more effectifly by catching
            * the SIGCHLD signal everytime a child terminates. This was
            * implemented in version 1.2.12-pre4. It just turned out that
            * this was not very portable since the behaviour differed
            * in combination with the select() system call. When using
            * SA_RESTART both Linux and Solaris would cause select() to
            * EINTR, which is very usfull, but FTX and Irix did not
            * do this. Then it could take up to 10 seconds before FD
            * caught its children. For this reason this was dropped. :-(
            */
      else if ((status > 0) && (FD_ISSET(read_fin_fd, &rset)))
           {
              int n;

              if ((n = read(read_fin_fd, fifo_buffer, fifo_size)) >= sizeof(pid_t))
              {
                 int   bytes_done = 0,
                       faulty = NO,
                       qb_pos;
                 pid_t pid;
#ifdef _WITH_BURST_2
                 int   start_new_process;
#endif /* _WITH_BURST_2 */

                 now = time(NULL);
                 do
                 {
#ifdef WITH_MULTI_FSA_CHECKS
                    if (fd_check_fsa() == YES)
                    {
                       if (check_fra_fd() == YES)
                       {
                          init_fra_data();
                       }
                       get_new_positions();
                       init_msg_buffer();
                    }
#endif /* WITH_MULTI_FSA_CHECKS */
                    pid = *(pid_t *)&fifo_buffer[bytes_done];
#ifdef _WITH_BURST_2
                    if (pid < 0)
                    {
                       pid = -pid;
                       qb_pos_pid(pid, &qb_pos);

                       /*
                        * Check third byte in unique_name. If this is
                        * NOT set to zero the process sf_xxx has given up
                        * waiting for FD to give it a new job, ie. the
                        * process no longer exists and we need to start
                        * a new one.
                        */
                       if (fsa[mdb[qb[qb_pos].pos].fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[2] == 4)
                       {
                          start_new_process = NO;
                       }
                       else
                       {
                          start_new_process = YES;
                       }
                    }
                    else
                    {
                       qb_pos_pid(pid, &qb_pos);
                       start_new_process = YES;
                    }
#else
                    qb_pos_pid(pid, &qb_pos);
#endif /* _WITH_BURST_2 */
                    if (qb_pos != -1)
                    {
#ifdef _WITH_BURST_2
                       /*
                        * This process is ready to process more data.
                        * Have a look in the queue if there is another
                        * job pending for this host.
                        */
                       if (start_new_process == NO)
                       {
                          int fsa_pos,
                              gotcha = NO;

                          /*
                           * NOTE: It's not necessary to check here if
                           *       qb[qb_pos].msg_name[0] equals '\0'
                           *       because if start_new_process is set
                           *       to NO it cannot be a retrieve
                           *       process. To check if this is really
                           *       true there is this debug output
                           *       a few lines down. Remove this
                           *       check if we never see this line.
                           */
                          if (qb[qb_pos].msg_name[0] != '\0')
                          {
                             fsa_pos = mdb[qb[qb_pos].pos].fsa_pos;
                          }
                          else
                          {
                             fsa_pos = fra[qb[qb_pos].pos].fsa_pos;
                             (void)rec(sys_log_fd, DEBUG_SIGN,
                                       "Oooops! This does really happen! So we may not remove this code! (%s %d)\n",
                                       __FILE__, __LINE__);
                          }
                          if (fsa[fsa_pos].jobs_queued > 0)
                          {
                             for (i = 0; i < *no_msg_queued; i++)
                             {
                                if ((qb[i].pid == PENDING) &&
                                    (qb[i].msg_name[0] != '\0') &&
                                    (mdb[qb[i].pos].fsa_pos == fsa_pos) &&
                                    (mdb[qb[i].pos].type == mdb[qb[qb_pos].pos].type))
                                {
                                   /* Yep, there is another job pending! */
                                   gotcha = YES;
                                   break;
                                }
                             }
                          }
                          if (gotcha == YES)
                          {
#ifdef _WITH_INTERRUPT_JOB
                             int interrupt = NO;

                             if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[3] == 4)
                             {
                                interrupt = YES;
                                if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag & INTERRUPT_JOB)
                                {
                                   fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag ^= INTERRUPT_JOB;
                                }
                             }
#endif
                             fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].error_file = qb[i].in_error_dir;
                             fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].job_id = mdb[qb[i].pos].job_id;
                             (void)memcpy(fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name,
                                          qb[i].msg_name, MAX_MSG_NAME_LENGTH);
                             qb[i].pid = pid;
                             qb[i].connect_pos = qb[qb_pos].connect_pos;
#ifdef _WITH_INTERRUPT_JOB
                             if (interrupt == NO)
                             {
#endif
                                ABS_REDUCE(fsa_pos);
                                remove_msg(qb_pos);
#ifdef _WITH_INTERRUPT_JOB
                             }
#endif
                             p_afd_status->burst2_counter++;
                          }
                          else
                          {
                             fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[0] = '\0';
                             fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].unique_name[1] = 1;
#ifdef _WITH_INTERRUPT_JOB
                             if (fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag & INTERRUPT_JOB)
                             {
                                fsa[fsa_pos].job_status[connection[qb[qb_pos].connect_pos].job_no].special_flag ^= INTERRUPT_JOB;
                             }
#endif
                          }
                       }
                       else
                       {
#endif /* _WITH_BURST_2 */
                          if ((faulty = zombie_check(&connection[qb[qb_pos].connect_pos],
                                                     now, &qb_pos, 0)) == YES)
                          {
                             qb[qb_pos].pid = PENDING;
                             if (qb[qb_pos].msg_name[0] != '\0')
                             {
                                fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                                if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter == 1)
                                {
                                   /* Move all jobs from this host */
                                   /* to the error directory.      */
                                   for (i = 0; i < *no_msg_queued; i++)
                                   {
                                      if ((qb[i].in_error_dir != YES) &&
                                          (qb[i].pid == PENDING) &&
                                          (mdb[qb[i].pos].fsa_pos == mdb[qb[qb_pos].pos].fsa_pos))
                                      {
                                         to_error_dir(i);
                                      }
                                   }
                                }
                             }
                             else
                             {
                                fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                             }
                          }
                          else if (faulty == NO)
                               {
                                  remove_msg(qb_pos);
                               }
                          else if (faulty == NONE)
                               {
                                  if (qb[qb_pos].msg_name[0] != '\0')
                                  {
                                     fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                                  }
                                  else
                                  {
                                     fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                                  }
                                  qb[qb_pos].pid = PENDING;
                               }
#ifdef _WITH_BURST_2
                       }
#endif /* _WITH_BURST_2 */
                    }
                    bytes_done += sizeof(pid_t);
                 } while ((n > bytes_done) &&
                          ((n - bytes_done) >= sizeof(pid_t)));
                 if ((n - bytes_done) > 0)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "Reading garbage from fifo [%d] (%s %d)\n",
                              (n - bytes_done), __FILE__, __LINE__);
                 }

                 if ((stop_flag == 0) && (faulty != NEITHER) &&
                     (*no_msg_queued > 0))
                 {
                    /*
                     * If the number of messages queued is very large
                     * and most messages belong to a host that is very
                     * slow, new messages will only be processed very
                     * slowly when we always scan the whole queue.
                     */
                    if (*no_msg_queued < MAX_QUEUED_BEFORE_CECKED)
                    {
                       START_PROCESS();
                    }
                    else
                    {
                       if (loop_counter > ELAPSED_LOOPS_BEFORE_CHECK)
                       {
                          START_PROCESS();
                          loop_counter = 0;
                       }
                       else
                       {
                          loop_counter++;
                       }
                    }
                 }
              }
              else
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "read() error or reading garbage from fifo %s (%s %d)\n",
                           SF_FIN_FIFO, __FILE__, __LINE__);
              }
           } /* sf_xxx or gf_xxx PROCESS TERMINATED */

           /*
            * RETRY
            * =====
            */
      else if ((status > 0) && (FD_ISSET(retry_fd, &rset)))
           {
              int fsa_pos;

              if (read(retry_fd, &fsa_pos, sizeof(int)) != sizeof(int))
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Reading garbage from fifo %s (%s %d)\n",
                           RETRY_FD_FIFO, __FILE__, __LINE__);
              }
              else
              {
                 if (stop_flag == 0)
                 {
                    int qb_pos;

                    qb_pos_fsa(fsa_pos, &qb_pos);
                    if (qb_pos != -1)
                    {
#if defined _BURST_MODE || defined _AGE_LIMIT
                       if ((qb[qb_pos].pid = start_process(fsa_pos, qb_pos,
                                                           time(NULL),
                                                           YES)) == REMOVED)
                       {
                          remove_msg(qb_pos);
                       }
#else
                       qb[qb_pos].pid = start_process(fsa_pos, qb_pos,
                                                      time(NULL), YES);
#endif
                    }
                 }
              }
           } /* RETRY */
           
           /*
            * NEW MESSAGE ARRIVED
            * ===================
            */
      else if ((status > 0) && (FD_ISSET(msg_fifo_fd, &rset)))
           {
              int n;

              if ((n = read(msg_fifo_fd, fifo_buffer,
                            fifo_size)) >= msg_fifo_buf_size)
              {
                 int bytes_done = 0,
                     pos;

                 now = time(NULL);
                 do
                 {
#ifdef WITH_MULTI_FSA_CHECKS
                    if (fd_check_fsa() == YES)
                    {
                       if (check_fra_fd() == YES)
                       {
                          init_fra_data();
                       }
                       get_new_positions();
                       init_msg_buffer();
                    }
#endif /* WITH_MULTI_FSA_CHECKS */
                    (void)memcpy(msg_buffer, &fifo_buffer[bytes_done],
                                 msg_fifo_buf_size);

                    /* Queue the job order */
                    check_queue_space();
                    if ((pos = lookup_job_id(*job_id)) == INCORRECT)
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Could not locate job %d (%s %d)\n",
                                 *job_id, __FILE__, __LINE__);
                       if (*msg_priority != 0)
                       {
                          char del_dir[MAX_PATH_LENGTH];

                          (void)sprintf(del_dir, "%s%s/%c_%ld_%04d_%u",
                                        p_work_dir, AFD_FILE_DIR, *msg_priority,
                                        *creation_time, *unique_number, *job_id);
#ifdef _DELETE_LOG
                          remove_files(del_dir, -1, *job_id, OTHER_DEL);
#else
                          remove_files(del_dir, -1);
#endif
                       }
                       else
                       {
                          (void)rec(sys_log_fd, DEBUG_SIGN,
                                    "Hmmm. Priority data is NULL! Must be reading garbage! (%s %d)\n",
                                    __FILE__, __LINE__);
                       }
                    }
                    else
                    {
                       int    qb_pos;
                       double msg_number = ((double)*msg_priority - 47.0) *
                                           (((double)*creation_time * 10000.0) +
                                           (double)*unique_number);

                       if (*no_msg_queued > 0)
                       {
                          if (*no_msg_queued == 1)
                          {
                             if (qb[0].msg_number < msg_number)
                             {
                                qb_pos = 1;
                             }
                             else
                             {
                                size_t move_size;

                                move_size = *no_msg_queued *
                                            sizeof(struct queue_buf);
                                (void)memmove(&qb[1], &qb[0], move_size);
                                qb_pos = 0;
                             }
                          }
                          else
                          {
                             if (msg_number < qb[0].msg_number)
                             {
                                size_t move_size;

                                move_size = *no_msg_queued *
                                            sizeof(struct queue_buf);
                                (void)memmove(&qb[1], &qb[0], move_size);
                                qb_pos = 0;
                             }
                             else if (msg_number > qb[*no_msg_queued - 1].msg_number)
                                  {
                                     qb_pos = *no_msg_queued;
                                  }
                                  else
                                  {
                                     int center,
                                         end = *no_msg_queued - 1,
                                         start = 0;

                                     for (;;)
                                     {
                                        center = (end - start) / 2;
                                        if (center == 0)
                                        {
                                           size_t move_size;

                                           move_size = (*no_msg_queued - (start + 1)) *
                                                       sizeof(struct queue_buf);
                                           (void)memmove(&qb[start + 2], &qb[start + 1],
                                                         move_size);
                                           qb_pos = start + 1;
                                           break;
                                        }
                                        if (msg_number < qb[start + center].msg_number)
                                        {
                                           end = start + center;
                                        }
                                        else
                                        {
                                           start = start + center;
                                        }
                                     }
                                  }
                          }
                       }
                       else
                       {
                          qb_pos = 0;
                       }

                       (void)sprintf(qb[qb_pos].msg_name, "%c_%ld_%04d_%u",
                                     *msg_priority, *creation_time,
                                     *unique_number, *job_id);
                       qb[qb_pos].msg_number = msg_number;
                       qb[qb_pos].pid = PENDING;
                       qb[qb_pos].creation_time = *creation_time;
                       qb[qb_pos].pos = pos;
                       qb[qb_pos].connect_pos = -1;
                       qb[qb_pos].in_error_dir = NO;
                       (*no_msg_queued)++;
                       fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;

                       if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter > 0)
                       {
                          to_error_dir(qb_pos);
                       }
                    }
                    bytes_done += msg_fifo_buf_size;
                 } while (n > bytes_done);

                 if ((n == fifo_size) && (fifo_full_counter < 6))
                 {
                    fifo_full_counter++;
                 }
                 else
                 {
                    fifo_full_counter = 0;
                 }

                 /*
                  * Try to handle other queued files. If we do not do
                  * it here, where else?
                  */
                 if ((fifo_full_counter == 0) && (stop_flag == 0) &&
                     (*no_msg_queued > 0))
                 {
                    /*
                     * If the number of messages queued is very large
                     * and most messages belong to a host that is very
                     * slow, new messages will only be processed very
                     * slowly when we always scan the whole queue.
                     */
                    if (*no_msg_queued < MAX_QUEUED_BEFORE_CECKED)
                    {
                       START_PROCESS();
                    }
                    else
                    {
                       if (loop_counter > ELAPSED_LOOPS_BEFORE_CHECK)
                       {
                          START_PROCESS();
                          loop_counter = 0;
                       }
                       else
                       {
                          loop_counter++;
                       }
                    }
                 }
              }
              else
              {
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Hmmm. Seems like I am reading garbage from the fifo. (%s %d)\n",
                           __FILE__, __LINE__);
              }
           } /* NEW MESSAGE ARRIVED */

           /*
            * DELETE ALL JOBS FROM SPECIFIC HOST
            * ==================================
            */
      else if ((status > 0) && (FD_ISSET(delete_jobs_host_fd, &rset)))
           {
              int  n;

              if ((n = read(delete_jobs_host_fd, fifo_buffer, fifo_size)) > 0)
              {
                 int  bytes_done = 0,
                      fsa_pos,
                      j;
                 char *p_host_name = fifo_buffer,
                      *p_host_stored;

                 now = time(NULL);
                 do
                 {
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (qb[i].msg_name[0] != '\0')
                       {
                          p_host_stored = mdb[qb[i].pos].host_name;
                       }
                       else
                       {
                          p_host_stored = fra[qb[i].pos].host_alias;
                       }
                       if (CHECK_STRCMP(p_host_stored, p_host_name) == 0)
                       {
                          int fsa_pos;

                          /*
                           * Kill the job when it is currently distributing
                           * data.
                           */
                          if (qb[i].pid > 0)
                          {
                             if (kill(qb[i].pid, SIGKILL) < 0)
                             {
                                if (errno != ESRCH)
                                {
                                   (void)rec(sys_log_fd, WARN_SIGN,
                                             "Failed to kill transfer job to %s (%d) : %s (%s %d)\n",
                                             p_host_stored,
                                             qb[i].pid, strerror(errno),
                                             __FILE__, __LINE__);
                                }
                             }
                             else
                             {
                                remove_connection(&connection[qb[i].connect_pos],
                                                  NO, now);
                             }
                          }

                          if (qb[i].msg_name[0] != '\0')
                          {
                             char *p_job_dir,
                                  *ptr;

                             if (qb[i].in_error_dir == YES)
                             {
                                p_job_dir = err_file_dir;
                                ptr = p_job_dir + strlen(p_job_dir);
                                (void)sprintf(ptr, "%s/%s",
                                              mdb[qb[i].pos].host_name,
                                              qb[i].msg_name);
                             }
                             else
                             {
                                p_job_dir = file_dir;
                                ptr = p_job_dir + strlen(p_job_dir);
                                (void)strcpy(ptr, qb[i].msg_name);
                             }
#ifdef _DELETE_LOG
                             remove_files(p_job_dir,
                                          mdb[qb[i].pos].fsa_pos,
                                          mdb[qb[i].pos].job_id,
                                          USER_DEL);
#else
                             remove_files(p_job_dir, -1);
#endif
                             *ptr = '\0';
                             fsa_pos = mdb[qb[i].pos].fsa_pos;
                          }
                          else
                          {
                             fsa_pos = fra[qb[i].pos].fsa_pos;
                          }

                          if (qb[i].pid < 1)
                          {
                             ABS_REDUCE(fsa_pos);
                          }
                          remove_msg(i);
                          if (i < *no_msg_queued)
                          {
                             i--;
                          }
                       }
                    } /* for (i = 0; i < *no_msg_queued; i++) */

                    /*
                     * Hmmm. Best is to reset ALL values, so we do not
                     * need to start and stop the FD to sort out any
                     * problems in the FSA.
                     */
                    if ((fsa_pos = get_host_position(fsa,
                                                     p_host_name,
                                                     no_of_hosts)) != INCORRECT)
                    {
                       fsa[fsa_pos].total_file_counter = 0;
                       fsa[fsa_pos].total_file_size = 0;
                       fsa[fsa_pos].active_transfers = 0;
                       fsa[fsa_pos].error_counter = 0;
                       fsa[fsa_pos].jobs_queued = 0;
                       for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
                       {
                          fsa[fsa_pos].job_status[j].no_of_files = 0;
                          fsa[fsa_pos].job_status[j].proc_id = -1;
                          fsa[fsa_pos].job_status[j].connect_status = DISCONNECT;
                          fsa[fsa_pos].job_status[j].file_name_in_use[0] = '\0';
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
           } /* DELETE ALL JOBS FROM SPECIFIC HOST */

           /*
            * DELETE SPECIFIC JOBS
            * ====================
            */
      else if ((status > 0) && (FD_ISSET(delete_jobs_fd, &rset)))
           {
              int  n;

              if ((n = read(delete_jobs_fd, fifo_buffer, fifo_size)) > 0)
              {
                 int  bytes_done = 0;
                 char *p_msg_name = fifo_buffer;

                 now = time(NULL);

                 do
                 {
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (CHECK_STRCMP(qb[i].msg_name, p_msg_name) == 0)
                       {
                          char *p_job_dir,
                               *ptr;

                          /*
                           * Kill the job when it is currently distributing
                           * data.
                           */
                          if (qb[i].pid > 0)
                          {
                             if (kill(qb[i].pid, SIGKILL) < 0)
                             {
                                if (errno != ESRCH)
                                {
                                   (void)rec(sys_log_fd, WARN_SIGN,
                                             "Failed to kill transfer job to %s (%d) : %s (%s %d)\n",
                                             mdb[qb[i].pos].host_name,
                                             qb[i].pid, strerror(errno),
                                             __FILE__, __LINE__);
                                }
                             }
                             else
                             {
                                remove_connection(&connection[qb[i].connect_pos],
                                                  NO, now);
                             }
                          }

                          if (qb[i].in_error_dir == YES)
                          {
                             p_job_dir = err_file_dir;
                             ptr = p_job_dir + strlen(p_job_dir);
                             (void)sprintf(ptr, "%s/%s",
                                           mdb[qb[i].pos].host_name,
                                           qb[i].msg_name);
                          }
                          else
                          {
                             p_job_dir = file_dir;
                             ptr = p_job_dir + strlen(p_job_dir);
                             (void)strcpy(ptr, qb[i].msg_name);
                          }
#ifdef _DELETE_LOG
                          remove_files(p_job_dir,
                                       mdb[qb[i].pos].fsa_pos,
                                       mdb[qb[i].pos].job_id,
                                       USER_DEL);
#else
                          remove_files(p_job_dir, -1);
#endif
                          *ptr = '\0';

                          if (qb[i].pid < 1)
                          {
                             ABS_REDUCE(mdb[qb[i].pos].fsa_pos);
                          }
                          remove_msg(i);
                          break;
                       }
                    } /* for (i = 0; i < *no_msg_queued; i++) */
                    while ((*p_msg_name != '\0') && (bytes_done < n))
                    {
                       p_msg_name++;
                       bytes_done++;
                    }
                    if ((*p_msg_name == '\0') && (bytes_done < n))
                    {
                       p_msg_name++;
                       bytes_done++;
                    }
                 } while (n > bytes_done);
              }
           } /* DELETE SPECIFIC JOBS */

           /*
            * TIMEOUT or WAKE-UP (Start/Stop Transfer)
            * ==================
            */
      else if ((status == 0) || (FD_ISSET(fd_wake_up_fd, &rset)))
           {
              /* Clear wake-up FIFO if necessary. */
              if (FD_ISSET(fd_wake_up_fd, &rset))
              {
                 if (read(fd_wake_up_fd, fifo_buffer, fifo_size) < 0)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "read() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                 }
              }

              if (stop_flag == 0)
              {
                 START_PROCESS();
              }
              else
              {
                 /* Lets not wait too long. It could be that one of the */
                 /* sending process is in an infinite loop and we could */
                 /* wait forever. So after a certain time kill(!) all   */
                 /* jobs and then exit.                                 */
                 loop_counter++;
                 if ((stop_flag == SAVE_STOP) || (stop_flag == STOP))
                 {
                    if ((loop_counter * FD_RESCAN_TIME) > FD_TIMEOUT)
                    {
                       break; /* To get out of for(;;) loop */
                    }
                 }
                 else
                 {
                    if ((loop_counter * FD_RESCAN_TIME) > FD_QUICK_TIMEOUT)
                    {
                       break; /* To get out of for(;;) loop */
                    }
                 }
              }
           } /* TIMEOUT or WAKE-UP (Start/Stop Transfer) */

           /*
            * SELECT ERROR
            * ============
            */
      else if (status < 0)
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "Select error : %s (%s %d)\n",
                        strerror(errno),  __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ start_process() ++++++++++++++++++++++++++*/
static pid_t
start_process(int fsa_pos, int qb_pos, time_t current_time, int retry)
{
   pid_t pid = PENDING;

#ifdef _AGE_LIMIT
   if ((qb[qb_pos].msg_name[0] != '\0') &&
       (mdb[qb[qb_pos].pos].age_limit > 0) &&
       ((current_time - qb[qb_pos].creation_time) > mdb[qb[qb_pos].pos].age_limit))
   {
      char del_dir[MAX_PATH_LENGTH];

      if (qb[qb_pos].in_error_dir == YES)
      {
         (void)sprintf(del_dir, "%s%s%s/%s/%s",
                       p_work_dir, AFD_FILE_DIR, ERROR_DIR,
                       fsa[fsa_pos].host_alias, qb[qb_pos].msg_name);
      }
      else
      {
         (void)sprintf(del_dir, "%s%s/%s",
                       p_work_dir, AFD_FILE_DIR, qb[qb_pos].msg_name);
      }
#ifdef _DELETE_LOG
      remove_files(del_dir, fsa_pos, mdb[qb[qb_pos].pos].job_id, AGE_OUTPUT);
#else
      remove_files(del_dir, fsa_pos);
#endif
      ABS_REDUCE(fsa_pos);
      pid = REMOVED;
   }
   else
#endif /* _AGE_LIMIT */
   {
      if (((fsa[fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
          ((fsa[fsa_pos].error_counter == 0) || (retry == YES) ||
           ((fsa[fsa_pos].active_transfers == 0) &&
           ((fsa[fsa_pos].special_flag & ERROR_FILE_UNDER_PROCESS) == 0) &&
           ((current_time - (fsa[fsa_pos].last_retry_time + fsa[fsa_pos].retry_interval)) >= 0))))
      {
         if ((p_afd_status->no_of_transfers < max_connections) &&
             (fsa[fsa_pos].active_transfers < fsa[fsa_pos].allowed_transfers))
         {
            int pos;

            if ((pos = get_free_connection()) == INCORRECT)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to get free connection. (%s %d)\n",
                         __FILE__, __LINE__);
            }
            else
            {
               if ((connection[pos].job_no = get_free_disp_pos(fsa_pos)) != INCORRECT)
               {
                  /*
                   * When the host can be toggled and auto toggle
                   * is in effect, we must check here if we are
                   * sending to the original host. If this is not
                   * the case lets temporally switch back to the
                   * original host to see if it is working now.
                   */
                  connection[pos].error_file = qb[qb_pos].in_error_dir;
                  if (qb[qb_pos].msg_name[0] == '\0')
                  {
                     connection[pos].fra_pos = qb[qb_pos].pos;
                     connection[pos].protocol = fra[qb[qb_pos].pos].protocol;
                     connection[pos].msg_name[0] = '\0';
                     (void)memcpy(connection[pos].dir_alias,
                                  fra[qb[qb_pos].pos].dir_alias,
                                  MAX_DIR_ALIAS_LENGTH + 1);
                  }
                  else
                  {
                     connection[pos].fra_pos = -1;
                     connection[pos].protocol = mdb[qb[qb_pos].pos].type;
                     (void)memcpy(connection[pos].msg_name, qb[qb_pos].msg_name,
                                  MAX_MSG_NAME_LENGTH);
                     connection[pos].dir_alias[0] = '\0';
                  }
                  connection[pos].temp_toggle = OFF;
                  (void)memcpy(connection[pos].hostname, fsa[fsa_pos].host_alias,
                               MAX_HOSTNAME_LENGTH + 1);
                  connection[pos].fsa_pos = fsa_pos;
#ifdef WITH_MULTI_FSA_CHECKS
                  if (fd_check_fsa() == YES)
                  {
                     if (check_fra_fd() == YES)
                     {
                        init_fra_data();
                     }

                     /*
                      * We need to set the connection[pos].pid to a
                      * value higher then 0 so the function get_new_positions()
                      * also locates the new connection[pos].fsa_pos. Otherwise
                      * from here on we point to some completely different
                      * host and this can cause havoc when someone uses
                      * edit_hc and changes the alias order.
                      */
                     connection[pos].pid = 1;
                     get_new_positions();
                     connection[pos].pid = 0;
                     init_msg_buffer();
                     fsa_pos = connection[pos].fsa_pos;
                  }
#endif /* WITH_MULTI_FSA_CHECKS */
                  (void)strcpy(fsa[fsa_pos].job_status[connection[pos].job_no].unique_name, qb[qb_pos].msg_name);
                  if ((fsa[fsa_pos].error_counter == 0) &&
                      (fsa[fsa_pos].auto_toggle == ON) &&
                      (fsa[fsa_pos].original_toggle_pos != NONE) &&
                      (fsa[fsa_pos].max_successful_retries > 0))
                  {
                     if ((fsa[fsa_pos].original_toggle_pos == fsa[fsa_pos].toggle_pos) &&
                         (fsa[fsa_pos].successful_retries > 0))
                     {
                        fsa[fsa_pos].original_toggle_pos = NONE;
                        fsa[fsa_pos].successful_retries = 0;
                     }
                     else if (fsa[fsa_pos].successful_retries >= fsa[fsa_pos].max_successful_retries)
                          {
                             connection[pos].temp_toggle = ON;
                             fsa[fsa_pos].successful_retries = 0;
                          }
                          else
                          {
                             fsa[fsa_pos].successful_retries++;
                          }
                  }

                  /* Create process to distribute file. */
                  if ((connection[pos].pid = make_process(&connection[pos])) > 0)
                  {
                     pid = fsa[fsa_pos].job_status[connection[pos].job_no].proc_id = connection[pos].pid;
                     fsa[fsa_pos].active_transfers += 1;
                     ABS_REDUCE(fsa_pos);
                     if (fsa[fsa_pos].error_counter > 0)
                     {
                        fsa[fsa_pos].special_flag |= ERROR_FILE_UNDER_PROCESS;
                     }
                     qb[qb_pos].connect_pos = pos;
                     p_afd_status->no_of_transfers++;
                  }
               }
            }
         }
#ifdef _BURST_MODE
         else if (qb[qb_pos].msg_name[0] != '\0')
              {
                 int limit = NO;

                 if (qb[qb_pos].in_error_dir == YES)
                 {
                    (void)sprintf(p_src_dir, "%s/%s/%s", ERROR_DIR,
                                  fsa[fsa_pos].host_alias, qb[qb_pos].msg_name);
                 }
                 else
                 {
                    (void)strcpy(p_src_dir, qb[qb_pos].msg_name);
                 }
                 if ((*no_msg_queued > 1) && (*no_msg_queued < 500))
                 {
                    int i = 0;

                    do
                    {
                       if (i != qb_pos)
                       {
                          if ((qb[qb_pos].msg_name[0] > qb[i].msg_name[0]) &&
                              (CHECK_STRCMP(mdb[qb[qb_pos].pos].host_name, mdb[qb[i].pos].host_name) == 0))
                          {
                             limit = YES;
                             break;
                          }
                       }
                       i++;
                    } while (i < *no_msg_queued);
                 }
                 else if (*no_msg_queued >= 500)
                      {
                         limit = YES;
                      }
                 if (check_burst(mdb[qb[qb_pos].pos].type, fsa_pos,
                                 mdb[qb[qb_pos].pos].job_id,
                                 src_dir, file_dir, limit) == YES)
                 {
                    ABS_REDUCE(fsa_pos);
                    pid = REMOVED;
                 }
              }
#endif /* _BURST_MODE */
      }
   }
   return(pid);
}


/*---------------------------- make_process() ---------------------------*/
static pid_t
make_process(struct connection *con)
{
   int   argcount;
   pid_t pid;
   char  *args[12],
         str_job_no[3],
         str_age_limit[MAX_INT_LENGTH];


   if (con->job_no < 10)
   {
      str_job_no[0] = con->job_no + '0';
      str_job_no[1] = '\0';
   }
   else if (con->job_no < 100)
        {
           str_job_no[0] = (con->job_no / 10) + '0';
           str_job_no[1] = (con->job_no % 10) + '0';
           str_job_no[2] = '\0';
        }
        else
        {
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "Get the programmer to expand the job number! Or else you are in deep trouble! (%s %d)\n",
                     __FILE__, __LINE__);
           str_job_no[0] = ((con->job_no / 10) % 10) + '0';
           str_job_no[1] = (con->job_no % 10) + '0';
           str_job_no[2] = '\0';
        }

   if (con->protocol == FTP)
   {
      if (con->msg_name[0] == '\0')
      {
         args[0] = GET_FILE_FTP;
      }
      else
      {
         args[0] = SEND_FILE_FTP;
      }
   }
   else if (con->protocol == LOC)
        {
           args[0] = SEND_FILE_LOC;
        }
#ifdef _WITH_SCP1_SUPPORT
   else if (con->protocol == SCP1)
        {
           args[0] = SEND_FILE_SCP1;
        }
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
   else if (con->protocol == WMO)
        {
           args[0] = SEND_FILE_WMO;
        }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
   else if (con->protocol == MAP)
        {
           args[0] = SEND_FILE_MAP;
        }
#endif /* _WITH_MAP_SUPPORT */
   else if (con->protocol == SMTP)
        {
           args[0] = SEND_FILE_SMTP;
        }
        else
        {
           (void)rec(sys_log_fd, DEBUG_SIGN,
                     ".....? Unknown protocol (%d) (%s %d)\n",
                     con->protocol, __FILE__, __LINE__);
           return(INCORRECT);
        }
   args[1] = WORK_DIR_ID;
   args[2] = p_work_dir;
   args[3] = "-j";
   args[4] = str_job_no;
   if ((default_age_limit > 0) && (con->msg_name[0] != '\0'))
   {
      args[5] = "-a";
      (void)sprintf(str_age_limit, "%u", default_age_limit);
      args[6] = str_age_limit;
      argcount = 7;
   }
   else
   {
      argcount = 5;
   }
   if (con->error_file == YES)
   {
      if (con->temp_toggle == ON)
      {
         args[argcount] = "-f";
         args[argcount + 1] = "-t";
         args[argcount + 2] = "-m";
         args[argcount + 3] = con->msg_name;
         args[argcount + 4] = NULL;
      }
      else
      {
         args[argcount] = "-f";
         args[argcount + 1] = "-m";
         args[argcount + 2] = con->msg_name;
         args[argcount + 3] = NULL;
      }
   }
   else
   {
      if (con->temp_toggle == ON)
      {
         if (con->msg_name[0] == '\0')
         {
            args[argcount] = "-t";
            args[argcount + 1] = "-d";
            args[argcount + 2] = fra[con->fra_pos].dir_alias;
            args[argcount + 3] = NULL;
         }
         else
         {
            args[argcount] = "-t";
            args[argcount + 1] = "-m";
            args[argcount + 2] = con->msg_name;
            args[argcount + 3] = NULL;
         }
      }
      else
      {
         if (con->msg_name[0] == '\0')
         {
            args[argcount] = "-d";
            args[argcount + 1] = fra[con->fra_pos].dir_alias;
            args[argcount + 2] = NULL;
         }
         else
         {
            args[argcount] = "-m";
            args[argcount + 1] = con->msg_name;
            args[argcount + 2] = NULL;
         }
      }
   }

   switch(pid = fork())
   {
      case -1 :

         /* Could not generate process */
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not create a new process : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         return(INCORRECT);

      case  0 : /* Child process */

         execvp(args[0], args);
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to start process %s : %s (%s %d)\n",
                   args[0], strerror(errno), __FILE__, __LINE__);
         pid = getpid();
         if (write(read_fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
         {
            (void)rec(sys_log_fd, ERROR_SIGN, "write() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         _exit(INCORRECT);

      default :

         /* Parent process */
         p_afd_status->fd_fork_counter++;
         return(pid);
   }
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*
 * Description : Checks if any process is finished (zombie), if this
 *               is the case it is killed with waitpid().
 */
static int
zombie_check(struct connection *p_con,
             time_t            now,
             int               *qb_pos,
             int               options)
{
   int   faulty = YES,
         status;
   pid_t ret;

   /* Wait for process to terminate. */
   if ((ret = waitpid(p_con->pid, &status, options)) == p_con->pid)
   {
      if (WIFEXITED(status))
      {
         int exit_status;

         switch(exit_status = WEXITSTATUS(status))
         {
            case STILL_FILES_TO_SEND   :
            case TRANSFER_SUCCESS      : /* Ordinary end of process. */
               faulty = NO;
               if (((p_con->temp_toggle == ON) &&
                    (fsa[p_con->fsa_pos].original_toggle_pos != fsa[p_con->fsa_pos].host_toggle)) ||
                   (fsa[p_con->fsa_pos].original_toggle_pos == fsa[p_con->fsa_pos].host_toggle))
               {
                  /*
                   * Do not forget to toggle back to the original
                   * host and deactivate original_toggle_pos!
                   */
                  p_con->temp_toggle = OFF;
                  fsa[p_con->fsa_pos].successful_retries = 0;
                  if (fsa[p_con->fsa_pos].original_toggle_pos != NONE)
                  {
                     fsa[p_con->fsa_pos].host_toggle = fsa[p_con->fsa_pos].original_toggle_pos;
                     fsa[p_con->fsa_pos].original_toggle_pos = NONE;
                     fsa[p_con->fsa_pos].host_dsp_name[(int)fsa[p_con->fsa_pos].toggle_pos] = fsa[p_con->fsa_pos].host_toggle_str[(int)fsa[p_con->fsa_pos].host_toggle];
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Switching back to host <%s> after successful transfer.\n",
                               fsa[p_con->fsa_pos].host_dsp_name);
                  }
               }
               if (fsa[p_con->fsa_pos].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
               {
                  fsa[p_con->fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
                  (void)rec(sys_log_fd, INFO_SIGN,
                            "Started input queue for host <%s>, due to to many jobs in the error directory. (%s %d)\n",
                            fsa[p_con->fsa_pos].host_alias,
                            __FILE__, __LINE__);
               }
               fsa[p_con->fsa_pos].last_connection = now;
               if (exit_status == STILL_FILES_TO_SEND)
               {
                  next_dir_check_time = 0L;
               }
               break;

            case SYNTAX_ERROR          : /* Syntax for sf_xxx/gf_xxx wrong. */
#ifdef WITH_MULTI_FSA_CHECKS
               if (fd_check_fsa() == YES)
               {
                  if (check_fra_fd() == YES)
                  {
                     init_fra_data();
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
#endif /* WITH_MULTI_FSA_CHECKS */
               fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = NOT_WORKING;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[0] = '\0';

               /*
                * Lets assume that in the case for sf_xxx the message is
                * moved to the faulty directory. So no further action is
                * necessary.
                */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Syntax for calling program wrong. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            case NO_MESSAGE_FILE : /* The message file has disappeared. */
                                   /* Remove the job, or else we        */
                                   /* will always fall for this one     */
                                   /* again.                            */
               {
                  char del_dir[MAX_PATH_LENGTH];

                  if (p_con->error_file == YES)
                  {
                     (void)sprintf(del_dir, "%s%s%s/%s/%s", p_work_dir,
                                   AFD_FILE_DIR, ERROR_DIR,
                                   fsa[p_con->fsa_pos].host_alias,
                                   p_con->msg_name);
                  }
                  else
                  {
                     (void)sprintf(del_dir, "%s%s/%s", p_work_dir,
                                   AFD_FILE_DIR, p_con->msg_name);
                  }
#ifdef _DELETE_LOG
                  remove_files(del_dir, -1,
                               fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id,
                               OTHER_DEL);
#else
                  remove_files(del_dir, -1);
#endif
               }
               break;

            case JID_NUMBER_ERROR      : /* Hmm, failed to determine JID */
                                         /* number, lets assume the      */
                                         /* queue entry is corrupted.    */

               /* Note: We have to trust the queue here to get the correct  */
               /*       connect position in struct connection. How else do  */
               /*       we know which values are to be reset in the connect */
               /*       structure!? :-((((                                  */
               faulty = NO;
               break;

            case OPEN_FILE_DIR_ERROR   : /* File directory does not exist */
               /*
                * Since sf_xxx has already reported this incident, no
                * need to do it here again.
                */
               faulty = NO;
               break;

            case MAIL_ERROR            : /* Failed to send mail to remote host */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Failed to send mail. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            case TIMEOUT_ERROR         : /* Timeout arrived */
            case CONNECT_ERROR         : /* Failed to connect to remote host */
            case USER_ERROR            : /* User name wrong */
            case TYPE_ERROR            : /* Setting transfer type failed */
            case LIST_ERROR            : /* Sending the LIST command failed  */
            case REMOTE_USER_ERROR     : /* Failed to send mail address. */
            case DATA_ERROR            : /* Failed to send data command. */
            case READ_LOCAL_ERROR      : /* */
            case WRITE_REMOTE_ERROR    : /* */
            case WRITE_LOCAL_ERROR     : /* */
            case READ_REMOTE_ERROR     : /* */
            case SIZE_ERROR            : /* */
            case DATE_ERROR            : /* */
            case OPEN_LOCAL_ERROR      : /* */
            case WRITE_LOCK_ERROR      : /* */
#ifdef _WITH_WMO_SUPPORT
            case CHECK_REPLY_ERROR     : /* Did not get a correct reply. */
#endif
            case REMOVE_LOCKFILE_ERROR : /* */
            case QUIT_ERROR            : /* Failed to disconnect. */
            case STAT_ERROR            : /* Failed to stat() file/directory */
            case RENAME_ERROR          : /* Rename file locally */
            case SELECT_ERROR          : /* selecting on sf_xxx command fifo */
#ifdef _WITH_WMO_SUPPORT
            case SIG_PIPE_ERROR        : /* When sf_wmo receives a SIGPIPE */
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP_FUNCTION_ERROR    : /* MAP function call has failed. */
#endif
               break;

            case PASSWORD_ERROR        : /* Password wrong */
            case CHDIR_ERROR           : /* Change remote directory */
            case CLOSE_REMOTE_ERROR    : /* */
            case MOVE_ERROR            : /* Move file locally */
            case MOVE_REMOTE_ERROR     : /* */
            case OPEN_REMOTE_ERROR     : /* Failed to open remote file. */
               if ((qb[*qb_pos].msg_name[0] != '\0') &&
                   (qb[*qb_pos].msg_number < 199999999999999.0) &&
                   ((*qb_pos + 1) < *no_msg_queued))
               {
                  register int i = *qb_pos + 1;

                  /*
                   * Increase the message number, so that this job
                   * will decrease in priority and resort the queue.
                   * The number is actually increased by 600 seconds
                   * (10 minutes).
                   */
                  qb[*qb_pos].msg_number += 60000000.0;
                  while ((i < *no_msg_queued) &&
                         (qb[*qb_pos].msg_number > qb[i].msg_number))
                  {
                     i++;
                  }
                  if (i > (*qb_pos + 1))
                  {
                     size_t           move_size;
                     struct queue_buf tmp_qb;

                     (void)memcpy(&tmp_qb, &qb[*qb_pos],
                                  sizeof(struct queue_buf));
                     i--;
                     move_size = (i - *qb_pos) * sizeof(struct queue_buf);
                     (void)memmove(&qb[*qb_pos], &qb[*qb_pos + 1], move_size);
                     (void)memcpy(&qb[i], &tmp_qb, sizeof(struct queue_buf));
                     *qb_pos = i;
                  }
               }
               break;

            case STAT_LOCAL_ERROR      : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Could not stat() local file. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            case LOCK_REGION_ERROR     : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to lock region. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            case UNLOCK_REGION_ERROR   : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to unlock region. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            case GOT_KILLED : /* Process has been killed, most properly */
                              /* by this process.                       */
               faulty = NONE;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = DISCONNECT;
               break;

            case NO_FILES_TO_SEND : /* There are no files to send. Most */
                                    /* properly the files have benn     */
                                    /* deleted due to age.              */
               remove_connection(p_con, NEITHER, now);

               /*
                * This is actually a good time to check if there are
                * at all any files to be send. If NOT and the auto
                * pause queue flag is set, we might get a deadlock.
                */
               if (p_con->fsa_pos != -1)
               {
                  if ((fsa[p_con->fsa_pos].total_file_counter == 0) &&
                      (fsa[p_con->fsa_pos].total_file_size == 0) &&
                      (fsa[p_con->fsa_pos].host_status & AUTO_PAUSE_QUEUE_STAT))
                  {
                     fsa[p_con->fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Starting input queue for %s that was stopped by init_afd. (%s %d)\n",
                                fsa[p_con->fsa_pos].host_alias,
                                __FILE__, __LINE__);
                  }
               }
               return(NO);

            case ALLOC_ERROR           : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Failed to allocate memory. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         __FILE__, __LINE__);
               break;

            default                    : /* Unknown error. */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected due to an unknown error (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname, p_con->job_no,
                         exit_status, __FILE__, __LINE__);
               break;
         }

         /*
          * When auto_toggle is active and we have just tried
          * the original host, lets not slow things done by
          * making this appear as an error. The second host
          * might be perfectly okay, lets continue sending
          * files as quickly as possible. So when temp_toggle
          * is ON, it may NEVER be faulty.
          */
         if ((p_con->temp_toggle == ON) && (faulty == YES))
         {
            faulty = NONE;
         }
      }
      else  if (WIFSIGNALED(status))
            {
               /* abnormal termination */
#ifdef WITH_MULTI_FSA_CHECKS
               if (fd_check_fsa() == YES)
               {
                  if (check_fra_fd() == YES)
                  {
                     init_fra_data();
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
#endif /* WITH_MULTI_FSA_CHECKS */
               fsa[p_con->fsa_pos].job_status[p_con->job_no].connect_status = NOT_WORKING;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].no_of_files_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_size_in_use_done = 0;
               fsa[p_con->fsa_pos].job_status[p_con->job_no].file_name_in_use[0] = '\0';

               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Abnormal termination of transfer job (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, p_con->hostname,
                         p_con->job_no, p_con->pid, __FILE__, __LINE__);
            }

      remove_connection(p_con, faulty, now);

      /*
       * Even if we did fail to send a file, lets set the transfer
       * time. Otherwise jobs will get deleted to early together
       * with their current files if no transfer was successful
       * and we did a reread DIR_CONFIG.
       */
      if (qb[*qb_pos].msg_name[0] != '\0')
      {
         mdb[qb[*qb_pos].pos].last_transfer_time = now;
      }
   } /* if (waitpid(p_con->pid, &status, 0) == p_con->pid) */
   else
   {
      if (ret == -1)
      {
         int tmp_errno = errno;

         (void)rec(sys_log_fd, ERROR_SIGN,
                   "waitpid() error [%d] : %s (%s %d)\n",
                   p_con->pid, strerror(errno), __FILE__, __LINE__);
         if (tmp_errno == ECHILD)
         {
            faulty = NONE;
            remove_connection(p_con, NONE, now);
         }
      }
      else
      {
         faulty = NEITHER;
      }
   }

   return(faulty);
}


/*---------------------------- to_error_dir() --------------------------*/
static void
to_error_dir(int pos)
{
   struct stat stat_buf;

   /*
    * First, lets move the files to the file error directory.
    */
   (void)strcpy(p_err_file_dir, fsa[mdb[qb[pos].pos].fsa_pos].host_alias);
   if ((stat(err_file_dir, &stat_buf) < 0) || (!S_ISDIR(stat_buf.st_mode)))
   {
      if (mkdir(err_file_dir, DIR_MODE) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to create directory %s : %s (%s %d)\n",
                   err_file_dir, strerror(errno), __FILE__, __LINE__);
         *p_err_file_dir = '\0';
         return;
      }
   }
   else
   {
      /*
       * Always check if the number of error files does not exceed
       * the maximum allowed. By this we ensure we do not create to
       * many directories in the error directory.
       */
      if (((fsa[mdb[qb[pos].pos].fsa_pos].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT) == 0) &&
#ifdef _LINK_MAX_TEST
          (stat_buf.st_nlink >= LINKY_MAX))
#else
#ifdef REDUCED_LINK_MAX
          (stat_buf.st_nlink >= REDUCED_LINK_MAX))
#else
          (stat_buf.st_nlink >= LINK_MAX))
#endif
#endif
      {
         fsa[mdb[qb[pos].pos].fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Stopped input queue for host %s, since the number jobs are reaching a dangerous level. (%s %d)\n",
                   fsa[mdb[qb[pos].pos].fsa_pos].host_alias,
                   __FILE__, __LINE__);
      }
   }
   (void)strcat(p_err_file_dir, "/");
   (void)strcat(p_err_file_dir, qb[pos].msg_name);
   (void)strcpy(p_file_dir, qb[pos].msg_name);

   if (rename(file_dir, err_file_dir) < 0)
   {
      if (errno == EMLINK)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Error jobs exceeded %d, so no more links are possible. (%s %d)\n",
#ifdef _LINK_MAX_TEST
                   (LINKY_MAX - 2), __FILE__, __LINE__);
#else
#ifdef REDUCED_LINK_MAX
                   (REDUCED_LINK_MAX - 2), __FILE__, __LINE__);
#else
                   (LINK_MAX - 2), __FILE__, __LINE__);
#endif
#endif
         if ((fsa[mdb[qb[pos].pos].fsa_pos].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT) == 0)
         {
            fsa[mdb[qb[pos].pos].fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Stopped input queue for host %s, since the number jobs are reaching a dangerous level. (%s %d)\n",
                      fsa[mdb[qb[pos].pos].fsa_pos].host_alias,
                      __FILE__, __LINE__);
         }
      }
      else if (errno == ENOSPC)
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Deleting job. No more space left on device. (%s %d)\n",
                        __FILE__, __LINE__);
           }
      else if (errno == ENOENT) /* No such file or directory */
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "File(s) in directory %s have already been transfered. (%s %d)\n",
                        file_dir, __FILE__, __LINE__);
           }
           else
           {
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Failed to rename() %s to %s : %s (%s %d)\n",
                        file_dir, err_file_dir, strerror(errno),
                        __FILE__, __LINE__);
           }

      /*
       * Since we failed to move the files, note this in the qb structure.
       */
      qb[pos].in_error_dir = NO;
   }
   else
   {
      qb[pos].in_error_dir = YES;
   }
   *p_file_dir = '\0';
   *p_err_file_dir = '\0';

   return;
}


/*++++++++++++++++++++++++++++ qb_pos_pid() +++++++++++++++++++++++++++++*/
static void
qb_pos_pid(pid_t pid, int *qb_pos)
{
   register int i;

   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == pid)
      {
         *qb_pos = i;
         return;
      }
   }
   *qb_pos = -1;

   return;
}


/*------------------------ recount_jobs_queued() ------------------------*/
static int
recount_jobs_queued(int fsa_pos)
{
   register int i,
                jobs_queued = 0;

   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == PENDING)
      {
         if (qb[i].msg_name[0] != '\0')
         {
            if (fsa_pos == mdb[qb[i].pos].fsa_pos)
            {
               jobs_queued++;
            }
         }
         else
         {
            if (fsa_pos == fra[qb[i].pos].fsa_pos)
            {
               jobs_queued++;
            }
         }
      }
   }
   return(jobs_queued);
}


/*++++++++++++++++++++++++++++ qb_pos_fsa() +++++++++++++++++++++++++++++*/
static void
qb_pos_fsa(int fsa_pos, int *qb_pos)
{
   register int i, j;

   *qb_pos = -1;
   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid == PENDING)
      {
         if (qb[i].msg_name[0] != '\0')
         {
            for (j = 0; j < *no_msg_cached; j++)
            {
               if ((fsa_pos == mdb[j].fsa_pos) && (qb[i].pos == j))
               {
                  *qb_pos = i;
                  return;
               }
            }
         }
         else
         {
            if (fsa_pos == fra[qb[i].pos].fsa_pos)
            {
               *qb_pos = i;
               return;
            }
         }
      }
   }
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "No message for %s in queue that is PENDING. (%s %d)\n",
             fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);

   return;
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_CONNECTIONS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_connections = atoi(value);
         if ((max_connections < 1) ||
             (max_connections > MAX_CONFIGURABLE_CONNECTIONS))
         {
            max_connections = MAX_DEFAULT_CONNECTIONS;
         }
      }
      if (get_definition(buffer, REMOTE_FILE_CHECK_INTERVAL_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         remote_file_check_interval = atoi(value);
         if (remote_file_check_interval < 1)
         {
            remote_file_check_interval = DEFAULT_REMOTE_FILE_CHECK_INTERVAL;
         }
      }
      if (get_definition(buffer, MAX_OUTPUT_LOG_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_output_log_files = atoi(value);
         if ((max_output_log_files < 1) || (max_output_log_files > 599))
         {
            max_output_log_files = MAX_OUTPUT_LOG_FILES;
         }
      }
      if (get_definition(buffer, DEFAULT_AGE_LIMIT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_age_limit = (unsigned int)atoi(value);
      }
      free(buffer);
   }

   return;
}


/*++++++++++++++++++++++++++ get_free_connection() ++++++++++++++++++++++*/
static int
get_free_connection(void)
{
   register int i;

   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].hostname[0] == '\0')
      {
         return(i);
      }
   }

   return(INCORRECT);
}


/*+++++++++++++++++++++++++ get_free_disp_pos() +++++++++++++++++++++++++*/
static int
get_free_disp_pos(int pos)
{
   register int i;

   for (i = 0; i < fsa[pos].allowed_transfers; i++)
   {
      if (fsa[pos].job_status[i].proc_id < 1)
      {
         return(i);
      }
   }

   /*
    * This should be impossible. But it could happen that when
    * we reduce the 'active_transfers' the next process is started
    * before we get a change to reset the 'connect_status' variable.
    */
   if ((pos >= 0) && (pos < no_of_hosts))
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Hmm. No display position free for %s [%d]. (%s %d)\n",
                fsa[pos].host_dsp_name, pos,
                __FILE__, __LINE__);
   }
   else
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Hmm. No display position free for FSA position %d. (%s %d)\n",
                pos, __FILE__, __LINE__);
   }

   /*
    * This is a good opertunity to check if the process for this
    * host still exist. If not lets simply reset all relevant
    * parameters of the job_status structure.
    */
   for (i = 0; i < fsa[pos].allowed_transfers; i++)
   {
      if (fsa[pos].job_status[i].proc_id > -1)
      {
         if (kill(fsa[pos].job_status[i].proc_id, 0) == -1)
         {
            fsa[pos].job_status[i].proc_id = -1;
#ifdef _BURST_MODE
            fsa[pos].job_status[i].unique_name[0] = '\0';
            fsa[pos].job_status[i].burst_counter = 0;
#endif
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
            fsa[pos].job_status[i].job_id = NO_ID;
#endif
         }
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++++ fd_exit() ++++++++++++++++++++++++++++++*/
static void
fd_exit(void)
{
   register int i, j;
   int          jobs_killed = 0,
                loop_counter;
   time_t       now;
   struct stat  stat_buf;

   if ((connection == NULL) || (qb == NULL) || (mdb == NULL))
   {
      /* We allready walked through this function. */
      return;
   }
   now = time(NULL);
   if (stop_flag == 0)
   {
      stop_flag = SAVE_STOP;
   }

   loop_counter = 0;
   do
   {
      /* Kill any job still active with a normal kill (2)! */
      for (i = 0; i < max_connections; i++)
      {
         if (connection[i].pid > 0)
         {
            if (kill(connection[i].pid, SIGINT) == -1)
            {
               if (errno != ESRCH)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to kill transfer job to %s (%d) : %s (%s %d)\n",
                            connection[i].hostname, connection[i].pid,
                            strerror(errno), __FILE__, __LINE__);
               }
            }
         }
      }
      (void)sleep(1);
      for (i = 0; i < max_connections; i++)
      {
         if (connection[i].pid > 0)
         {
            int faulty,
                qb_pos;

            qb_pos_pid(connection[i].pid, &qb_pos);
            if (qb_pos != -1)
            {
               if ((faulty = zombie_check(&connection[i], now, &qb_pos,
                                          WNOHANG)) == YES)
               {
                  qb[qb_pos].pid = PENDING;
                  if (qb[qb_pos].msg_name[0] != '\0')
                  {
                     fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                     if (fsa[connection[i].fsa_pos].error_counter == 1)
                     {
                        /* Move all jobs from this host to the error directory. */
                        for (j = 0; j < *no_msg_queued; j++)
                        {
                           if ((qb[j].in_error_dir != YES) &&
                               (qb[j].pid == PENDING) &&
                               (mdb[qb[j].pos].fsa_pos == mdb[qb[qb_pos].pos].fsa_pos))
                           {
                              to_error_dir(j);
                           }
                        }
                     }
                  }
                  else
                  {
                     fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                  }
               }
               else if (faulty == NO)
                    {
                       remove_msg(qb_pos);
                    }
               else if (faulty == NONE)
                    {
                       if (qb[qb_pos].msg_name[0] != '\0')
                       {
                          fsa[mdb[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                       }
                       else
                       {
                          fsa[fra[qb[qb_pos].pos].fsa_pos].jobs_queued++;
                       }
                       qb[qb_pos].pid = PENDING;
                    }
            }
         } /* if (connection[i].pid > 0) */
      }
      loop_counter++;
   } while ((p_afd_status->no_of_transfers > 0) && (loop_counter < 15));

   /* Kill any job still active with a kill -9! */
   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].pid > 0)
      {
         if (kill(connection[i].pid, SIGKILL) == -1)
         {
            if (errno != ESRCH)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to kill transfer job to %s (%d) : %s (%s %d)\n",
                         connection[i].hostname, connection[i].pid,
                         strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            int faulty,
                qb_pos;

            jobs_killed++;
            qb_pos_pid(connection[i].pid, &qb_pos);
            if (qb_pos != -1)
            {
               if ((faulty = zombie_check(&connection[i], now,
                                          &qb_pos, 0)) == YES)
               {
                  qb[qb_pos].pid = PENDING;
                  if (qb[qb_pos].msg_name[0] != '\0')
                  {
                     if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter == 1)
                     {
                        /* Move all jobs from this host to the error directory. */
                        for (j = 0; j < *no_msg_queued; j++)
                        {
                           if ((qb[j].in_error_dir != YES) &&
                               (qb[j].pid == PENDING) &&
                               (mdb[qb[j].pos].fsa_pos == mdb[qb[qb_pos].pos].fsa_pos))
                           {
                              to_error_dir(j);
                           }
                        }
                     }
                  }
               }
               else if (faulty == NO)
                    {
                       remove_msg(qb_pos);
                    }
               else if (faulty == NONE)
                    {
                       qb[qb_pos].pid = PENDING;
                    }
            }
         }
      }
   }
   if (jobs_killed > 0)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Have killed %d jobs the hard way! (%s %d)\n",
                jobs_killed, __FILE__, __LINE__);
   }

   /* Unmap message queue buffer. */
   if (fstat(qb_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "fstat() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char *ptr = (char *)qb - AFD_WORD_OFFSET;

      if (msync(ptr, stat_buf.st_size, MS_ASYNC) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "msync() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         qb = NULL;
      }
   }
   if (close(qb_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Unmap message cache buffer. */
   if (fstat(mdb_fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "fstat() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char *ptr = (char *)mdb - AFD_WORD_OFFSET;

      if (msync(ptr, stat_buf.st_size, MS_ASYNC) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "msync() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         mdb = NULL;
      }
   }
   if (close(mdb_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Free all memory that we allocated. */
   if (connection != NULL)
   {
      free(connection);
      connection = NULL;
   }

   /* Set number of transfers to zero */
   p_afd_status->no_of_transfers = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      fsa[i].active_transfers = 0;
      for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
      {
         fsa[i].job_status[j].no_of_files = 0;
         fsa[i].job_status[j].proc_id = -1;
         fsa[i].job_status[j].connect_status = DISCONNECT;
         fsa[i].job_status[j].file_name_in_use[0] = '\0';
      }
   }
   (void)fsa_detach(YES);

   (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s.\n", FD);

   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   p_afd_status->fd = OFF;
   (void)rec(sys_log_fd, FATAL_SIGN, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   fd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   p_afd_status->fd = OFF;
   (void)rec(sys_log_fd, FATAL_SIGN, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   fd_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
