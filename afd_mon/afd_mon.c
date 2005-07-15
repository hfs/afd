/*
 *  afd_mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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
 **   afd_mon - monitors remote AFD's
 **
 ** SYNOPSIS
 **   afd_mon [--version] [-w <working directory>]
 **
 ** DESCRIPTION
 **   The AFD (Automatic File Distributor) monitor, checks and controls
 **   the activity of remote AFD's.
 **
 **   For this it needs to contact the AFDD (AFD Daemon) on a regular
 **   interval, which can be specified in the AFD_MON_DB file for each
 **   host separatly. This file is a normal ASCII Text file which is
 **   constantly checked for any changes. Thus it is not neccessary to
 **   always restart the afd_mon in order for the change to take effect.
 **
 **   The following parametes are read from the AFD_MON_DB file:
 **      AFD alias name       - the alias name of the remote AFD.
 **      host name            - the true hostname where the AFD is
 **                             active.
 **      port                 - the port number of the AFDD.
 **      interval             - the interval (in seconds) at which
 **                             the remote AFDD is contacted.
 **      command              - the command to execute remote programs
 **                             where the AFD is active.
 **
 **   For each remote AFD specified in the AFD_MON_DB file, afd_mon
 **   will monitor the following parameters:
 **      - status of the AMG
 **      - status of the FD
 **      - status of the archive_watch
 **      - number of jobs in the queue
 **      - number of active transfers
 **      - status of what is in the system
 **      - number of hosts that are red
 **      - number of files send in this interval
 **      - number of bytes still to be send
 **      - number of errors
 **      - AFD version
 **      - AFD working directory
 **
 **   To view these activities there is an X interface called mon_ctrl.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.09.1997 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* atexit(), exit(), atoi()                */
#include <signal.h>           /* kill(), signal()                        */
#include <ctype.h>            /* isdigit()                               */
#include <time.h>             /* time()                                  */
#include <sys/types.h>
#include <sys/wait.h>         /* waitpid()                               */
#include <sys/time.h>         /* struct timeval                          */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>           /* lseek(), write(), exit(), getpid()      */
#include <errno.h>
#include "mondefs.h"
#include "version.h"

/* Global variables */
int                    got_shuttdown_message = NO,
                       mon_cmd_fd,
                       mon_resp_fd = -1,
                       msa_fd = -1,
                       msa_id,
                       no_of_afds,
                       probe_only_fd,
                       sys_log_fd = STDERR_FILENO;
off_t                  msa_size;
size_t                 proc_list_size;
pid_t                  mon_log_pid,
                       own_pid,
                       sys_log_pid;
time_t                 afd_mon_db_time;
char                   afd_mon_db_file[MAX_PATH_LENGTH],
                       mon_active_file[MAX_PATH_LENGTH],
                       mon_cmd_fifo[MAX_PATH_LENGTH],
                       probe_only_fifo[MAX_PATH_LENGTH],
                       *p_work_dir;
struct mon_status_area *msa;
struct process_list    *pl = NULL;

