/*
 *  init_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2000 Deutscher Wetterdienst (DWD),
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
 **   init_afd - starts all processes for the AFD and keeps them
 **              alive
 **
 ** SYNOPSIS
 **   init_afd [--version] [-w <work dir>] [-nd]
 **        --version      Prints current version and copyright
 **        -w <work dir>  Working directory of the AFD
 **        -nd            Do not start as daemon process
 **
 ** DESCRIPTION
 **   This program will start all programs used by the AFD in the
 **   correct order.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.1995 H.Kiehl Created
 **   11.02.1997 H.Kiehl Start and watch I/O-process.
 **   22.03.1997 H.Kiehl Added support for systems without mmap().
 **   10.06.1997 H.Kiehl When job counter reaches critical value stop
 **                      queue for jobs with lots of files waiting to
 **                      be send.
 **   03.09.1997 H.Kiehl Added process AFDD.
 **   23.02.1998 H.Kiehl Clear pool directory before starting AMG.
 **   16.10.1998 H.Kiehl Added parameter -nd so we do NOT start as
 **                      daemon.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fprintf()                           */
#include <string.h>              /* strcpy(), strcat(), strerror()      */
#include <stdlib.h>              /* getenv(), atexit(), exit(), abort() */
#include <time.h>                /* time(), ctime()                     */
#include <sys/types.h>
#ifndef _NO_MMAP
#include <sys/mman.h>            /* mmap(), munmap()                    */
#endif
#include <sys/stat.h>
#include <sys/time.h>            /* struct timeval                      */
#include <sys/wait.h>            /* waitpid()                           */
#include <signal.h>              /* signal()                            */
#include <unistd.h>              /* select(), unlink(), lseek(),        */
                                 /* sleep(), STDERR_FILENO              */
#include <fcntl.h>               /* O_RDWR, O_CREAT, O_WRONLY, etc      */
#include <limits.h>              /* LINK_MAX                            */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* Global definitions */
int                        sys_log_fd = STDERR_FILENO,
                           afd_cmd_fd,
                           afd_resp_fd,
                           amg_cmd_fd,
                           fd_cmd_fd,
                           afd_active_fd,
                           probe_only_fd,
                           status_shm_id,
                           probe_only = 1,
                           no_of_hosts = 0,
                           amg_flag = NO,
                           fsa_fd = -1,
                           fsa_id;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir,
                           afd_status_file[MAX_PATH_LENGTH],
                           afd_cmd_fifo[MAX_PATH_LENGTH],
                           amg_cmd_fifo[MAX_PATH_LENGTH],
                           fd_cmd_fifo[MAX_PATH_LENGTH],
                           probe_only_fifo[MAX_PATH_LENGTH],
                           afd_active_file[MAX_PATH_LENGTH];
struct afd_status          *p_afd_status;
struct filetransfer_status *fsa;
struct proc_table          proc_table[NO_OF_PROCESS];

/* local functions */
static pid_t               make_process(char *, char *);
static void                afd_exit(void),
                           check_dirs(char *),
                           log_pid(pid_t, int),
                           sig_exit(int),
                           sig_bus(int),
                           sig_segv(int),
                           zombie_check(void);

/* Local constant definitions */
#define DANGER_NO_OF_FILES (2 * MAX_COPIED_FILES)


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            current_month,
                  status,
                  i,
                  fd,
                  auto_amg_stop = NO,
                  old_afd_stat;
   off_t          offset;
   time_t         month_check_time,
                  now;
   fd_set         rset;
   signed char    stop_typ = STARTUP_ID,
                  char_value;
   char           *ptr = NULL,
#ifdef _FIFO_DEBUG
                  cmd[2],
