/*
 *  fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
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
#include <dirent.h>           /* opendir(), closedir()                   */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int                        max_connections = MAX_DEFAULT_CONNECTIONS,
                           no_of_hosts = 0,
                           no_of_transfers = 0,
                           *no_msg_queued,
                           *no_msg_cached,
                           qb_fd = -1,
                           mdb_fd = -1,
                           msg_fifo_fd,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           fd_cmd_fd,
                           fd_resp_fd,
                           fd_wake_up_fd,
                           read_fin_fd,
                           retry_fd,
                           delete_jobs_fd,
                           delete_jobs_host_fd,
                           amg_flag = NO,
                           fsa_fd = -1,
                           fsa_id;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      *buf_file_size = NULL;  /* _PRIO_CHECK */
char                       *p_work_dir,
                           **p_buf_name = NULL,    /* _PRIO_CHECK */
                           *file_buffer = NULL,    /* _PRIO_CHECK */
                           *p_err_file_dir,
                           *p_file_dir,
                           *p_msg_dir,
#ifdef _BURST_MODE
                           *p_src_dir,
                           src_dir[MAX_PATH_LENGTH],
#endif
                           current_msg_list_file[MAX_PATH_LENGTH],
                           fd_cmd_fifo[MAX_PATH_LENGTH],
                           file_dir[MAX_PATH_LENGTH],
                           err_file_dir[MAX_PATH_LENGTH],
                           msg_dir[MAX_PATH_LENGTH],
                           msg_cache_file[MAX_PATH_LENGTH],
                           msg_queue_file[MAX_PATH_LENGTH];
struct filetransfer_status *fsa;
struct afd_status          *p_afd_status;
struct connection          *connection = NULL;
struct queue_buf           *qb;
struct msg_cache_buf       *mdb;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

/* Local global variables */
static time_t              now;

#if defined _BURST_MODE || defined _AGE_LIMIT
#define START_PROCESS()                                          \
        {                                                        \
           time_t current_time = time(NULL);                     \
                                                                 \
           /* Try handle any pending jobs. */                    \
           for (i = 0; i < *no_msg_queued; i++)                  \
           {                                                     \
              if (qb[i].pid == PENDING)                          \
              {                                                  \
                 if ((qb[i].pid = start_process(i, current_time, NO)) == REMOVED) \
                 {                                               \
                    /*                                           \
                     * The message can be removed because the    \
                     * files are queued in another message       \
                     * or have been removed due to age.          \
                     */                                          \
                    remove_msg(i);                               \
                    if (i < *no_msg_queued)                      \
                    {                                            \
                       i--;                                      \
                    }                                            \
                 }                                               \
              }                                                  \
           }                                                     \
        }
#else
#define START_PROCESS()                                          \
        {                                                        \
           time_t current_time = time(NULL);                     \
                                                                 \
           /* Try handle any pending jobs. */                    \
           for (i = 0; i < *no_msg_queued; i++)                  \
           {                                                     \
              if (qb[i].pid == PENDING)                          \
              {                                                  \
                 qb[i].pid = start_process(i, current_time, NO); \
              }                                                  \
           }                                                     \
        }
#endif

/* Local functions prototypes */
static void  remove_connection(int, char),
             to_error_dir(int),
             get_afd_config_value(void),
             get_new_positions(void),
             fd_exit(void),
             qb_pos_fsa(int, int *),
             qb_pos_pid(pid_t, int *),
             sig_exit(int),
             sig_segv(int),
             sig_bus(int);
static int   get_free_connection(void),
             get_free_disp_pos(int);
static char  zombie_check(int, int);
static pid_t make_process(struct connection *),
             start_process(int, time_t, int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int               force_check = NO,
                     i,
                     *job_id,
                     status,
                     max_fd,
                     pos,
                     loop_counter = 0;
   unsigned short    *unique_number;
   time_t            *creation_time,
                     abnormal_term_check_time,
                     next_dir_check_time;
   size_t            fifo_size,
                     msg_fifo_buf_size;
   char              stop_flag = 0,
                     *fifo_buffer,
                     *msg_priority,
                     work_dir[MAX_PATH_LENGTH],
                     sys_log_fifo[MAX_PATH_LENGTH];
   fd_set            rset;
   struct timeval    timeout;
   struct stat       stat_buf;
#ifdef SA_FULLDUMP
   struct sigaction  sact;
#endif

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Initialise variables */
   (void)strcpy(sys_log_fifo, work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcpy(current_msg_list_file, sys_log_fifo);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);
   (void)strcpy(msg_cache_file, sys_log_fifo);
   (void)strcat(msg_cache_file, MSG_CACHE_FILE);
   (void)strcpy(msg_queue_file, sys_log_fifo);
   (void)strcat(msg_queue_file, MSG_QUEUE_FILE);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
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
                 &fifo_buffer);

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open system log fifo */
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open and create all fifos */
   if (init_fifos_fd() == INCORRECT)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to initialize fifos. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

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
   if (p_afd_status->amg_jobs & FD_DIR_CHECK_ACTIVE)
   {
      p_afd_status->amg_jobs ^= FD_DIR_CHECK_ACTIVE;
   }

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif /* _DELETE_LOG */

   /* Attach/create memory area for message data and queue. */
   init_msg_buffer();
   for (i = 0; i < *no_msg_queued; i++)
   {
      qb[i].pid = PENDING;
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
   fifo_size++; /* Just in case. Cannot do any harm. */

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
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

   /* Get value from AFD_CONFIG file */
   get_afd_config_value();

   /* Allocate memory for connection structure. */
   if ((connection = malloc((max_connections * sizeof(struct connection)))) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)memset(connection, 0, (max_connections * sizeof(struct connection)));

   /* Initialise structure connection */
   for (i = 0; i < max_connections; i++)
   {
      connection[i].temp_toggle = OFF;
   }

   /* Tell user we are starting the FD */
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (PRE %d.%d.%d-%d)\n",
             FD, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             FD, MAJOR, MINOR, BUG_FIX);
