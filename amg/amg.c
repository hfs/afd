/*
 *  amg.c - Part of AFD, an automatic file distribution program.
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
 **   amg - creates messages for the FD (File Distributor)
 **
 ** SYNOPSIS
 **   amg [working directory] [-f          filename of the AMG database
 **                            -r          rescan time
 **                            -p          max. no. of process
 **                            -b          busy time
 **                            --version   show current version]
 **
 ** DESCRIPTION
 **   The AMG (Automatic Message Generator) searches certain directories
 **   for files to then generate a message for the process FD (File
 **   Distributor). The directories where the AMG must search are
 **   specified in the DIR_CONFIG file. When it generates the message
 **   it also moves all the files from the 'user' directory to a unique
 **   directory, so the FD just needs to send all files which are in
 **   this directory. Since the message name and the directory name are
 **   the same, the FD will need no further information to get the
 **   files.
 **
 **   These 'user'-directories are scanned every DEFAULT_RESCAN_TIME
 **   (5 seconds) time. It also checks if there are any changes made to
 **   the DIR_CONFIG or HOST_CONFIG file. If so, it will reread them,
 **   stop all its process, create a new shared memory area and restart
 **   all jobs again (only if the DIR_CONFIG changes). Thus, it is not
 **   necessary to stop the AFD when entering a new host entry or removing
 **   one.
 **
 **   The AMG is also able to receive commands via the AFD_CMD_FIFO
 **   fifo from the AFD. So far only one command is recognised: STOP.
 **   This is used when the user wants to stop only the AMG or when
 **   the AFD is shutdown.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **   When it failes to start any of its jobs because they cannot
 **   access there shared memory area it will exit and return the value
 **   3. So the process init_afd can restart it.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.1995 H.Kiehl Created
 **   31.08.1997 H.Kiehl Remove check for HOST_CONFIG file.
 **   19.11.1997 H.Kiehl Return of the HOST_CONFIG file!
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), sprintf()            */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* atexit(), abort()               */
#include <time.h>                     /* ctime(), time()                 */
#include <sys/types.h>                /* fdset                           */
#include <sys/ipc.h>
#include <sys/shm.h>                  /* shmctl(), shmdt()               */
#include <sys/stat.h>
#include <sys/time.h>                 /* struct timeval                  */
#include <sys/wait.h>                 /* waitpid()                       */
#ifndef _NO_MMAP
#include <sys/mman.h>                 /* mmap(), munmap()                */
#endif
#include <signal.h>                   /* kill(), signal()                */
#include <unistd.h>                   /* select(), read(), write(),      */
                                      /* close(), unlink(), sleep()      */
#include <fcntl.h>                    /* O_RDWR, O_CREAT, O_WRONLY, etc  */
                                      /* open()                          */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"

/* Global variables */
#ifdef _DEBUG
FILE                       *p_debug_file;
#endif
int                        shm_id,      /* The shared memory ID's for    */
                                        /* the sorted data and pointers. */
                           data_length, /* The size of data for each     */
                                        /* main sorted data and pointers.*/
                           no_of_dir,   /* Number of directories in the  */
                                        /* AMG database file.            */
                           no_of_hosts,
                           fsa_fd = -1,
                           fsa_id,
                           sys_log_fd = STDERR_FILENO,
                           stop_flag = 0,
                           amg_flag = YES;
pid_t                      pid,
                           tmp_pid;
off_t                      fsa_size;
#ifndef _NO_MMAP
off_t                      afd_active_size;
#endif
char                       *p_work_dir,
                           *pid_list,
                           counter_file[MAX_PATH_LENGTH],
                           host_config_file[MAX_PATH_LENGTH],
                           dir_config_file[MAX_PATH_LENGTH],
                           cmd_fifo[MAX_PATH_LENGTH],
                           resp_fifo[MAX_FILENAME_LENGTH];
struct host_list           *hl;
struct filetransfer_status *fsa;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