/* Local function prototypes */
static void            afd_mon_exit(void),
                       mon_active(void),
                       sig_bus(int),
                       sig_exit(int),
                       sig_segv(int),
                       zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            status;
   char           *ptr,
                  mon_status_file[MAX_PATH_LENGTH],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   struct stat    stat_buf;
   struct timeval timeout;
   fd_set         rset;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Check if this directory does exists, if not create it. */
   if (check_dir(work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }

   /* Now check if the log directory has been created */
   ptr = p_work_dir + strlen(p_work_dir);
   (void)strcpy(ptr, LOG_DIR);
   if (check_dir(p_work_dir, R_OK | W_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   *ptr = '\0';

   /* Initialise variables */
   (void)strcpy(mon_status_file, work_dir);
   (void)strcat(mon_status_file, FIFO_DIR);
   (void)strcpy(mon_active_file, mon_status_file);
   (void)strcat(mon_active_file, MON_ACTIVE_FILE);
   (void)strcpy(sys_log_fifo, mon_status_file);
   (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);
   (void)strcat(mon_status_file, MON_STATUS_FILE);
   (void)strcpy(afd_mon_db_file, work_dir);
   (void)strcat(afd_mon_db_file, ETC_DIR);
   (void)strcat(afd_mon_db_file, AFD_MON_CONFIG_FILE);
   own_pid = getpid();

   /* Open (create) system log fifo. */
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
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (init_fifos_mon() == INCORRECT)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to initialize fifos. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Make sure that no other afd_monitor is running in this directory */
   if (check_mon(10L) == 1)
   {
      (void)fprintf(stderr, "Another %s is active, terminating.\n", AFD_MON);
      exit(0);
   }
   mon_active();

   /* Do some cleanups when we exit */
   if (atexit(afd_mon_exit) != 0)
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
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not set signal handlers : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Read AFD_MON_DB file and create MSA (Monitor Status Area).
    */
   if (stat(afd_mon_db_file, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not stat() %s : %s (%s %d)\n",
                    afd_mon_db_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   afd_mon_db_time = stat_buf.st_mtime;
   create_msa();

   daemon_init(AFD_MON);

   /* Start log process for afd_monitor */
   if ((sys_log_pid = start_process(MON_SYS_LOG, -1)) < 0)
   {
      (void)fprintf(stderr, 
                    "ERROR   : Could not start system log process for AFD_MON. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((mon_log_pid = start_process(MON_LOG, -1)) < 0)
   {
      (void)fprintf(stderr, 
                    "ERROR   : Could not start monitor log process for AFD_MON. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)rec(sys_log_fd, INFO_SIGN,
             "=================> STARTUP <=================\n");
#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d-pre%d)\n",
             AFD_MON, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             AFD_MON, MAJOR, MINOR, BUG_FIX);
#endif

   if (msa_attach() != SUCCESS)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to attach to MSA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Start all process */
   start_all();

   /* Log all pid's in MON_ACTIVE file. */
   mon_active();

   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_SET(mon_cmd_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = 10;

      /* Wait for message x seconds and then continue. */
      status = select(mon_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(mon_cmd_fd, &rset))
      {
         int  n;
         char buffer[DEFAULT_BUFFER_SIZE];

         if ((n = read(mon_cmd_fd, buffer, DEFAULT_BUFFER_SIZE)) > 0)
         {
            int count = 0;

            /* Now evaluate all data read from fifo, byte after byte */
            while (count < n)
            {
               switch(buffer[count])
               {
                  case SHUTDOWN  : /* Shutdown AFDMON */

                     got_shuttdown_message = YES;

                     /* Shutdown of other process is handled by */
                     /* the exit handler.                       */
                     exit(SUCCESS);

                  case IS_ALIVE  : /* Somebody wants to know whether an */
                                   /* AFDMON process is running in this */
                                   /* directory.                        */

                     if (send_cmd(ACKN, probe_only_fd) < 0)
                     {
                        (void)rec(sys_log_fd, FATAL_SIGN,
                                  "Was not able to send acknowledge via fifo. (%s %d)\n",
                                  __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     break;

                  case DISABLE_MON : /* Disable monitoring of an AFD. */
                     count++;
                     if (buffer[count] == ' ')
                     {
                        int  n = 0;
                        char str_num[MAX_INT_LENGTH];

                        count++;
                        while ((isdigit(buffer[count])) &&
                               (n < MAX_INT_LENGTH))
                        {
                           str_num[n] = buffer[count];
                           count++; n++;
                        }
                        if (n > 0)
                        {
                           int pos;

                           str_num[n] = '\0';
                           pos = atoi(str_num);

                           if ((pos < no_of_afds) && (pl[pos].pid > 0))
                           {
                              msa[pos].connect_status = DISABLED;
                              stop_process(pos, NO);
                           }
                        }
                     }
                     count--;
                     break;

                  case ENABLE_MON : /* Enable monitoring of an AFD. */
                     count++;
                     if (buffer[count] == ' ')
                     {
                        int  n = 0;
                        char str_num[MAX_INT_LENGTH];

                        count++;
                        while ((isdigit(buffer[count])) &&
                               (n < MAX_INT_LENGTH))
                        {
                           str_num[n] = buffer[count];
                           count++; n++;
                        }
                        if (n > 0)
                        {
                           int pos;

                           str_num[n] = '\0';
                           pos = atoi(str_num);

                           if ((pos < no_of_afds) && (pl[pos].pid == 0))
                           {
                              msa[pos].connect_status = DISCONNECTED;
                              if ((pl[pos].pid = start_process("mon", pos)) != INCORRECT)
                              {
                                 pl[pos].start_time = time(NULL);
                              }
                           }
                        }
                     }
                     count--;
                     break;

                  default        : /* Reading garbage from fifo */

                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Reading garbage on fifo %s [%d]. Ignoring. (%s %d)\n",
                               MON_CMD_FIFO, (int)buffer[count],
                               __FILE__, __LINE__);
                     break;
               }
               count++;
            } /* while (count < n) */
         }
      }
      else if (status == 0)
           {
              if (stat(afd_mon_db_file, &stat_buf) == -1)
              {
                 (void)fprintf(stderr,
                               "ERROR   : Could not stat() %s : %s (%s %d)\n",
                               afd_mon_db_file, strerror(errno),
                               __FILE__, __LINE__);
                 exit(INCORRECT);
              }
              if (stat_buf.st_mtime != afd_mon_db_time)
              {
                 (void)rec(sys_log_fd, INFO_SIGN,
                           "Rereading AFD_MON_CONFIG.\n");
                 afd_mon_db_time = stat_buf.st_mtime;

                 /* Kill all process */
                 stop_process(-1, NO);

                 if (msa_detach() != SUCCESS)
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN,
                              "Failed to detach from MSA. (%s %d)\n",
                              __FILE__, __LINE__);
                 }

                 create_msa();

                 if (msa_attach() != SUCCESS)
                 {
                    (void)rec(sys_log_fd, FATAL_SIGN,
                              "Failed to attach to MSA. (%s %d)\n",
                              __FILE__, __LINE__);
                    exit(INCORRECT);
                 }

                 /* Start all process */
                 start_all();
              }

              /* Check if any process terminated for whatever reason. */
              zombie_check();
           }
           else
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "select() error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ zombie_check() ++++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int   faulty = NO,
         i,
         status;
   pid_t ret;

   /*
    * Check if log process are still active.
    */
   if ((ret = waitpid(sys_log_pid, &status, WNOHANG)) == sys_log_pid)
   {
      int ret_stat;

      if ((ret_stat = WIFEXITED(status)))
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "System log of %s terminated with %d. (%s %d)\n",
                   AFD_MON, ret_stat, __FILE__, __LINE__);
         sys_log_pid = 0;
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Abnormal termination of system log process of %s. (%s %d)\n",
                        AFD_MON, __FILE__, __LINE__);
              sys_log_pid = 0;
           }

      /* Restart log process. */
      (void)rec(sys_log_fd, INFO_SIGN,
                "Restart %s system log process. (%s %d)\n",
                AFD_MON, __FILE__, __LINE__);
      if ((sys_log_pid = start_process(MON_SYS_LOG, -1)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, 
                   "Could not start system log process for AFD_MON. (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else if (ret == -1)
        {
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "waitpid() error : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
        }
   if ((ret = waitpid(mon_log_pid, &status, WNOHANG)) == mon_log_pid)
   {
      int ret_stat;

      if ((ret_stat = WIFEXITED(status)))
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Monitor log of %s terminated with %d. (%s %d)\n",
                   AFD_MON, ret_stat, __FILE__, __LINE__);
         mon_log_pid = 0;
      }
      else if (WIFSIGNALED(status))
           {
              /* Abnormal termination. */
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Abnormal termination of monitor log process of %s. (%s %d)\n",
                        AFD_MON, __FILE__, __LINE__);
              mon_log_pid = 0;
           }

      /* Restart log process. */
      (void)rec(sys_log_fd, INFO_SIGN,
                "Restart %s monitor log process. (%s %d)\n",
                AFD_MON, __FILE__, __LINE__);
      if ((mon_log_pid = start_process(MON_LOG, -1)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, 
                   "Could not start monitor log process for AFD_MON. (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else if (ret == -1)
        {
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "waitpid() error : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
        }

   /*
    * Now check if all mon processes are still alive.
    */
   for (i = 0; i < no_of_afds; i++)
   {
      if (pl[i].pid > 0)
      {
         if ((ret = waitpid(pl[i].pid, &status, WNOHANG)) == pl[i].pid)
         {
            if (WIFEXITED(status))
            {
               switch(WEXITSTATUS(status))
               {
                  case SUCCESS :
                     faulty = NO;
                     pl[i].pid = 0;
                     pl[i].start_time = 0;
                     pl[i].number_of_restarts = 0;
                     break;

                  default   : /* Unkown error */
                     faulty = YES;
                     pl[i].pid = 0;
                     break;
               }
            }
            else if (WIFSIGNALED(status))
                 {
                    /* Abnormal termination. */
                    (void)rec(sys_log_fd, WARN_SIGN,
                              "Abnormal termination of process %d monitoring %s. (%s %d)\n",
                              pl[i].pid, pl[i].afd_alias,
                              __FILE__, __LINE__);
                    faulty = YES;
                    pl[i].pid = 0;
                 }
         }
         else if (ret == -1)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "waitpid() error [%d] : %s (%s %d)\n",
                           pl[i].pid, strerror(errno), __FILE__, __LINE__);
              }

         if ((faulty == YES) && (msa[i].connect_status != DISABLED))
         {
            if (pl[i].number_of_restarts < 20)
            {
               /* Restart monitor process. */
               if ((pl[i].pid = start_process("mon", i)) != INCORRECT)
               {
                  time_t current_time;

                  (void)time(&current_time);
                  if (current_time > (pl[i].start_time + 5))
                  {
                     pl[i].number_of_restarts = 0;
                  }
                  else
                  {
                     pl[i].number_of_restarts++;
                  }
                  pl[i].start_time = current_time;
               }
            }
            else
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "To many restarts of mon process for %s. Will NOT try to start it again. (%s %d)\n",
                         pl[i].afd_alias, __FILE__, __LINE__);
            }
            faulty = NO;
         }
      } /* if (pl[i].pid > 0) */
   } /* for (i = 0; i < no_of_afds; i++) */

   return;
}


/*++++++++++++++++++++++++++++ mon_active() +++++++++++++++++++++++++++++*/
/*                             ------------                              */
/* Description: Writes all pid's of processes that have been started by  */
/*              the afd_mon process to the MON_ACTIVE file. So that when */
/*              afd_mon is killed, all it's child process get eliminated */
/*              before starting it again.                                */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
mon_active(void)
{
   int    i,
          mon_active_fd;
   size_t size;
   char   *buffer,
          *ptr;

   if ((mon_active_fd = open(mon_active_file,
                             (O_RDWR | O_CREAT | O_TRUNC),
                             (S_IRUSR | S_IWUSR))) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to create %s : %s (%s %d)\n",
                mon_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   size = ((3 + no_of_afds) * sizeof(pid_t)) + sizeof(int) + 1;
   if ((buffer = malloc(size)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write data to buffer. */
   ptr = buffer;
   *(pid_t *)ptr = own_pid;
   ptr += sizeof(pid_t);
   *(pid_t *)ptr = sys_log_pid;
   ptr += sizeof(pid_t);
   *(pid_t *)ptr = mon_log_pid;
   ptr += sizeof(pid_t);
   *(int *)ptr = no_of_afds;
   ptr += sizeof(int);
   for (i = 0; i < no_of_afds; i++)
   {
      *(pid_t *)ptr = pl[i].pid;
      ptr += sizeof(pid_t);
   }

   /* Write buffer into file. */
   if (write(mon_active_fd, buffer, size) != size)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "write() error in %s : %s (%s %d)\n",
                mon_active_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(mon_active_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (buffer != NULL)
   {
      free(buffer);
   }

   return;
}


/*++++++++++++++++++++++++++++ afd_mon_exit() +++++++++++++++++++++++++++*/
static void
afd_mon_exit(void)
{
   /* Kill any job still active! */
   stop_process(-1, got_shuttdown_message);

   (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s.\n", AFD_MON);
   (void)unlink(mon_active_file);

   if (mon_log_pid > 0)
   {
      if (kill(mon_log_pid, SIGINT) == -1)
      {
         if (errno != ESRCH)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to kill monitor log process (%d) : %s (%s %d)\n",
                      mon_log_pid, strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   if (msa_detach() != SUCCESS)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to detach from MSA. (%s %d)\n",
                __FILE__, __LINE__);
   }

   (void)rec(sys_log_fd, INFO_SIGN,
             "=================> SHUTDOWN <=================\n");

   if (got_shuttdown_message == YES)
   {
      /* Tell 'mafd' that we received shutdown message */
      if (send_cmd(ACKN, mon_resp_fd) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to send ACKN : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   /* As the last process kill the system log process. */
   if (sys_log_pid > 0)
   {
      FILE *p_sys_log;

      if ((p_sys_log = fdopen(sys_log_fd, "a+")) != NULL)
      {
         (void)fflush(p_sys_log);
         (void)fclose(p_sys_log);
      }
      /* Give system log to tell whether AMG, FD, etc */
      /* have been stopped.                           */
      (void)my_usleep(10000L);
      (void)kill(sys_log_pid, SIGINT);
   }

   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   afd_mon_exit();

   /* Dump core so we know what happened. */
   abort();                                 
}          


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   afd_mon_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