#endif
   abnormal_term_check_time = (time(&now) / 45) * 45 + 45;
   next_dir_check_time = ((now / DIR_CHECK_TIME) * DIR_CHECK_TIME) +
                         DIR_CHECK_TIME;

   /* Now watch and start transfer jobs */
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
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
                  int  qb_pos;
                  char faulty;

                  qb_pos_pid(connection[i].pid, &qb_pos);
                  if (qb_pos != -1)
                  {
                     if ((faulty = zombie_check(qb_pos, WNOHANG)) == YES)
                     {
                        qb[qb_pos].pid = PENDING;
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
                     else if (faulty == NO)
                          {
                             remove_msg(qb_pos);
                          }
                     else if (faulty == NONE)
                          {
                             qb[qb_pos].pid = PENDING;
                          }
                  }
                  else
                  {
                     (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Hmmm. Failed to locate pid %d in queue? (%s %d)\n",
                               connection[i].pid, __FILE__, __LINE__);

                     /* It does not make sence to keep this job in */
                     /* the connect structure, so just remove it.  */
                     faulty = NEITHER;
                     remove_connection(i, faulty);
                  }

                  if ((stop_flag == 0) && (faulty != NEITHER) &&
                      (*no_msg_queued > 0))
                  {
                     START_PROCESS();
                  }
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

         abnormal_term_check_time = ((now / 45) * 45) + 45;
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
            if ((p_afd_status->amg_jobs & INST_JOB_ACTIVE) == 0)
            {
               gotcha = YES;
               break;
            }
            (void)my_usleep(100000);
#ifdef FTX
         } while (loops++ < 100);
#else
         } while (loops++ < 50);