/* local functions    */
static void                amg_exit(void),
                           get_afd_config_value(int *, int *),
                           notify_dir_check(void),
                           sig_segv(int),
                           sig_bus(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              i,
                    amg_cmd_fd,              /* File descriptor of the   */
                                             /* used by the controlling  */
                                             /* program AFD.             */
                    db_update_fd,            /* If the dialog for creat- */
                                             /* ing a new database has a */
                                             /* new database it can      */
                                             /* notify AMG over this     */
                                             /* fifo.                    */
                    max_fd,                  /* Biggest file descriptor. */
                    afd_active_fd,           /* File descriptor to file  */
                                             /* holding all active pid's */
                                             /* of the AFD.              */
                    status = 0,
                    fd,
                    rescan_time = DEFAULT_RESCAN_TIME,
                    max_no_proc = MAX_NO_OF_DIR_CHECKS;
   time_t           dc_old_time,
                    hc_old_time;
   char             buffer[10],
                    sys_log_fifo[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH],
                    amg_cmd_fifo[MAX_PATH_LENGTH],
                    db_update_fifo[MAX_PATH_LENGTH],
                    afd_active_file[MAX_PATH_LENGTH];
   fd_set           rset;
   struct timeval   timeout;
   struct stat      stat_buf;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef _DEBUG
   /* Open debug file for writing */
   if ((p_debug_file = fopen("amg.debug", "w")) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                    "amg.debug", strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif

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
   if (atexit(amg_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit function : %s (%s %d)\n",
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
                "Could not set signal handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   
   /* Check syntax if necessary */
   dir_config_file[0] = '\0';
   host_config_file[0] = '\0';
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if (argc > 1)
   {
      eval_input_amg(argc, argv, work_dir, dir_config_file,
                     host_config_file, &rescan_time, &max_no_proc);
   }
   p_work_dir = work_dir;
   if (dir_config_file[0] == '\0')
   {
      (void)strcpy(dir_config_file, work_dir);
      (void)strcat(dir_config_file, ETC_DIR);
      (void)strcat(dir_config_file, DEFAULT_DIR_CONFIG_FILE);
   }
   if (host_config_file[0] == '\0')
   {
      (void)strcpy(host_config_file, work_dir);
      (void)strcat(host_config_file, ETC_DIR);
      (void)strcat(host_config_file, DEFAULT_HOST_CONFIG_FILE);
   }

   /* Initialise variables with default values */
   shm_id = -1;
   (void)strcpy(amg_cmd_fifo, work_dir);
   (void)strcat(amg_cmd_fifo, FIFO_DIR);
   (void)strcpy(cmd_fifo, amg_cmd_fifo);
   (void)strcat(cmd_fifo, IP_CMD_FIFO);
   (void)strcpy(resp_fifo, amg_cmd_fifo);
   (void)strcat(resp_fifo, IP_RESP_FIFO);
   (void)strcpy(db_update_fifo, amg_cmd_fifo);
   (void)strcat(db_update_fifo, DB_UPDATE_FIFO);
   (void)strcpy(counter_file, amg_cmd_fifo);
   (void)strcat(counter_file, COUNTER_FILE);
   (void)strcpy(sys_log_fifo, amg_cmd_fifo);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcpy(afd_active_file, amg_cmd_fifo);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);
   (void)strcat(amg_cmd_fifo, AMG_CMD_FIFO);

   /* Create and open sys_log fifo. */
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * We need to write the pid of dir_check to the AFD_ACTIVE file.
    * Otherwise it can happen that two or more dir_check's run at
    * the same time, when init_afd was killed.
    */
   if ((afd_active_fd = coe_open(afd_active_file, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Could not open() %s : %s (%s %d)\n",
                afd_active_file, strerror(errno), __FILE__, __LINE__);
      pid_list = NULL;
   }
   else
   {
      if (fstat(afd_active_fd, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not fstat() %s : %s (%s %d)\n",
                   afd_active_file, strerror(errno), __FILE__, __LINE__);
         (void)close(afd_active_fd);
         pid_list = NULL;
      }
      else
      {
#ifdef _NO_MMAP
         if ((pid_list = mmap_emu(0, stat_buf.st_size,
                                  (PROT_READ | PROT_WRITE), MAP_SHARED,
                                  afd_active_file, 0)) == (caddr_t) -1)
#else
         if ((pid_list = mmap(0, stat_buf.st_size,
                              (PROT_READ | PROT_WRITE), MAP_SHARED,
                              afd_active_fd, 0)) == (caddr_t) -1)
#endif
         {
            (void)rec(sys_log_fd, WARN_SIGN, "mmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            pid_list = NULL;
         }

         afd_active_size = stat_buf.st_size;

         if (close(afd_active_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to close() %s : %s (%s %d)\n",
                      afd_active_file, strerror(errno), __FILE__, __LINE__);
         }

         /*
          * Before starting to activate new process make sure there is
          * no old process still running.
          */
         if (*(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) > 0)
         {
            if (kill(*(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))), SIGINT) == 0)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Had to kill() an old dir_check job! (%s %d)\n",
                         __FILE__, __LINE__);
            }
         }
         (void)shmctl(*(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t))), IPC_RMID, 0);
      }
   }

   /*
    * Create and initialize AMG counter file. Do it here to
    * avoid having two dir_checks trying to do the same.
    */
   if ((fd = coe_open(counter_file, O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open() %s : %s (%s %d)\n",
                counter_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise counter file with zero */
   if (write(fd, &status, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not initialise %s : %s (%s %d)\n",
                counter_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* If process AFD and AMG_DIALOG have not yet been created */
   /* we create the fifos needed to communicate with them.    */
   if ((stat(amg_cmd_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(amg_cmd_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   amg_cmd_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((stat(db_update_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(db_update_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   db_update_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open fifo to AFD to receive commands */
   if ((amg_cmd_fd = coe_open(amg_cmd_fifo, (O_RDONLY | O_NONBLOCK))) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                amg_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open fifo for edit_hc and edit_dc so they can */
   /* inform the AMG about any changes.             */
   if ((db_update_fd = coe_open(db_update_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                db_update_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   get_afd_config_value(&rescan_time, &max_no_proc);

   /* Find largest file descriptor */
   if (amg_cmd_fd > db_update_fd)
   {
      max_fd = amg_cmd_fd + 1;
   }
   else
   {
      max_fd = db_update_fd + 1;
   }

   /* Evaluate HOST_CONFIG file */
   hl = NULL;
   if (eval_host_config() == NO_ACCESS)
   {
      /*
       * Try get the host information from the current FSA.
       */
      if (fsa_attach() != INCORRECT)
      {
         size_t new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                           HOST_BUF_SIZE * sizeof(struct host_list);

         if ((hl = realloc(hl, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not reallocate memory for host list : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         for (i = 0; i < no_of_hosts; i++)
         {
            (void)memcpy(hl[i].host_alias, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
            (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
            (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
            (void)memcpy(hl[i].host_toggle_str, fsa[i].host_toggle_str, 5);
            (void)memcpy(hl[i].proxy_name, fsa[i].proxy_name, MAX_PROXY_NAME_LENGTH);
            (void)memset(hl[i].fullname, 0, MAX_FILENAME_LENGTH);
            hl[i].allowed_transfers   = fsa[i].allowed_transfers;
            hl[i].max_errors          = fsa[i].max_errors;
            hl[i].retry_interval      = fsa[i].retry_interval;
            hl[i].transfer_blksize    = fsa[i].block_size;
            hl[i].successful_retries  = fsa[i].max_successful_retries;
            hl[i].file_size_offset    = fsa[i].file_size_offset;
            hl[i].transfer_timeout    = fsa[i].transfer_timeout;
            hl[i].protocol            = fsa[i].protocol;
            hl[i].number_of_no_bursts = fsa[i].special_flag & NO_BURST_COUNT_MASK;
         }

         (void)fsa_detach();
      }
   }

   /* Get the size of the database file */
   if (stat(dir_config_file, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not get size of database file %s : %s (%s %d)\n",
                dir_config_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef _DEBUG
   (void)fprintf(stderr, ">>DEBUG<< database size = %d\n", (int)stat_buf.st_size);
#endif

   /* Since this is the first time round and this */
   /* is the time of the actual database, we      */
   /* store its value here in dc_old_time.        */
   dc_old_time = stat_buf.st_mtime;

   /* evaluate database */
   if ((no_of_dir = eval_dir_config(stat_buf.st_size)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not find any valid entries (%d) in database file %s (%s %d)\n",
                no_of_dir, dir_config_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Since there might have been an old FSA which has more information
    * then the HOST_CONFIG lets rewrite this file using the information
    * from both HOST_CONFIG and old FSA. That what is found in the
    * HOST_CONFIG will always have a higher priority.
    */
   hc_old_time = write_host_config();
   (void)rec(sys_log_fd, INFO_SIGN, "Found %d hosts in HOST_CONFIG.\n",
             no_of_hosts);

#ifdef _DEBUG
   show_shm(p_debug_file);
#endif

   /*
    * Before we start any programs copy any files that are in the
    * pool directory back to their original directories (if they
    * still exist).
    */
#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif
   clear_pool_dir();
#ifdef _DELETE_LOG
   if ((dl.fd != -1) && (dl.data != NULL))
   {
      free(dl.data);
   }
#endif

   /* First create the fifo to communicate with other process */
   (void)unlink(cmd_fifo);
   if (make_fifo(cmd_fifo) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not create fifo %s. (%s %d)\n",
                cmd_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)unlink(resp_fifo);
   if (make_fifo(resp_fifo) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not create fifo %s. (%s %d)\n",
                resp_fifo, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Start process if database has information */
   if (data_length > 0)
   {
      pid = make_process_amg(work_dir, shm_id, rescan_time, max_no_proc, no_of_dir);
      if (pid_list != NULL)
      {
         *(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) = pid;
         *(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t))) = shm_id;
      }
   }
   else
   {
      /* Process not started */
      pid = NOT_RUNNING;
   }

   /* Note time when AMG is started */
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (PRE %d.%d.%d-%d)\n",
             AMG, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             AMG, MAJOR, MINOR, BUG_FIX);
#endif

   /* Check if the database has been changed */
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(amg_cmd_fd, &rset);
      FD_SET(db_update_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = rescan_time;

      /* Wait for message x seconds and then continue. */
      status = select(max_fd, &rset, NULL, NULL, &timeout);

      /* Did we get a message from the AFD control */
      /* fifo to shutdown the AMG?                 */
      if ((status > 0) && (FD_ISSET(amg_cmd_fd, &rset)))
      {
         if (read(amg_cmd_fd, buffer, 10) > 0)
         {
            /* Show user we got shutdown message */
            (void)rec(sys_log_fd, INFO_SIGN, "%s shutting down ....\n", AMG);

            /* Do not forget to stop all running jobs */
            if (pid > 0)
            {
               int j;

               if (com(STOP) < 0)
               {
                  (void)rec(sys_log_fd, INFO_SIGN, "Giving it another try ...\n");
                  (void)com(STOP);
               }

               /* Wait for the child to terminate */
               for (j = 0; j < MAX_SHUTDOWN_TIME;  j++)
               {
                  if (waitpid(pid, NULL, WNOHANG) == pid)
                  {
                     pid = NOT_RUNNING;
                     break;
                  }
                  else
                  {
                     (void)sleep(1);
                  }
               }
            }

            stop_flag = 1;

            break;
         }
      }
           /* Did we receive a message from the edit_hc or */
           /* edit_dc dialog?                              */
      else if ((status > 0) && (FD_ISSET(db_update_fd, &rset)))
           {
              int n,
                  count = 0;

              if ((n = read(db_update_fd, buffer, 10)) > 0)
              {
#ifdef _FIFO_DEBUG
                 show_fifo_data('R', db_update_fifo, buffer, n, __FILE__, __LINE__);
#endif
                 while (count < n)
                 {
                    switch(buffer[count])
                    {
                       case HOST_CONFIG_UPDATE :
                          /* HOST_CONFIG updated by edit_hc */
                          if (fsa_attach() == INCORRECT)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN,
                                       "Could not attach to FSA! (%s %d)\n",
                                       __FILE__, __LINE__);
                             exit(INCORRECT);
                          }

                          for (i = 0; i < no_of_hosts; i++)
                          {
                             (void)memcpy(hl[i].host_alias, fsa[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
                             (void)memcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
                             (void)memcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
                             (void)memcpy(hl[i].host_toggle_str, fsa[i].host_toggle_str, 5);
                             (void)memcpy(hl[i].proxy_name, fsa[i].proxy_name, MAX_PROXY_NAME_LENGTH);
                             (void)memset(hl[i].fullname, 0, MAX_FILENAME_LENGTH);
                             hl[i].allowed_transfers   = fsa[i].allowed_transfers;
                             hl[i].max_errors          = fsa[i].max_errors;
                             hl[i].retry_interval      = fsa[i].retry_interval;
                             hl[i].transfer_blksize    = fsa[i].block_size;
                             hl[i].successful_retries  = fsa[i].max_successful_retries;
                             hl[i].file_size_offset    = fsa[i].file_size_offset;
                             hl[i].transfer_timeout    = fsa[i].transfer_timeout;
                             hl[i].protocol            = fsa[i].protocol;
                             hl[i].number_of_no_bursts = fsa[i].special_flag & NO_BURST_COUNT_MASK;
                          }

                          /* Increase HOST_CONFIG counter so others */
                          /* can see there was a change.            */
                          (*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + sizeof(int)))++;
                          (void)fsa_detach();

                          notify_dir_check();
                          hc_old_time = write_host_config();
                          (void)rec(sys_log_fd, INFO_SIGN,
                                    "Updated HOST_CONFIG file.\n");
                          break;

                       case DIR_CONFIG_UPDATE :
                          /* DIR_CONFIG updated by edit_dc */
                          (void)rec(sys_log_fd, INFO_SIGN,
                                    "This function has not yet been implemented. (%s %d)\n",
                                    __FILE__, __LINE__);
                          break;

                       case REREAD_HOST_CONFIG :
                          reread_host_config(&hc_old_time, NULL, NULL,
                                             NULL, NULL);
                          break;

                       case REREAD_DIR_CONFIG :
                          {
                             struct stat stat_buf;

                             if (stat(dir_config_file, &stat_buf) < 0)
                             {
                                (void)rec(sys_log_fd, WARN_SIGN,
                                          "Failed to stat() %s : %s (%s %d)\n",
                                          dir_config_file, strerror(errno),
                                          __FILE__, __LINE__);
                             }
                             else
                             {
                                if (dc_old_time < stat_buf.st_mtime)
                                {
                                   int              old_no_of_hosts,
                                                    rewrite_host_config = NO;
                                   size_t           old_size = 0;
                                   struct host_list *old_hl = NULL;

                                   /* Better check if there was a change in HOST_CONFIG */
                                   reread_host_config(&hc_old_time,
                                                      &old_no_of_hosts,
                                                      &rewrite_host_config,
                                                      &old_size,
                                                      &old_hl);

                                   reread_dir_config(&dc_old_time,
                                                     &hc_old_time,
                                                     old_no_of_hosts,
                                                     rewrite_host_config,
                                                     old_size,
                                                     rescan_time,
                                                     max_no_proc,
                                                     old_hl);
                                }
                             }
                          }
                          break;

                       default : 
                          /* Assume we are reading garbage */
                          (void)rec(sys_log_fd, INFO_SIGN,
                                    "Reading garbage (%d) on fifo %s (%s %d)\n",
                                    (int)buffer[count], db_update_fifo,
                                    __FILE__, __LINE__);
                          break;
                    }
                    count++;
                 }
              }
           }
           /* Did we get a timeout. */
      else if (status == 0)
           {
              /*
               * Check if the HOST_CONFIG file still exists. If not recreate
               * it from the internal current host_list structure.
               */
              if (stat(host_config_file, &stat_buf) < 0)
              {
                 if (errno == ENOENT)
                 {
                    (void)rec(sys_log_fd, INFO_SIGN,
                              "Recreating HOST_CONFIG file with %d hosts.\n",
                              no_of_hosts);
                    hc_old_time = write_host_config();
                 }
                 else
                 {
                    (void)rec(sys_log_fd, FATAL_SIGN,
                              "Could not stat() HOST_CONFIG file %s : %s (%s %d)\n",
                              host_config_file, strerror(errno),
                              __FILE__, __LINE__);
                    exit(INCORRECT);
                 }
              }
           }
           else 
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "select() error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }

      /* Check if any process died */
      if (pid > 0)
      {
         if (amg_zombie_check(&pid, WNOHANG) == YES)
         {
            struct shmid_ds dummy_buf;

            /* So what do we do now? */
            /* For now lets only tell the user that the job died. */
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Job %s has died! (%s %d)\n",
                      IT_PROC_NAME, __FILE__, __LINE__);

            /*
             * For some unknown reason it can happen that the shared
             * memory region is gone. If that is the case terminate AMG
             * so init_afd can restart it.
             */
            if ((shmctl(shm_id, IPC_STAT, &dummy_buf) == -1) &&
                (errno == EINVAL))
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Hmmm. shmctl() reports that shmid %d is invalid. Recreating shared memory region. (%s %d)\n",
                         shm_id, __FILE__, __LINE__);
               exit(3);
            }
            pid = make_process_amg(work_dir, shm_id, rescan_time, max_no_proc, no_of_dir);
            if (pid_list != NULL)
            {
               *(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) = pid;
               *(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t))) = shm_id;
            }
            (void)rec(sys_log_fd, INFO_SIGN, "Restarted %s. (%s %d)\n",
                      IT_PROC_NAME, __FILE__, __LINE__);
         }
      } /* if (pid > 0) */
   } /* for (;;) */

   /* Remove shared memory region */
   if (data_length > 0)
   {
      if (shmctl(shm_id, IPC_RMID, 0) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not remove shared memory region %d for dir_check : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   } /* if (data_length > 0) */

#ifdef _DEBUG
   /* Don't forget to close debug file */
   (void)fclose(p_debug_file);
#endif

   exit(0);
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(int *rescan_time, int *max_no_proc)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((access(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, AMG_DIR_RESCAN_TIME_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *rescan_time = atoi(value);
         if (*rescan_time < 1)
         {
            *rescan_time = DEFAULT_RESCAN_TIME;
         }
      }
      if (get_definition(buffer, MAX_NO_OF_DIR_CHECKS_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_no_proc = atoi(value);
         if ((*max_no_proc < 1) || (*max_no_proc > 10240))
         {
            *max_no_proc = MAX_NO_OF_DIR_CHECKS;
         }
      }
      free(buffer);
   }

   return;
}


/*+++++++++++++++++++++++++++ notify_dir_check() ++++++++++++++++++++++++*/
static void
notify_dir_check(void)
{
   int  fd;
   char fifo_name[MAX_PATH_LENGTH];

   (void)sprintf(fifo_name, "%s%s%s", p_work_dir, FIFO_DIR, IP_FIN_FIFO);
   if ((fd = open(fifo_name, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Could not open() fifo %s : %s (%s %d)\n",
                fifo_name, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      if (write(fd, " ", 1) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not write() to fifo %s : %s (%s %d)\n",
                   fifo_name, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*++++++++++++++++++++++++++++++ amg_exit() +++++++++++++++++++++++++++++*/
static void
amg_exit(void)
{
   (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s\n", AMG);

   /* Kill all jobs that where started */
   if (pid > 0)
   {
      if (kill(pid, SIGINT) < 0)
      {
         if (errno != ESRCH)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to kill job %d : %s (%s %d)\n",
                      pid, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   /* Destroy all shm areas that where created. */
   if (shm_id > -1)
   {
      (void)shmctl(shm_id, IPC_RMID, 0);
   }

   if (pid_list != NULL)
   {
#ifdef _NO_MMAP
      (void)munmap_emu((void *)pid_list);
#else
      (void)munmap((void *)pid_list, afd_active_size);
#endif
   }

   if (stop_flag == 0)
   {
      (void)unlink(counter_file);
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
   amg_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   amg_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