#endif
                  work_dir[MAX_PATH_LENGTH],
                  afd_file_dir[MAX_PATH_LENGTH],
                  sys_log_fifo[MAX_PATH_LENGTH];
   struct timeval timeout;
   struct tm      *bd_time;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }

   /* Check if this directory does exists, if not create it. */
   if (check_dir(work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   (void)strcpy(afd_status_file, work_dir);
   (void)strcat(afd_status_file, FIFO_DIR);
   (void)strcpy(afd_active_file, afd_status_file);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);
   (void)strcpy(sys_log_fifo, afd_status_file);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcpy(afd_cmd_fifo, afd_status_file);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);
   (void)strcpy(probe_only_fifo, afd_status_file);
   (void)strcat(probe_only_fifo, PROBE_ONLY_FIFO);
   (void)strcat(afd_status_file, STATUS_SHMID_FILE);

   (void)strcpy(afd_file_dir, work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);

   /* Make sure that no other AFD is running in this directory */
   if (check_afd(10L) == 1)
   {
      exit(0);
   }
   probe_only = 0;
   if ((afd_active_fd = coe_open(afd_active_file, (O_RDWR | O_CREAT | O_TRUNC),
                                 (S_IRUSR | S_IWUSR))) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to create %s : %s (%s %d)\n",
                    afd_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   offset = ((NO_OF_PROCESS + 1) * sizeof(pid_t)) + 1;
   if (lseek(afd_active_fd, offset, SEEK_SET) == -1)
   {
      (void)fprintf(stderr, "ERROR   : lseek() error in %s : %s (%s %d)\n",
                    afd_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   char_value = EOF;
   if (write(afd_active_fd, &char_value, 1) != 1)
   {
      (void)fprintf(stderr, "ERROR   : write() error in %s : %s (%s %d)\n",
                    afd_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open and create all fifos */
   init_fifos_afd();

   if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'n') &&
       (argv[1][2] == 'd'))
   {
      /* DO NOT START AS DAEMON!!! */;
   }
   else
   {
      daemon_init(AFD);
   }

   /* Now check if all directories needed are created */
   check_dirs(work_dir);

   if ((stat(afd_status_file, &stat_buf) == -1) ||
       (stat_buf.st_size != sizeof(struct afd_status)))
   {
      if (errno != ENOENT)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   afd_status_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if ((fd = coe_open(afd_status_file, O_RDWR | O_CREAT | O_TRUNC,
                         S_IRUSR | S_IWUSR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create %s : %s (%s %d)\n",
                   afd_status_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (lseek(fd, sizeof(struct afd_status) - 1, SEEK_SET) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not seek() on %s : %s (%s %d)\n",
                   afd_status_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (write(fd, "", 1) != 1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      old_afd_stat = NO;
   }
   else
   {
      if ((fd = coe_open(afd_status_file, O_RDWR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create %s : %s (%s %d)\n",
                   afd_status_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      old_afd_stat = YES;
   }
#ifdef _NO_MMAP
   /* Start mapper process that emulates mmap() */
   proc_table[MAPPER_NO].pid = make_process(MAPPER, work_dir);
   log_pid(proc_table[MAPPER_NO].pid, MAPPER_NO + 1);

   if ((ptr = mmap_emu(0, sizeof(struct afd_status), (PROT_READ | PROT_WRITE),
                       MAP_SHARED, afd_status_file, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap(0, sizeof(struct afd_status), (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t) -1)
#endif
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "mmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifndef MMAP_KILLER
   /*
    * In Irix 5.x there is a process fsr that always tries to
    * optimise the file system. This process however does not care
    * about memory mapped files! So, lets hope that when we leave
    * the memory mapped file open, it will leave this file
    * untouched.
    */
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
#endif /* IRIX5 */
   p_afd_status = (struct afd_status *)ptr;
   if (old_afd_stat == NO)
   {
      (void)memset(p_afd_status, 0, sizeof(struct afd_status));
      (void)memset(p_afd_status->receive_log_history, NO_INFORMATION,
                   MAX_LOG_HISTORY);
      (void)memset(p_afd_status->sys_log_history, NO_INFORMATION,
                   MAX_LOG_HISTORY);
      (void)memset(p_afd_status->trans_log_history, NO_INFORMATION,
                   MAX_LOG_HISTORY);
   }
   else
   {
      p_afd_status->amg = 0;
      p_afd_status->amg_jobs = 0;
      p_afd_status->fd = 0;
      p_afd_status->sys_log = 0;
      p_afd_status->receive_log = 0;
      p_afd_status->trans_log = 0;
      p_afd_status->trans_db_log = 0;
      p_afd_status->archive_watch = 0;
      p_afd_status->afd_stat = 0;
      p_afd_status->afdd = 0;
#ifdef _NO_MMAP
      p_afd_status->mapper = 0;
#endif
#ifdef _INPUT_LOG
      p_afd_status->input_log = 0;
#endif
#ifdef _OUTPUT_LOG
      p_afd_status->output_log = 0;
#endif
#ifdef _DELETE_LOG
      p_afd_status->delete_log = 0;
#endif
      p_afd_status->no_of_transfers = 0;
      p_afd_status->start_time = 0L;
   }
   for (i = 0; i < NO_OF_PROCESS; i++)
   {
      proc_table[i].pid = 0;
      switch(i)
      {
         case AMG_NO    : proc_table[i].status = &p_afd_status->amg;
                          (void)strcpy(proc_table[i].proc_name, AMG);
                          break;

         case FD_NO     : proc_table[i].status = &p_afd_status->fd;
                          (void)strcpy(proc_table[i].proc_name, FD);
                          break;

         case SLOG_NO   : proc_table[i].status = &p_afd_status->sys_log;
                          (void)strcpy(proc_table[i].proc_name, SLOG);
                          break;

         case RLOG_NO   : proc_table[i].status = &p_afd_status->receive_log;
                          (void)strcpy(proc_table[i].proc_name, RLOG);
                          break;

         case TLOG_NO   : proc_table[i].status = &p_afd_status->trans_log;
                          (void)strcpy(proc_table[i].proc_name, TLOG);
                          break;

         case TDBLOG_NO : proc_table[i].status = &p_afd_status->trans_db_log;
                          (void)strcpy(proc_table[i].proc_name, TDBLOG);
                          break;

         case AW_NO     : proc_table[i].status = &p_afd_status->archive_watch;
                          (void)strcpy(proc_table[i].proc_name, ARCHIVE_WATCH);
                          break;

         case STAT_NO   : proc_table[i].status = &p_afd_status->afd_stat;
                          (void)strcpy(proc_table[i].proc_name, AFD_STAT);
                          break;

         case DC_NO     : /* dir_check */
                          log_pid(0, i + 1);
                          break;

         case AFDD_NO   : proc_table[i].status = &p_afd_status->afdd;
                          (void)strcpy(proc_table[i].proc_name, AFDD);
                          break;

#ifdef _NO_MMAP
         case MAPPER_NO : proc_table[i].status = &p_afd_status->mapper;
                          (void)strcpy(proc_table[i].proc_name, MAPPER);
                          *proc_table[MAPPER_NO].status = ON;
                          break;
#endif
#ifdef _INPUT_LOG
         case IL_NO     : proc_table[i].status = &p_afd_status->input_log;
                          (void)strcpy(proc_table[i].proc_name, INPUT_LOG_PROCESS);
                          break;
#endif
#ifdef _OUTPUT_LOG
         case OL_NO     : proc_table[i].status = &p_afd_status->output_log;
                          (void)strcpy(proc_table[i].proc_name, OUTPUT_LOG_PROCESS);
                          break;
#endif
#ifdef _DELETE_LOG
         case DL_NO     : proc_table[i].status = &p_afd_status->delete_log;
                          (void)strcpy(proc_table[i].proc_name, DELETE_LOG_PROCESS);
                          break;
#endif
         default        : /* Impossible */
                          (void)rec(sys_log_fd, FATAL_SIGN,
                                    "Don't know what's going on here. Giving up! (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
      }
   }

   /* Do some cleanups when we exit */
   if (atexit(afd_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit function : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Activate some signal handlers, so we know what happened. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Determine current month.
    */
   time(&now);

   bd_time = gmtime(&now);
   current_month = bd_time->tm_mon;
   month_check_time = ((now / 86400) * 86400) + 86400;

   /* Initialise communication flag FD <-> AMG */
   p_afd_status->amg_jobs = 0;

   /* Start all log process */
   proc_table[SLOG_NO].pid = make_process(SLOG, work_dir);
   log_pid(proc_table[SLOG_NO].pid, SLOG_NO + 1);
   *proc_table[SLOG_NO].status = ON;
   proc_table[RLOG_NO].pid = make_process(RLOG, work_dir);
   log_pid(proc_table[RLOG_NO].pid, RLOG_NO + 1);
   proc_table[TLOG_NO].pid = make_process(TLOG, work_dir);
   log_pid(proc_table[TLOG_NO].pid, TLOG_NO + 1);
   *proc_table[TLOG_NO].status = ON;
   proc_table[TDBLOG_NO].pid = make_process(TDBLOG, work_dir);
   log_pid(proc_table[TDBLOG_NO].pid, TDBLOG_NO + 1);
   *proc_table[TDBLOG_NO].status = ON;

   /* Start process cleaning archive directory */
   proc_table[AW_NO].pid = make_process(ARCHIVE_WATCH, work_dir);
   log_pid(proc_table[AW_NO].pid, AW_NO + 1);
   *proc_table[AW_NO].status = ON;

   /* Start process doing the I/O logging */
#ifdef _INPUT_LOG
   proc_table[IL_NO].pid = make_process(INPUT_LOG_PROCESS, work_dir);
   log_pid(proc_table[IL_NO].pid, IL_NO + 1);
   *proc_table[IL_NO].status = ON;
#endif
#ifdef _OUTPUT_LOG
   proc_table[OL_NO].pid = make_process(OUTPUT_LOG_PROCESS, work_dir);
   log_pid(proc_table[OL_NO].pid, OL_NO + 1);
   *proc_table[OL_NO].status = ON;
#endif
#ifdef _DELETE_LOG
   proc_table[DL_NO].pid = make_process(DELETE_LOG_PROCESS, work_dir);
   log_pid(proc_table[DL_NO].pid, DL_NO + 1);
   *proc_table[DL_NO].status = ON;
#endif

   /* Tell user at what time the AFD was started */
   log_pid(getpid(), 0);
   (void)rec(sys_log_fd, INFO_SIGN,
             "=================> STARTUP <=================\n");
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (PRE %d.%d.%d-%d)\n",
             AFD, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             AFD, MAJOR, MINOR, BUG_FIX);
#endif

   /* Start the process AMG */
   proc_table[AMG_NO].pid = make_process(AMG, work_dir);
   log_pid(proc_table[AMG_NO].pid, AMG_NO + 1);
   *proc_table[AMG_NO].status = ON;

   /* Start TCP info daemon of AFD */
   proc_table[AFDD_NO].pid = make_process(AFDD, work_dir);
   log_pid(proc_table[AFDD_NO].pid, AFDD_NO + 1);
   *proc_table[AFDD_NO].status = ON;

   /*
    * Before starting the FD lets initialise all critical values
    * for this process.
    */
   p_afd_status->no_of_transfers = 0;
   if (fsa_attach() < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to attach to FSA. (%s %d)\n",
                __FILE__, __LINE__);
   }
   else
   {
      for (i = 0; i < no_of_hosts; i++)
      {
         int j;

         fsa[i].active_transfers = 0;
         for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
         {
            fsa[i].job_status[j].no_of_files = 0;
            fsa[i].job_status[j].proc_id = -1;
            fsa[i].job_status[j].job_id = NO_ID;
            fsa[i].job_status[j].connect_status = DISCONNECT;
            fsa[i].job_status[j].file_name_in_use[0] = '\0';
         }
      }

      (void)fsa_detach();
   }

   /*
    * Watch if any of the two process (AMG, FD) dies.
    * While doing this wait and see if any commands or replies
    * are received via fifos.
    */
   for (;;)
   {
      /*
       * Lets write the month into the SYSTEM_LOG once only. So check
       * every day if a new month has been reached.
       */
      if (time(&now) > month_check_time)
      {
         bd_time = gmtime(&now);
         if (bd_time->tm_mon != current_month)
         {
            char month[15];

            (void)strftime(month, 15, "%B", bd_time);
            (void)rec(sys_log_fd, DUMMY_SIGN,
                      "================= %s <=================\n", month);
            current_month = bd_time->tm_mon;
         }
         month_check_time = ((now / 86400) * 86400) + 86400;
      }

      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(afd_cmd_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = AFD_RESCAN_TIME;

      /* Wait for message x seconds and then continue. */
      status = select(afd_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      /* Did we get a timeout? */
      if (status == 0)
      {
         /* Check if all jobs are still running */
         zombie_check();

         /*
          * See how many directories there are in file directory,
          * so we can show user the number of jobs in queue.
          */
         if (stat(afd_file_dir, &stat_buf) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      afd_file_dir, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (stat_buf.st_nlink > 2)
         {
            p_afd_status->jobs_in_queue = stat_buf.st_nlink - DIRS_IN_FILE_DIR;
            /* - 6 --> ".", "..", "error", "pool", "time", "incoming" */
         }
         else
         {
            p_afd_status->jobs_in_queue = 0;
         }

         /*
          * If there are more then LINK_MAX directories stop the AMG.
          * Or else files will be lost!
          */
#ifdef _LINK_MAX_TEST
         if ((stat_buf.st_nlink > (LINKY_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)) && (proc_table[AMG_NO].pid != 0))
#else
#ifdef REDUCED_LINK_MAX
         if ((stat_buf.st_nlink > (REDUCED_LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)) && (proc_table[AMG_NO].pid != 0))
#else
         if ((stat_buf.st_nlink > (LINK_MAX - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)) && (proc_table[AMG_NO].pid != 0))
#endif
#endif
         {
            /* Tell user that AMG is stopped. */
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Have stopped AMG, due to to many jobs in system! (%s %d)\n",
                      __FILE__, __LINE__);
            (void)rec(sys_log_fd, INFO_SIGN,
                      "Will start AMG again when job counter is less then %d\n",
#ifdef _LINK_MAX_TEST
                      (LINKY_MAX - START_AMG_THRESHOLD + 1));
#else
#ifdef REDUCED_LINK_MAX
                      (REDUCED_LINK_MAX - START_AMG_THRESHOLD + 1));
#else
                      (LINK_MAX - START_AMG_THRESHOLD + 1));
#endif
#endif
            auto_amg_stop = YES;

#ifdef _FIFO_DEBUG
            cmd[0] = STOP; cmd[1] = '\0';
            show_fifo_data('W', "amg_cmd", cmd, 1, __FILE__, __LINE__);
#endif
            if (send_cmd(STOP, amg_cmd_fd) < 0)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Was not able to stop %s. (%s %d)\n",
                         AMG, __FILE__, __LINE__);
            }
         }
         else
         {
            if ((auto_amg_stop == YES) &&
#ifdef _LINK_MAX_TEST
                (stat_buf.st_nlink < (LINKY_MAX - START_AMG_THRESHOLD)))
#else
#ifdef REDUCED_LINK_MAX
                (stat_buf.st_nlink < (REDUCED_LINK_MAX - START_AMG_THRESHOLD)))
#else
                (stat_buf.st_nlink < (LINK_MAX - START_AMG_THRESHOLD)))
#endif
#endif
            {
               if (proc_table[AMG_NO].pid < 1)
               {
                  /* Restart the AMG */
                  proc_table[AMG_NO].pid = make_process(AMG, work_dir);
                  log_pid(proc_table[AMG_NO].pid, AMG_NO + 1);
                  *proc_table[AMG_NO].status = ON;
                  (void)rec(sys_log_fd, ERROR_SIGN, "Have started AMG, that was stopped due to too many jobs in the system! (%s %d)\n",
                            __FILE__, __LINE__);
               }
               auto_amg_stop = NO;
            }
         }

         /*
          * If the number of errors are larger then max_errors
          * stop the queue for this host.
          */
         if (fsa != NULL)
         {
            (void)check_fsa();

            for (i = 0; i < no_of_hosts; i++)
            {
               if (((fsa[i].error_counter >= (2 * fsa[i].max_errors)) &&
                   ((fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT) == 0)) ||
                   ((fsa[i].error_counter < (2 * fsa[i].max_errors)) &&
                   (fsa[i].host_status & AUTO_PAUSE_QUEUE_STAT)))
               {
                  fsa[i].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                  if (fsa[i].error_counter >= (2 * fsa[i].max_errors))
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Stopped queue for host %s, since there are to many errors. (%s %d)\n",
                               fsa[i].host_alias, __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)rec(sys_log_fd, INFO_SIGN,
                               "Started queue for host %s that has been stopped due to too many errors. (%s %d)\n",
                               fsa[i].host_alias, __FILE__, __LINE__);
                  }
               }
               if ((stat_buf.st_nlink >= DANGER_NO_OF_JOBS) &&
                   ((fsa[i].host_status & DANGER_PAUSE_QUEUE_STAT) == 0) &&
                   (fsa[i].total_file_counter > DANGER_NO_OF_FILES))
               {
                  fsa[i].host_status ^= DANGER_PAUSE_QUEUE_STAT;
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Stopped queue for host %s, since there are to many jobs in the queue. (%s %d)\n",
                            fsa[i].host_alias, __FILE__, __LINE__);
               }
               else if ((fsa[i].host_status & DANGER_PAUSE_QUEUE_STAT) &&
                        ((fsa[i].total_file_counter < (DANGER_NO_OF_FILES / 2)) ||
                        (stat_buf.st_nlink < (DANGER_NO_OF_JOBS - 10))))
                    {
                       fsa[i].host_status ^= DANGER_PAUSE_QUEUE_STAT;
                       (void)rec(sys_log_fd, INFO_SIGN,
                                 "Started queue for host %s, that was stopped due to too many jobs in the queue. (%s %d)\n",
                                 fsa[i].host_alias, __FILE__, __LINE__);
                    }
               if ((fsa[i].total_file_counter == 0) &&
                   (fsa[i].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT))
               {
                  fsa[i].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
                  (void)rec(sys_log_fd, INFO_SIGN,
                            "Started queue for host %s, that was locked automatically. (%s %d)\n",
                            fsa[i].host_alias, __FILE__, __LINE__);
               }
            }
         } /* if (fsa != NULL) */
      }
           /* Did we get a message from the process */
           /* that controls the AFD?                */
      else if (FD_ISSET(afd_cmd_fd, &rset))
           {
              int  n;
              char buffer[DEFAULT_BUFFER_SIZE];

              if ((n = read(afd_cmd_fd, buffer, DEFAULT_BUFFER_SIZE)) > 0)
              {
                 int count = 0;

#ifdef _FIFO_DEBUG
                 show_fifo_data('R', "afd_cmd", buffer, n, __FILE__, __LINE__);
#endif

                 /* Now evaluate all data read from fifo, byte after byte */
                 while (count < n)
                 {
                    switch(buffer[count])
                    {
                       case SHUTDOWN  : /* Shutdown AFD */

                          /* Tell 'afd' that we received shutdown message */
                          if (send_cmd(ACKN, afd_resp_fd) < 0)
                          {
                             (void)rec(sys_log_fd, ERROR_SIGN,
                                       "Failed to send ACKN : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                          }

                          if (proc_table[AMG_NO].pid > 0)
                          {
                             int j;

                             /*
                              * Killing the AMG by sending it an SIGINT, is
                              * not a good idea. Lets wait for it to shutdown
                              * properly so it has time to finish writing
                              * its messages.
                              */
#ifdef _FIFO_DEBUG
                             cmd[0] = STOP; cmd[1] = '\0';
                             show_fifo_data('W', "amg_cmd", cmd, 1, __FILE__, __LINE__);
#endif
                             if (send_cmd(STOP, amg_cmd_fd) < 0)
                             {
                                (void)rec(sys_log_fd, WARN_SIGN,
                                          "Was not able to stop %s. (%s %d)\n",
                                          AMG, __FILE__, __LINE__);
                             }
                             for (j = 0; j < MAX_SHUTDOWN_TIME;  j++)
                             {
                                if (waitpid(proc_table[AMG_NO].pid, NULL, WNOHANG) == proc_table[AMG_NO].pid)
                                {
                                   proc_table[AMG_NO].pid = 0;
                                   break;
                                }
                                else
                                {
                                   (void)sleep(1);
                                }
                             }
                          }

                          /* Shutdown of other process is handled by */
                          /* the exit handler.                       */
                          exit(SUCCESS);

                       case STOP      : /* Stop all process of the AFD */

                          stop_typ = ALL_ID;

                          /* Send all process the stop signal (via fifo) */
#ifdef _FIFO_DEBUG
                          cmd[0] = STOP; cmd[1] = '\0';
                          show_fifo_data('W', "amg_cmd", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(STOP, amg_cmd_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to stop %s. (%s %d)\n",
                                       AMG, __FILE__, __LINE__);
                          }
#ifdef _FIFO_DEBUG
                          cmd[0] = STOP; cmd[1] = '\0';
                          show_fifo_data('W', "fd_cmd", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(STOP, fd_cmd_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to stop %s. (%s %d)\n",
                                       FD, __FILE__, __LINE__);
                          }

                          break;

                       case STOP_AMG  : /* Only stop the AMG */

                          stop_typ = AMG_ID;
#ifdef _FIFO_DEBUG
                          cmd[0] = STOP; cmd[1] = '\0';
                          show_fifo_data('W', "amg_cmd", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(STOP, amg_cmd_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to stop %s. (%s %d)\n",
                                       AMG, __FILE__, __LINE__);
                          }
                          break;

                       case STOP_FD   : /* Only stop the FD */

                          stop_typ = FD_ID;
#ifdef _FIFO_DEBUG
                          cmd[0] = QUICK_STOP; cmd[1] = '\0';
                          show_fifo_data('W', "fd_cmd", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(QUICK_STOP, fd_cmd_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to stop %s. (%s %d)\n",
                                       FD, __FILE__, __LINE__);
                          }
                          break;

                       case START_AMG : /* Start the AMG */

                          /* First check if it is running */
                          if (proc_table[AMG_NO].pid > 0)
                          {
                             (void)rec(sys_log_fd, INFO_SIGN,
                                       "%s is already running.\n",
                                       AMG, __FILE__, __LINE__);
                          }
                          else
                          {
                             proc_table[AMG_NO].pid = make_process(AMG, work_dir);
                             log_pid(proc_table[AMG_NO].pid, AMG_NO + 1);
                             *proc_table[AMG_NO].status = ON;
                             stop_typ = NONE_ID;
                          }
                          break;
                          
                       case START_FD  : /* Start the FD */

                          /* First check if it is running */
                          if (proc_table[FD_NO].pid > 0)
                          {
                             (void)rec(sys_log_fd, INFO_SIGN,
                                       "%s is already running.\n",
                                       FD, __FILE__, __LINE__);
                          }
                          else
                          {
                             proc_table[FD_NO].pid = make_process(FD, work_dir);
                             log_pid(proc_table[FD_NO].pid, FD_NO + 1);
                             *proc_table[FD_NO].status = ON;
                             stop_typ = NONE_ID;
                          }
                          break;
                          
                       case AMG_READY : /* AMG has successfully executed the command */

                          /* Tell afd startup procedure that it may */
                          /* start afd_ctrl.                        */
#ifdef _FIFO_DEBUG
                          cmd[0] = ACKN; cmd[1] = '\0';
                          show_fifo_data('W', "probe_only", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(ACKN, probe_only_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to send acknowledge via fifo. (%s %d)\n",
                                       __FILE__, __LINE__);
                             exit(INCORRECT);
                          }

                          if (stop_typ == ALL_ID)
                          {
                             proc_table[AMG_NO].pid = 0;
                          }
                          else if (stop_typ == AMG_ID)
                               {
                                  proc_table[AMG_NO].pid = 0;
                                  stop_typ = NONE_ID;
                               }
                               /* We did not send any stop message. The */
                               /* AMG was started the first time and we */
                               /* may now start the next process.       */
                          else if (stop_typ == STARTUP_ID)
                               {
                                  /* Start the AFD_STAT */
                                  proc_table[STAT_NO].pid = make_process(AFD_STAT, work_dir);
                                  log_pid(proc_table[STAT_NO].pid, STAT_NO + 1);
                                  *proc_table[STAT_NO].status = ON;

                                  /* Attach to the FSA */
                                  if (fsa_attach() < 0)
                                  {
                                     (void)rec(sys_log_fd, ERROR_SIGN,
                                               "Failed to attach to FSA. (%s %d)\n",
                                               __FILE__, __LINE__);
                                  }

                                  /* Start the FD */
                                  proc_table[FD_NO].pid  = make_process(FD, work_dir);
                                  log_pid(proc_table[FD_NO].pid, FD_NO + 1);
                                  *proc_table[FD_NO].status = ON;
                                  stop_typ = NONE_ID;
                               }
                          else if (stop_typ != NONE_ID)
                               {
                                  (void)rec(sys_log_fd, WARN_SIGN,
                                            "Unknown stop_typ (%d) (%s %d)\n",
                                            stop_typ, __FILE__, __LINE__);
                               }
                          break;

                       case FD_READY  : /* FD has successfully executed the command */

                          if (stop_typ == ALL_ID)
                          {
                             proc_table[FD_NO].pid = 0;
                          }
                          else if (stop_typ == FD_ID)
                               {
                                  proc_table[FD_NO].pid = 0;
                                  stop_typ = NONE_ID;
                               }

                          break;

                       case IS_ALIVE  : /* Somebody wants to know whether an AFD */
                                        /* is running in this directory. Quickly */
                                        /* lets answer or we will be killed!     */
#ifdef _FIFO_DEBUG
                          cmd[0] = ACKN; cmd[1] = '\0';
                          show_fifo_data('W', "probe_only", cmd, 1, __FILE__, __LINE__);
#endif
                          if (send_cmd(ACKN, probe_only_fd) < 0)
                          {
                             (void)rec(sys_log_fd, WARN_SIGN,
                                       "Was not able to send acknowledge via fifo. (%s %d)\n",
                                       __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          break;

                       default        : /* Reading garbage from fifo */

                          (void)rec(sys_log_fd, FATAL_SIGN,
                                    "Reading garbage on fifo %s [%d]. Ignoring. (%s %d)\n",
                                    afd_cmd_fifo, (int)buffer[count],
                                    __FILE__, __LINE__);
                          break;
                    }
                    count++;
                 } /* while (count < n) */
              } /* if (n > 0) */
           }
                /* An error has occurred */
           else if (status < 0)
                {
                   (void)rec(sys_log_fd, FATAL_SIGN,
                             "Select error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
                   exit(INCORRECT);
                }
                else
                {
                   (void)rec(sys_log_fd, FATAL_SIGN,
                             "Unknown condition. (%s %d)\n",
                             __FILE__, __LINE__);
                   exit(INCORRECT);
                }
   } /* for (;;) */
   
   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_dirs() +++++++++++++++++++++++++++*/
static void
check_dirs(char *work_dir)
{
   char        new_dir[MAX_PATH_LENGTH],
               *ptr;
   struct stat stat_buf;

   /* First check that the working directory does exist */
   /* and make sure that it is a directory.             */
   if (stat(work_dir, &stat_buf) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not stat() %s : %s (%s %d)\n",
                    work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (!S_ISDIR(stat_buf.st_mode))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "%s is not a directory. (%s %d)\n",
                    work_dir, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Now lets check if the fifo directory is there. */
   (void)strcpy(new_dir, work_dir);
   (void)strcat(new_dir, FIFO_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is messages directory there? */
   (void)strcpy(new_dir, work_dir);
   (void)strcat(new_dir, AFD_MSG_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is file directory there? */
   (void)strcpy(new_dir, work_dir);
   (void)strcat(new_dir, AFD_FILE_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is error file directory there? */
   ptr = new_dir + strlen(new_dir);
   (void)strcpy(ptr, ERROR_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is the temp pool file directory there? */
   (void)strcpy(ptr, AFD_TMP_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is the time pool file directory there? */
   (void)strcpy(ptr, AFD_TIME_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is the incoming file directory there? */
   (void)strcpy(ptr, INCOMING_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is the file mask directory there? */
   ptr = new_dir + strlen(new_dir);
   (void)strcpy(ptr, FILE_MASK_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is the ls data from remote hosts directory there? */
   (void)strcpy(ptr, LS_DATA_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is log directory there? */
   (void)strcpy(new_dir, work_dir);
   (void)strcat(new_dir, LOG_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Is archive directory there? */
   (void)strcpy(new_dir, work_dir);
   (void)strcat(new_dir, AFD_ARCHIVE_DIR);
   if (check_dir(new_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   return;
}


/*++++++++++++++++++++++++++++ make_process() ++++++++++++++++++++++++++*/
static pid_t
make_process(char *progname, char *directory)
{
   pid_t pid;

   switch(pid = fork())
   {
      case -1: /* Could not generate process */

         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create a new process : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);

      case  0: /* Child process */

         if (execlp(progname, progname, WORK_DIR_ID, directory, (char *)0) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to start process %s : %s (%s %d)\n",
                      progname, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         exit(SUCCESS);

      default: /* Parent process */

         return(pid);
   }
}


/*+++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++*/
/*
 * Description : Checks if any process is finished (zombie), if this
 *               is the case it is killed with waitpid().
 */
static void
zombie_check(void)
{
   int status,
       i;

   for (i = 0; i < NO_OF_PROCESS; i++)
   {
      if ((proc_table[i].pid > 0) &&
          (waitpid(proc_table[i].pid, &status, WNOHANG) > 0))
      {
         if (WIFEXITED(status))
         {
            switch(WEXITSTATUS(status))
            {
               case 0 : /* Ordinary end of process. */
                        (void)rec(sys_log_fd, INFO_SIGN,
                                  "<INIT> Normal termination of process %s (%s %d)\n",
                                  proc_table[i].proc_name, __FILE__, __LINE__);
                        proc_table[i].pid = 0;
                        log_pid(0, i + 1);
                        *proc_table[i].status = STOPPED;
                        break;

               case 1 : /* Process has been stopped by user */
                        break;

               case 2 : /* Process has received SIGHUP */
                        proc_table[i].pid = make_process(proc_table[i].proc_name, p_work_dir);
                        log_pid(proc_table[i].pid, i + 1);
                        *proc_table[i].status = ON;
                        (void)rec(sys_log_fd, INFO_SIGN,
                                  "<INIT> Have restarted %s. SIGHUP received! (%s %d)\n",
                                  proc_table[i].proc_name, __FILE__, __LINE__);
                        break;

               case 3 : /* Shared memory region gone. Restart. */
                        proc_table[i].pid = make_process(proc_table[i].proc_name, p_work_dir);
                        log_pid(proc_table[i].pid, i + 1);
                        *proc_table[i].status = ON;
                        (void)rec(sys_log_fd, INFO_SIGN,
                                  "<INIT> Have restarted %s, due to missing shared memory area. (%s %d)\n",
                                  proc_table[i].proc_name,
                                  __FILE__, __LINE__);
                        break;

               default: /* Unknown error. Assume process has died. */
                        proc_table[i].pid = 0;
                        *proc_table[i].status = OFF;
                        (void)rec(sys_log_fd, ERROR_SIGN,
                                  "<INIT> Process %s has died! (%s %d)\n",
                                  proc_table[i].proc_name, __FILE__, __LINE__);

                        /* The log and archive process may never die! */
                        if ((i == SLOG_NO) || (i == TLOG_NO) ||
                            (i == RLOG_NO) ||
#ifdef _NO_MMAP
                            (i == MAPPER_NO) ||
#endif
                            (i == TDBLOG_NO) || (i == AW_NO) ||
                            (i == AFDD_NO))
                        {
                           proc_table[i].pid = make_process(proc_table[i].proc_name, p_work_dir);
                           log_pid(proc_table[i].pid, i + 1);
                           *proc_table[i].status = ON;
                           (void)rec(sys_log_fd, INFO_SIGN,
                                     "<INIT> Have restarted %s (%s %d)\n",
                                     proc_table[i].proc_name,
                                     __FILE__, __LINE__);
                        }
                        break;
            }
         }
         else if (WIFSIGNALED(status))
              {
                 /* abnormal termination */
                 proc_table[i].pid = 0;
                 *proc_table[i].status = OFF;
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "<INIT> Abnormal termination of %s! (%s %d)\n",
                           proc_table[i].proc_name, __FILE__, __LINE__);

                 /* No process may end abnormally! */
                 proc_table[i].pid = make_process(proc_table[i].proc_name, p_work_dir);
                 log_pid(proc_table[i].pid, i + 1);
                 *proc_table[i].status = ON;
                 (void)rec(sys_log_fd, INFO_SIGN,
                           "<INIT> Have restarted %s (%s %d)\n",
                           proc_table[i].proc_name, __FILE__, __LINE__);
              }
              else if (WIFSTOPPED(status))
                   {
                      /* Child stopped */
                      (void)rec(sys_log_fd, ERROR_SIGN,
                                "<INIT> Process %s has been put to sleep! (%s %d)\n",
                                proc_table[i].proc_name, __FILE__, __LINE__);
                   }
      }
   }

   return;
}


/*------------------------------ log_pid() ------------------------------*/
static void
log_pid(pid_t pid, int pos)
{
   off_t offset;

   offset = pos * sizeof(pid_t);

   /* Position write pointer */
   if (lseek(afd_active_fd, offset, SEEK_SET) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "lseek() error %s : %s (%s %d)\n",
                afd_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(afd_active_fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "write() error when writing to %s : %s (%s %d)\n",
                afd_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*++++++++++++++++++++++++++++++ afd_exit() +++++++++++++++++++++++++++++*/
static void
afd_exit(void)
{
   int         i,
               read_fd;
   pid_t       syslog = 0;
   char        *buffer,
               *ptr;
   struct stat stat_buf;

   if (probe_only != 1)
   {
      (void)rec(sys_log_fd, INFO_SIGN,
#ifdef PRE_RELEASE
                "<INIT> Stopped %s (PRE %d.%d.%d-%d)\n",
                AFD, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
                "<INIT> Stopped %s (%d.%d.%d)\n", AFD, MAJOR, MINOR, BUG_FIX);
#endif

      /*
       * If some afd_ctrl dialog is still active, let the
       * LED's show the user that all process have been stopped.
       */
      for (i = 0; i < NO_OF_PROCESS; i++)
      {
         if (i != DC_NO)
         {
            *proc_table[i].status = STOPPED;
         }
      }

      if ((read_fd = open(afd_active_file, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open %s : %s (%s %d)\n",
                   afd_active_file, strerror(errno), __FILE__, __LINE__);
         _exit(INCORRECT);
      }
      if (fstat(read_fd, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   afd_active_file, strerror(errno),  __FILE__, __LINE__);
         _exit(INCORRECT);
      }
      if ((buffer = calloc(stat_buf.st_size, sizeof(char))) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         _exit(INCORRECT);
      }
      if (read(read_fd, buffer, stat_buf.st_size) != stat_buf.st_size)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "read() error : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         _exit(INCORRECT);
      }

#ifdef _NO_MMAP
      /* We have to unmap now because when we kill the mapper */
      /* this will no longer be possible and the job will     */
      /* hang forever.                                        */
      (void)munmap_emu((void *)p_afd_status);
#endif

      /* Try to send kill signal to all running process */
      ptr = buffer;
      ptr += sizeof(pid_t);
      for (i = 1; i <= NO_OF_PROCESS; i++)
      {
         if (i == (SLOG_NO + 1))
         {
            syslog = *(pid_t *)ptr;
         }
         else
         {
            if (*(pid_t *)ptr > 0)
            {
               if (kill(*(pid_t *)ptr, SIGINT) == -1)
               {
                  if (errno != ESRCH)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to kill() %d : %s (%s %d)\n",
                               *(pid_t *)ptr, strerror(errno),
                               __FILE__, __LINE__);
                  }
               }
            }
         }
         ptr += sizeof(pid_t);
      }

#ifndef _NO_MMAP
      (void)munmap((void *)p_afd_status, sizeof(struct afd_status));
#endif

      (void)rec(sys_log_fd, INFO_SIGN,
                "=================> SHUTDOWN <=================\n");

      (void)unlink(afd_active_file);

      /* As the last process kill the system log process. */
      if (syslog > 0)
      {
         /* Give system log to tell whether AMG, FD, etc */
         /* have been stopped.                           */
         (void)my_usleep(10000L);
         (void)kill(syslog, SIGINT);
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN,
             "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