#endif
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

               case FSA_UPDATED :
                  if (check_fsa() == YES)
                  {
                     get_new_positions();
                     init_msg_buffer();
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
                  (void)rec(sys_log_fd, INFO_SIGN, "FD shutting down ...\n");

                  exit(SUCCESS);

               default :

                  /* Most properly we are reading garbage */
                  (void)rec(sys_log_fd, WARN_SIGN, 
                            "Reading garbage (%d) on fifo %s. (%s %d)\n",
                            (int)buffer, FD_CMD_FIFO, __FILE__, __LINE__);
                  break;
            } /* switch(buffer) */
         }
      }

           /*
            * sf_xxx PROCESS TERMINATED
            * =========================
            */
      else if ((status > 0) && (FD_ISSET(read_fin_fd, &rset)))
           {
              pid_t pid;

              if (read(read_fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Reading garbage from fifo %s (%s %d)\n",
                           SF_FIN_FIFO, __FILE__, __LINE__);
              }
              else
              {
                 int  qb_pos;
                 char faulty = NO;

                 qb_pos_pid(pid, &qb_pos);
                 if (qb_pos != -1)
                 {
                    if ((faulty = zombie_check(qb_pos, 0)) == YES)
                    {
                       qb[qb_pos].pid = PENDING;
                       if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter == 1)
                       {
                          /* Move all jobs from this host to the error directory. */
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
                    else if (faulty == NO)
                         {
                            remove_msg(qb_pos);
                         }
                    else if (faulty == NONE)
                         {
                            qb[qb_pos].pid = PENDING;
                         }
                 }

                 if ((stop_flag == 0) && (faulty != NEITHER) &&
                     (*no_msg_queued > 0))
                 {
                    START_PROCESS();
                 }
              }
           } /* sf_xxx PROCESS TERMINATED */

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
                 int qb_pos;

                 if (stop_flag == 0)
                 {
                    qb_pos_fsa(fsa_pos, &qb_pos);
                    if (qb_pos != -1)
                    {
#if defined _BURST_MODE || defined _AGE_LIMIT
                       if ((qb[qb_pos].pid = start_process(qb_pos, time(NULL), YES)) == REMOVED)
                       {
                          remove_msg(qb_pos);
                       }
                    }
#else
                       qb[qb_pos].pid = start_process(qb_pos, time(NULL), YES);
                    }
#endif
                 }
              }
           } /* RETRY */
           
           /*
            * NEW MESSAGE ARRIVED
            * ===================
            */
      else if ((status > 0) && (FD_ISSET(msg_fifo_fd, &rset)))
           {
              if ((read(msg_fifo_fd, fifo_buffer, msg_fifo_buf_size)) != msg_fifo_buf_size)
              {
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Hmmm. Seems like I am reading garbage from the fifo. (%s %d)\n",
                           __FILE__, __LINE__);
              }
              else
              {
                 int tmp_job_id = *job_id;

                 /* Queue the job order */
                 if ((*no_msg_queued != 0) &&
                     ((*no_msg_queued % MSG_QUE_BUF_SIZE) == 0))
                 {
                    char   *ptr;
                    size_t new_size;

                    new_size = (((*no_msg_queued / MSG_QUE_BUF_SIZE) + 1) *
                               MSG_QUE_BUF_SIZE * sizeof(struct queue_buf)) +
                               AFD_WORD_OFFSET;
                    ptr = (char *)qb - AFD_WORD_OFFSET;
                    if ((ptr = mmap_resize(qb_fd, ptr, new_size)) == (caddr_t) -1)
                    {
                       (void)rec(sys_log_fd, FATAL_SIGN,
                                 "mmap() error : %s (%s %d)\n",
                                 strerror(errno), __FILE__, __LINE__);
                       exit(INCORRECT);
                    }
                    no_msg_queued = (int *)ptr;
                    ptr += AFD_WORD_OFFSET;
                    qb = (struct queue_buf *)ptr;
                 }

                 if ((pos = lookup_job_id(tmp_job_id)) == INCORRECT)
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN,
                              "Could not locate job %d (%s %d)\n",
                              *job_id, __FILE__, __LINE__);
                    if (*msg_priority != 0)
                    {
                       char del_dir[MAX_PATH_LENGTH];

                       (void)sprintf(del_dir, "%s%s/%c_%ld_%04d_%u", p_work_dir,
                                     AFD_FILE_DIR, *msg_priority, *creation_time,
                                     *unique_number, *job_id);
#ifdef _DELETE_LOG
                       remove_files(del_dir, -1, *job_id, OTHER_DEL);
#else
                       remove_files(del_dir, -1);
#endif
                    }
                 }
                 else
                 {
                    int    i,
                           qb_pos = *no_msg_queued;
                    double msg_number = ((double)*msg_priority - 47.0) *
                                        (((double)*creation_time * 10000.0) +
                                        (double)*unique_number);

                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (msg_number < qb[i].msg_number)
                       {
                          size_t move_size;

                          move_size = (*no_msg_queued - i) *
                                      sizeof(struct queue_buf);
                          (void)memmove(&qb[i + 1], &qb[i], move_size);
                          qb_pos = i;

                          break;
                       }
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

                    if (fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter > 0)
                    {
                       to_error_dir(qb_pos);
                    }
                    else
                    {
                       if (stop_flag == 0)
                       {
#if defined _BURST_MODE || defined _AGE_LIMIT
                          if ((qb[qb_pos].pid = start_process(qb_pos, time(NULL), NO)) == REMOVED)
                          {
                             remove_msg(qb_pos);
                          }
#else
                          qb[qb_pos].pid = start_process(qb_pos, time(NULL), NO);
#endif
                       }
                    }

                    /*
                     * Try to handle other queued files. If we do not do
                     * it here, where else?
                     */
                    if ((stop_flag == 0) && (*no_msg_queued > 1))
                    {
                       START_PROCESS();
                    }
                 }
              }
           } /* NEW MESSAGE ARRIVED */

           /*
            * DELETE ALL JOBS FROM SPECIFIC HOST
            * ==================================
            */
      else if ((status > 0) && (FD_ISSET(delete_jobs_host_fd, &rset)))
           {
              int  n;
              char *fifo_buffer;

              /* Now lets allocate memory for the fifo buffer */
              if ((fifo_buffer = calloc(fifo_size, sizeof(char))) == NULL)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Could not allocate memory for the fifo buffer : %s (%s %d)\n",
                           strerror(errno), __FILE__, __LINE__);
                 exit(INCORRECT);
              }

              if ((n = read(delete_jobs_host_fd, fifo_buffer, fifo_size)) > 0)
              {
                 int  bytes_done = 0,
                      fsa_position,
                      j;
                 char *p_host_name = fifo_buffer;

                 do
                 {
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (strcmp(mdb[qb[i].pos].host_name, p_host_name) == 0)
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
                                remove_connection(qb[i].connect_pos, NO);
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
                    if ((fsa_position = get_position(fsa, p_host_name, no_of_hosts)) != INCORRECT)
                    {
                       fsa[fsa_position].total_file_counter = 0;
                       fsa[fsa_position].total_file_size = 0;
                       fsa[fsa_position].active_transfers = 0;
                       fsa[fsa_position].error_counter = 0;
                       for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
                       {
                          fsa[fsa_position].job_status[j].no_of_files = 0;
                          fsa[fsa_position].job_status[j].proc_id = -1;
                          fsa[fsa_position].job_status[j].connect_status = DISCONNECT;
                          fsa[fsa_position].job_status[j].file_name_in_use[0] = '\0';
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

              free(fifo_buffer);
           } /* DELETE ALL JOBS FROM SPECIFIC HOST */

           /*
            * DELETE SPECIFIC JOBS
            * ====================
            */
      else if ((status > 0) && (FD_ISSET(delete_jobs_fd, &rset)))
           {
              int  n;
              char *fifo_buffer;

              /* Now lets allocate memory for the fifo buffer */
              if ((fifo_buffer = calloc(fifo_size, sizeof(char))) == NULL)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Could not allocate memory for the fifo buffer : %s (%s %d)\n",
                           strerror(errno), __FILE__, __LINE__);
                 exit(INCORRECT);
              }

              if ((n = read(delete_jobs_fd, fifo_buffer, fifo_size)) > 0)
              {
                 int  bytes_done = 0;
                 char *p_msg_name = fifo_buffer;

                 do
                 {
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (strcmp(qb[i].msg_name, p_msg_name) == 0)
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
                                remove_connection(qb[i].connect_pos, NO);
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

              free(fifo_buffer);
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
                 char fifo_buffer[10];

                 if (read(fd_wake_up_fd, fifo_buffer, 10) < 0)
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
           }

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
start_process(int qb_pos, time_t current_time, int retry)
{
   static pid_t pid;

   pid = PENDING;
#ifdef _AGE_LIMIT
   if ((mdb[qb[qb_pos].pos].age_limit > 0) &&
       ((current_time - qb[qb_pos].creation_time) > mdb[qb[qb_pos].pos].age_limit))
   {
      if (qb[qb_pos].msg_name[0] == '\0')
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Uuups! How can this be? No message name!? (%s %d)\n",
                   __FILE__, __LINE__);
      }
      else
      {
         char del_dir[MAX_PATH_LENGTH];

         if (qb[qb_pos].in_error_dir == YES)
         {
            (void)sprintf(del_dir, "%s%s%s/%s/%s",
                          p_work_dir, AFD_FILE_DIR, ERROR_DIR,
                          fsa[mdb[qb[qb_pos].pos].fsa_pos].host_alias,
                          qb[qb_pos].msg_name);
         }
         else
         {
            (void)sprintf(del_dir, "%s%s/%s",
                          p_work_dir, AFD_FILE_DIR, qb[qb_pos].msg_name);
         }
#ifdef _DELETE_LOG
         remove_files(del_dir, mdb[qb[qb_pos].pos].fsa_pos,
                      mdb[qb[qb_pos].pos].job_id, AGE_OUTPUT);
#else
         remove_files(del_dir, mdb[qb[qb_pos].pos].fsa_pos);
#endif
      }

      return(REMOVED);
   }
#endif /* _AGE_LIMIT */

   if (((fsa[mdb[qb[qb_pos].pos].fsa_pos].host_status & STOP_TRANSFER_STAT) == 0) &&
       ((fsa[mdb[qb[qb_pos].pos].fsa_pos].error_counter == 0) ||
        (retry == YES) ||
        ((fsa[mdb[qb[qb_pos].pos].fsa_pos].active_transfers == 0) &&
        ((fsa[mdb[qb[qb_pos].pos].fsa_pos].special_flag & ERROR_FILE_UNDER_PROCESS) == 0) &&
        ((current_time - (fsa[mdb[qb[qb_pos].pos].fsa_pos].last_retry_time + fsa[mdb[qb[qb_pos].pos].fsa_pos].retry_interval)) >= 0))))
   {
      if ((p_afd_status->no_of_transfers < max_connections) &&
          (fsa[mdb[qb[qb_pos].pos].fsa_pos].active_transfers < fsa[mdb[qb[qb_pos].pos].fsa_pos].allowed_transfers))
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
            if ((connection[pos].job_no = get_free_disp_pos(mdb[qb[qb_pos].pos].fsa_pos)) != INCORRECT)
            {
               /*
                * When the host can be toggled and auto toggle
                * is in effect, we must check here if we are
                * sending to the original host. If this is not
                * the case lets temporally switch back to the
                * original host to see if it is working now.
                */
               connection[pos].error_file = qb[qb_pos].in_error_dir;
               connection[pos].protocol = mdb[qb[qb_pos].pos].type;
               connection[pos].position = mdb[qb[qb_pos].pos].fsa_pos;
               (void)strcpy(connection[pos].msg_name, qb[qb_pos].msg_name);
               (void)strcpy(fsa[connection[pos].position].job_status[connection[pos].job_no].unique_name, qb[qb_pos].msg_name);
               if ((fsa[connection[pos].position].error_counter == 0) &&
                   (fsa[connection[pos].position].auto_toggle == ON) &&
                   (fsa[connection[pos].position].original_toggle_pos != NONE) &&
                   (connection[pos].temp_toggle == OFF) &&
                   (fsa[connection[pos].position].max_successful_retries > 0))
               {
                  if ((fsa[connection[pos].position].original_toggle_pos == fsa[connection[pos].position].toggle_pos) &&
                      (fsa[connection[pos].position].successful_retries > 0))
                  {
                     fsa[connection[pos].position].original_toggle_pos = NONE;
                     fsa[connection[pos].position].successful_retries = 0;
                  }
                  else if (fsa[connection[pos].position].successful_retries >= fsa[connection[pos].position].max_successful_retries)
                       {
                          connection[pos].temp_toggle = ON;
                          fsa[connection[pos].position].successful_retries = 0;
                       }
                       else
                       {
                          fsa[connection[pos].position].successful_retries++;
                       }
               }
               else
               {
                  connection[pos].temp_toggle = OFF;
               }

               /* Create process to distribute file. */
               if ((connection[pos].pid = make_process(&connection[pos])) > 0)
               {
                  (void)strcpy(connection[pos].hostname, mdb[qb[qb_pos].pos].host_name);
                  if (check_fsa() == YES)
                  {
                     get_new_positions();
                     init_msg_buffer();
                  }

                  pid = fsa[connection[pos].position].job_status[connection[pos].job_no].proc_id = connection[pos].pid;
                  fsa[connection[pos].position].active_transfers += 1;
                  if (fsa[connection[pos].position].error_counter > 0)
                  {
                     fsa[connection[pos].position].special_flag |= ERROR_FILE_UNDER_PROCESS;
                  }
                  qb[qb_pos].connect_pos = pos;
     
                  p_afd_status->no_of_transfers++;
               }
            }
         }
      }
#ifdef _BURST_MODE
      else
      {
         int limit = NO;

         if (qb[qb_pos].in_error_dir == YES)
         {
            (void)sprintf(p_src_dir, "%s/%s/%s", ERROR_DIR,
                          fsa[mdb[qb[qb_pos].pos].fsa_pos].host_alias,
                          qb[qb_pos].msg_name);
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
                      (strcmp(mdb[qb[qb_pos].pos].host_name, mdb[qb[i].pos].host_name) == 0))
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
         if (check_burst(mdb[qb[qb_pos].pos].type,
                         mdb[qb[qb_pos].pos].fsa_pos,
                         mdb[qb[qb_pos].pos].job_id,
                         src_dir,
                         file_dir,
                         limit) == YES)
         {
            pid = REMOVED;
         }
      }
#endif
   }

   return(pid);
}


/*---------------------------- make_process() ---------------------------*/
static pid_t
make_process(struct connection *con)
{
   static pid_t pid;
   char         progname[MAX_PROCNAME_LENGTH],
                str_job_no[MAX_INT_LENGTH];

   if (con->protocol == FTP)
   {
      (void)strcpy(progname, SEND_FILE_FTP);
   }
   else if (con->protocol == LOC)
        {
           (void)strcpy(progname, SEND_FILE_LOC);
        }
#ifdef _WITH_WMO_SUPPORT
   else if (con->protocol == WMO)
        {
           (void)strcpy(progname, SEND_FILE_WMO);
        }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
   else if (con->protocol == MAP)
        {
           (void)strcpy(progname, SEND_FILE_MAP);
        }
#endif /* _WITH_MAP_SUPPORT */
   else if (con->protocol == SMTP)
        {
           (void)strcpy(progname, SEND_FILE_SMTP);
        }
        else
        {
           (void)rec(sys_log_fd, DEBUG_SIGN,
                     ".....? Unknown protocol (%d) (%s %d)\n",
                     con->protocol, __FILE__, __LINE__);
           return(INCORRECT);
        }

   switch(pid = fork())
   {
      case -1 :

         /* Could not generate process */
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not create a new process : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         return(INCORRECT);

      case  0 :

         /* Child process */
         (void)sprintf(str_job_no, "%d", (int)con->job_no);

         if (con->error_file == YES)
         {
            if (con->temp_toggle == ON)
            {
               (void)execlp(progname, progname, WORK_DIR_ID, p_work_dir,
                            "-f", "-t", "-j", str_job_no,
                            "-m", con->msg_name, (char *)0);
            }
            else
            {
               (void)execlp(progname, progname, WORK_DIR_ID, p_work_dir,
                            "-f", "-j", str_job_no,
                            "-m", con->msg_name, (char *)0);
            }
         }
         else
         {
            if (con->temp_toggle == ON)
            {
               (void)execlp(progname, progname, WORK_DIR_ID, p_work_dir,
                            "-t", "-j", str_job_no,
                            "-m", con->msg_name, (char *)0);
            }
            else
            {
               (void)execlp(progname, progname, WORK_DIR_ID, p_work_dir,
                            "-j", str_job_no, "-m", con->msg_name,
                            (char *)0);
            }
         }
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to start process %s : %s (%s %d)\n",
                   progname, strerror(errno), __FILE__, __LINE__);
         pid = getpid();
         if (write(read_fin_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
         {
            (void)rec(sys_log_fd, ERROR_SIGN, "write() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
         _exit(INCORRECT);

      default :

         /* Parent process */
         return(pid);
   }
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*
 * Description : Checks if any process is finished (zombie), if this
 *               is the case it is killed with waitpid().
 */
static char
zombie_check(int qb_pos, int options)
{
   static char faulty;
   int         host_deleted = NO,
               exit_status,
               status;
   pid_t       ret;

   /* Wait for process to terminate. */
   faulty = YES;
   if ((ret = waitpid(qb[qb_pos].pid, &status, options)) == qb[qb_pos].pid)
   {
      if (WIFEXITED(status))
      {
         switch(exit_status = WEXITSTATUS(status))
         {
            case TRANSFER_SUCCESS      : /* Ordinary end of process. */
               faulty = NO;
               if (((connection[qb[qb_pos].connect_pos].temp_toggle == ON) &&
                   (fsa[connection[qb[qb_pos].connect_pos].position].original_toggle_pos != fsa[connection[qb[qb_pos].connect_pos].position].host_toggle)) ||
                   (fsa[connection[qb[qb_pos].connect_pos].position].original_toggle_pos == fsa[connection[qb[qb_pos].connect_pos].position].host_toggle))
               {
                  /*
                   * Do not forget to toggle back to the original
                   * host and deactivate original_toggle_pos!
                   */
                  connection[qb[qb_pos].connect_pos].temp_toggle = OFF;
                  fsa[connection[qb[qb_pos].connect_pos].position].successful_retries = 0;
                  if (fsa[connection[qb[qb_pos].connect_pos].position].original_toggle_pos != NONE)
                  {
                     fsa[connection[qb[qb_pos].connect_pos].position].host_toggle = fsa[connection[qb[qb_pos].connect_pos].position].original_toggle_pos;
                     fsa[connection[qb[qb_pos].connect_pos].position].original_toggle_pos = NONE;
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Switching back to host %s%c after successful transfer.\n",
                               fsa[connection[qb[qb_pos].connect_pos].position].host_alias,
                               fsa[connection[qb[qb_pos].connect_pos].position].host_toggle_str[(int)fsa[connection[qb[qb_pos].connect_pos].position].host_toggle]);
                  }
               }
               fsa[connection[qb[qb_pos].connect_pos].position].last_connection = time(NULL);
               break;

            case SYNTAX_ERROR          : /* Syntax for sf_xxx wrong. */
               if (check_fsa() == YES)
               {
                  if (get_position(fsa, connection[qb[qb_pos].connect_pos].hostname, no_of_hosts) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
               if (host_deleted == NO)
               {
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].connect_status = NOT_WORKING;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[0] = '\0';
               } 
               /*
                * Lets assume that the message is moved to the faulty directory.
                * So no further action is necessary.
                */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Syntax for calling sf_xxx wrong. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case NO_MESSAGE_FILE : /* The message file has disappeared. */
                                   /* Remove the job, or else we        */
                                   /* will always fall for this one     */
                                   /* again.                            */
               {
                  char del_dir[MAX_PATH_LENGTH];

                  if (qb[qb_pos].in_error_dir == YES)
                  {
                     (void)sprintf(del_dir, "%s%s%s/%s/%s", p_work_dir,
                                   AFD_FILE_DIR,
                                   ERROR_DIR,
                                   mdb[qb[qb_pos].pos].host_name,
                                   qb[qb_pos].msg_name);
                  }
                  else
                  {
                     (void)sprintf(del_dir, "%s%s/%s", p_work_dir,
                                   AFD_FILE_DIR, qb[qb_pos].msg_name);
                  }
#ifdef _DELETE_LOG
                  remove_files(del_dir, -1, mdb[qb[qb_pos].pos].job_id, OTHER_DEL);
#else
                  remove_files(del_dir, -1);
#endif
               }
               break;

            case JID_NUMBER_ERROR      : /* Hmm, failed to determine JID */
                                         /* number, lets assume the      */
                                         /* queue entry is corrupted.    */
                                         /* Remove it or else it will    */
                                         /* always try again.            */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Hmm..., looks like queue is corupt, will remove this (%d) entry. (%s %d)\n",
                         qb_pos, __FILE__, __LINE__);

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
                         "%-*s[%d]: Failed to send mail. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case CONNECT_ERROR         : /* Failed to connect to remote host */
            case USER_ERROR            : /* User name wrong */
            case PASSWORD_ERROR        : /* Password wrong */
            case TYPE_ERROR            : /* Setting transfer type failed */
            case REMOTE_USER_ERROR     : /* Failed to send mail address. */
            case DATA_ERROR            : /* Failed to send data command. */
            case OPEN_REMOTE_ERROR     : /* */
            case WRITE_REMOTE_ERROR    : /* */
            case WRITE_LOCK_ERROR      : /* */
            case CLOSE_REMOTE_ERROR    : /* */
            case MOVE_REMOTE_ERROR     : /* */
#ifdef _WITH_WMO_SUPPORT
            case CHECK_REPLY_ERROR     : /* Did not get a correct reply. */
#endif
            case REMOVE_LOCKFILE_ERROR : /* */
            case CHDIR_ERROR           : /* Change remote directory */
            case STAT_ERROR            : /* Failed to stat() file/directory */
            case MOVE_ERROR            : /* Move file locally */
            case RENAME_ERROR          : /* Rename file locally */
            case SELECT_ERROR          : /* selecting on sf_xxx command fifo */
#ifdef _WITH_WMO_SUPPORT
            case SIG_PIPE_ERROR        : /* When sf_wmo receives a SIGPIPE */
#endif
#ifdef _WITH_MAP_SUPPORT
            case MAP_FUNCTION_ERROR    : /* MAP function call has failed. */
#endif
               break;

            case OPEN_LOCAL_ERROR      : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to open local file. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case READ_LOCAL_ERROR      : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to read local file. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case STAT_LOCAL_ERROR      : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Could not stat() local file. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case LOCK_REGION_ERROR     : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to lock region. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case UNLOCK_REGION_ERROR   : /* */
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected. Failed to unlock region. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            case GOT_KILLED : /* Process has been killed, most properly */
                              /* by this process.                       */
               faulty = NONE;
               break;

            case NO_FILES_TO_SEND : /* There are no files to send. Most */
                                    /* properly the files have benn     */
                                    /* deleted due to age.              */
               remove_connection(qb[qb_pos].connect_pos, NEITHER);

               /*
                * This is actually a good time to check if there are
                * at all any files to be send. If NOT and the auto
                * pause queue flag is set, we might get a deadlock.
                */
               if (connection[qb[qb_pos].connect_pos].position != -1)
               {
                  if ((fsa[connection[qb[qb_pos].connect_pos].position].total_file_counter == 0) &&
                      (fsa[connection[qb[qb_pos].connect_pos].position].total_file_size == 0) &&
                      (fsa[connection[qb[qb_pos].connect_pos].position].host_status & AUTO_PAUSE_QUEUE_STAT))
                  {
                     fsa[connection[qb[qb_pos].connect_pos].position].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Starting queue for %s that was stopped by init_afd. (%s %d)\n",
                                fsa[connection[qb[qb_pos].connect_pos].position].host_alias,
                                __FILE__, __LINE__);
                  }
               }
               return(NO);

            case ALLOC_ERROR           : /* */
               if (check_fsa() == YES)
               {
                  if (get_position(fsa, connection[qb[qb_pos].connect_pos].hostname, no_of_hosts) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
               if (host_deleted == NO)
               {
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].connect_status = NOT_WORKING;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[0] = '\0';
               } 
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Failed to allocate memory. #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
               break;

            default                    : /* Unknown error. */
               if (check_fsa() == YES)
               {
                  if (get_position(fsa, connection[qb[qb_pos].connect_pos].hostname, no_of_hosts) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
               if (host_deleted == NO)
               {
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].connect_status = NOT_WORKING;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[0] = '\0';
               } 
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Disconnected due to an unknown error (%d). #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         exit_status, mdb[qb[qb_pos].pos].job_id,
                         __FILE__, __LINE__);
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
         if ((connection[qb[qb_pos].connect_pos].temp_toggle == ON) &&
             (faulty == YES))
         {
            faulty = NONE;
         }
      }
      else  if (WIFSIGNALED(status))
            {
               /* abnormal termination */
               if (check_fsa() == YES)
               {
                  if (get_position(fsa, connection[qb[qb_pos].connect_pos].hostname, no_of_hosts) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
                  get_new_positions();
                  init_msg_buffer();
               }
               if (host_deleted == NO)
               {
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].connect_status = NOT_WORKING;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].no_of_files_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_size_in_use_done = 0;
                  fsa[connection[qb[qb_pos].connect_pos].position].job_status[connection[qb[qb_pos].connect_pos].job_no].file_name_in_use[0] = '\0';
               } 

               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Abnormal termination of transfer job (%d). #%u (%s %d)\n",
                         MAX_HOSTNAME_LENGTH,
                         connection[qb[qb_pos].connect_pos].hostname,
                         connection[qb[qb_pos].connect_pos].job_no,
                         connection[qb[qb_pos].connect_pos].pid,
                         mdb[qb[qb_pos].pos].job_id, __FILE__, __LINE__);
            }

      remove_connection(qb[qb_pos].connect_pos, faulty);

      /*
       * Even if we did fail to send a file, lets set the transfer
       * time. Otherwise jobs will get deletet to early together
       * with their current files if no transfer was successful
       * and we did a reread DIR_CONFIG.
       */
      mdb[qb[qb_pos].pos].last_transfer_time = now;
   } /* if (waitpid(pid, &status, 0) == pid) */
   else
   {
      if (ret == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "waitpid() error [%d] : %s (%s %d)\n",
                   qb[qb_pos].pid, strerror(errno), __FILE__, __LINE__);
         if (errno == ECHILD)
         {
            faulty = NONE;
            remove_connection(qb[qb_pos].connect_pos, NONE);
         }
      }
      else
      {
         faulty = NEITHER;
      }
   }

   return(faulty);
}


/*------------------------- remove_connection() ------------------------*/
static void
remove_connection(int pos, char faulty)
{
   if (connection[pos].position != -1)
   {
      /* Decrease number of active transfers to this host in FSA */
      if (faulty == YES)
      {
         fsa[connection[pos].position].last_retry_time = time(NULL);
         lock_region_w(fsa_fd, (char *)&fsa[connection[pos].position].error_counter - (char *)fsa);
         fsa[connection[pos].position].error_counter += 1;
         fsa[connection[pos].position].total_errors += 1;
         unlock_region(fsa_fd, (char *)&fsa[connection[pos].position].error_counter - (char *)fsa);

         /* Check if we need to toggle hosts */
         if ((fsa[connection[pos].position].error_counter == fsa[connection[pos].position].max_errors) &&
             (fsa[connection[pos].position].original_toggle_pos == NONE))
         {
            fsa[connection[pos].position].original_toggle_pos = fsa[connection[pos].position].host_toggle;
         }
         if ((fsa[connection[pos].position].auto_toggle == ON) &&
             ((fsa[connection[pos].position].error_counter % fsa[connection[pos].position].max_errors) == 0))
         {
            (void)rec(sys_log_fd, INFO_SIGN, "Automatic host switch initiated for host %s\n",
                      fsa[connection[pos].position].host_dsp_name);
            if (fsa[connection[pos].position].host_toggle == HOST_ONE)
            {
               fsa[connection[pos].position].host_toggle = HOST_TWO;
            }
            else
            {
               fsa[connection[pos].position].host_toggle = HOST_ONE;
            }
         }
      }
      else
      {
         if ((faulty != NEITHER) &&
             (fsa[connection[pos].position].error_counter > 0) &&
             (connection[pos].temp_toggle == OFF))
         {
            int j;

            lock_region_w(fsa_fd, (char *)&fsa[connection[pos].position].error_counter - (char *)fsa);
            fsa[connection[pos].position].error_counter = 0;

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa[connection[pos].position].allowed_transfers; j++)
            {
               if ((j != pos) &&
                   (fsa[connection[pos].position].job_status[j].connect_status == NOT_WORKING))
               {
                  fsa[connection[pos].position].job_status[j].connect_status = DISCONNECT;
               }
            }
            unlock_region(fsa_fd, (char *)&fsa[connection[pos].position].error_counter - (char *)fsa);
         }
      }

      /*
       * Reset some values of FSA structure. But before we do so it's
       * essential that we do NOT write to an old FSA! So lets check if we
       * are still in the correct FSA. Otherwise we subtract the number of
       * active transfers without ever resetting the pid! This can lead to
       * some very fatal behaviour of the AFD.
       */
      if (check_fsa() == YES)
      {
         get_new_positions();
         init_msg_buffer();
      }

      if (fsa[connection[pos].position].active_transfers > fsa[connection[pos].position].allowed_transfers)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Active transfers > allowed transfers %d!? [%d] (%s %d)\n",
                   fsa[connection[pos].position].allowed_transfers,
                   fsa[connection[pos].position].active_transfers,
                   __FILE__, __LINE__);
         fsa[connection[pos].position].active_transfers = fsa[connection[pos].position].allowed_transfers;
      }
      fsa[connection[pos].position].active_transfers -= 1;
      if (fsa[connection[pos].position].active_transfers < 0)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Active transfers < 0!? [%d] (%s %d)\n",
                   fsa[connection[pos].position].active_transfers,
                   __FILE__, __LINE__);
         fsa[connection[pos].position].active_transfers = 0;
      }
      if (fsa[connection[pos].position].special_flag & ERROR_FILE_UNDER_PROCESS)
      {
         /* Error job is no longer under process. */
         fsa[connection[pos].position].special_flag ^= ERROR_FILE_UNDER_PROCESS;
      }
      fsa[connection[pos].position].job_status[(int)connection[pos].job_no].proc_id = -1;
#ifdef _BURST_MODE
      fsa[connection[pos].position].job_status[(int)connection[pos].job_no].unique_name[0] = '\0';
      fsa[connection[pos].position].job_status[(int)connection[pos].job_no].burst_counter = 0;
#endif
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
      fsa[connection[pos].position].job_status[(int)connection[pos].job_no].job_id = NO_ID;
#endif
   }

   /*
    * Reset all values of connection structure.
    */
   connection[pos].hostname[0] = '\0';
   connection[pos].job_no = -1;
   connection[pos].position = -1;
   connection[pos].msg_name[0] = '\0';
   connection[pos].pid = 0;

   /* Decrease the number of active transfers */
   if (p_afd_status->no_of_transfers > 0)
   {
      p_afd_status->no_of_transfers--;
   }
   else
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Huh?! Whats this trying to reduce number of transfers although its zero??? (%s %d)\n",
                __FILE__, __LINE__);
   }

   return;
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
          (stat_buf.st_nlink >= LINK_MAX))
#endif
      {
         fsa[mdb[qb[pos].pos].fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Stopped queue for host %s, since the number jobs are reaching a dangerous level. (%s %d)\n",
                   fsa[mdb[qb[pos].pos].fsa_pos].host_alias, __FILE__, __LINE__);
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
                   (LINK_MAX - 2), __FILE__, __LINE__);
#endif
         if (fsa[mdb[qb[pos].pos].fsa_pos].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
         {
            fsa[mdb[qb[pos].pos].fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Stopped queue for host %s, since the number jobs are reaching a dangerous level. (%s %d)\n",
                      fsa[mdb[qb[pos].pos].fsa_pos].host_alias, __FILE__, __LINE__);
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


/*++++++++++++++++++++++++++++ qb_pos_fsa() +++++++++++++++++++++++++++++*/
static void
qb_pos_fsa(int fsa_pos, int *qb_pos)
{
   int    i,
          j = 0,
          pos;
   double msg_number = 0.0;

   *qb_pos = -1;
   do
   {
      pos = -1;
      for (i = j; i < *no_msg_cached; i++)
      {
         if (fsa_pos == mdb[i].fsa_pos)
         {
            pos = i;
            break;
         }
      }
      if ((pos == -1) && (j == 0))
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "No message cached for FSA position %d. (%s %d)\n",
                   fsa_pos, __FILE__, __LINE__);
         return;
      }
      j = i + 1;

      for (i = 0; i < *no_msg_queued; i++)
      {
         if ((qb[i].pos == pos) && (qb[i].pid == PENDING))
         {
            if ((msg_number == 0.0) || (msg_number > qb[i].msg_number))
            {
               *qb_pos = i;
               msg_number = qb[i].msg_number;
            }
            break;
         }
      }
   } while (j < *no_msg_cached);

   if (*qb_pos == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "No message for %s in queue that is PENDING. (%s %d)\n",
                fsa[fsa_pos].host_dsp_name, __FILE__, __LINE__);
   }

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
   if ((access(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_CONNECTIONS_DEF, value, MAX_INT_LENGTH) == SUCCESS)
      {
         max_connections = atoi(value);
         if ((max_connections < 1) ||
             (max_connections > MAX_CONFIGURABLE_CONNECTIONS))
         {
            max_connections = MAX_DEFAULT_CONNECTIONS;
         }
      }
      free(buffer);
   }

   return;
}


/*++++++++++++++++++++++++++ get_free_connection() ++++++++++++++++++++++*/
static int
get_free_connection(void)
{
   static int i;

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
   static int i;

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
      if (fsa[pos].job_status[i].proc_id > 0)
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


/*+++++++++++++++++++++++++++ get_new_positions() +++++++++++++++++++++++*/
static void
get_new_positions(void)
{
   register int i;

   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].pid > 0)
      {
         if ((connection[i].position = get_position(fsa, connection[i].hostname, no_of_hosts)) < 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "Hmm. Failed to locate host %s for connection %d [pid = %d] has been removed. Writing data to end of FSA 8-( (%s %d)\n",
                      connection[i].hostname, i, connection[i].pid,
                      __FILE__, __LINE__);
            connection[i].position = no_of_hosts;
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ fd_exit() ++++++++++++++++++++++++++++++*/
static void
fd_exit(void)
{
   register int i, j, jobs_killed = 0;
   struct stat  stat_buf;

   /* Kill any job still active with a normal kill (2)! */
   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid > 0)
      {
         if (kill(qb[i].pid, SIGINT) == -1)
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
   for (i = 0; i < *no_msg_queued; i++)
   {
      if (qb[i].pid > 0)
      {
         char faulty;

         if ((faulty = zombie_check(i, WNOHANG)) == YES)
         {
            qb[i].pid = PENDING;
            if (fsa[mdb[qb[i].pos].fsa_pos].error_counter == 1)
            {
               int j;

               /* Move all jobs from this host to the error directory. */
               for (j = 0; j < *no_msg_queued; j++)
               {
                  if ((qb[j].in_error_dir != YES) &&
                      (qb[j].pid == PENDING) &&
                      (mdb[qb[j].pos].fsa_pos == mdb[qb[i].pos].fsa_pos))
                  {
                     to_error_dir(j);
                  }
               }
            }
         }
         else if (faulty == NO)
              {
                 remove_msg(i);
                 if (i < *no_msg_queued)
                 {
                    i--;
                 }
              }
         else if (faulty == NONE)
              {
                 qb[i].pid = PENDING;
              }
      } /* if (qb[i].pid > 0) */
   }

   /* Kill any job still active with a kill -9! */
   for (i = 0; i < p_afd_status->no_of_transfers; i++)
   {
      if (connection[i].pid > 0)
      {
         int  qb_pos;
         char faulty;

         if (kill(connection[i].pid, SIGKILL) == -1)
         {
            jobs_killed++;
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
            qb_pos_pid(connection[i].pid, &qb_pos);
            if (qb_pos != -1)
            {
               if ((faulty = zombie_check(qb_pos, 0)) == YES)
               {
                  qb[qb_pos].pid = PENDING;
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
               else if (faulty == NO)
                    {
                       remove_msg(qb_pos);
                    }
               else if (faulty == NONE)
                    {
                       qb[qb_pos].pid = PENDING;
                    }
            }
            i--;
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
   }
   if (close(mdb_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   if (connection != NULL)
   {
      free(connection);
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

   (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s.\n", FD);

   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
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
