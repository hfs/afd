/*
 *  mon.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Deutscher Wetterdienst (DWD),
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
 **   mon - monitor process that watches one AFD
 **
 ** SYNOPSIS
 **   mon [--version] [-w <working directory>] AFD-number
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.08.1998 H.Kiehl Created
 **   05.06.1999 H.Kiehl Added remote host list.
 **   03.09.2000 H.Kiehl Addition of log history.
 **   13.09.2000 H.Kiehl Addition of top number of process.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* atexit(), exit()                         */
#include <signal.h>          /* signal()                                 */
#include <ctype.h>           /* isdigit()                                */
#include <time.h>            /* time()                                   */
#include <sys/types.h>
#include <sys/time.h>        /* struct timeval                           */
#include <sys/stat.h>
#include <sys/mman.h>        /* munmap()                                 */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "afdddefs.h"
#include "version.h"

/* Global variables */
int                    afd_no,
                       mon_log_fd = STDERR_FILENO,
                       msa_fd = -1,
                       msa_id,
                       no_of_afds,
                       sock_fd,
                       sys_log_fd = STDERR_FILENO,
                       timeout_flag;
size_t                 ahl_size;
off_t                  msa_size;
long                   tcp_timeout = 120L;
char                   ahl_file_name[MAX_PATH_LENGTH],
                       msg_str[MAX_RET_MSG_LENGTH],
                       *p_work_dir;
struct mon_status_area *msa;
struct afd_host_list   *ahl = NULL;

/* Local functions prototypes */
static int             evaluate_message(int *);
static void            mon_exit(void),
                       sig_bus(int),
                       sig_exit(int),
                       sig_segv(int);

/* #define _DEBUG_PRINT 1 */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ mon() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            bytes_done,
                  bytes_buffered = 0,
                  retry_fd,
                  status;
   time_t         new_day_time,
                  now;
   char           mon_log_fifo[MAX_PATH_LENGTH],
                  retry_fifo[MAX_PATH_LENGTH],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   fd_set         rset;
   struct stat    stat_buf;
   struct timeval timeout;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc != 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w working directory] AFD-number\n",
                    argv[0]);
      exit(SYNTAX_ERROR);
   }
   else
   {
      char *ptr = argv[1];

      while (*ptr != '\0')
      {
         if (isdigit(*ptr) == 0)
         {
            (void)fprintf(stderr,
                          "Usage: %s [-w working directory] AFD-number\n",
                          argv[0]);
            exit(SYNTAX_ERROR);
         }
         ptr++;
      }
      afd_no = atoi(argv[1]);
   }

   /* Initialize variables */
   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcpy(mon_log_fifo, sys_log_fifo);
   (void)strcat(mon_log_fifo, MON_LOG_FIFO);
   (void)strcpy(ahl_file_name, sys_log_fifo);
   (void)strcat(ahl_file_name, AHL_FILE_NAME);
   (void)strcat(ahl_file_name, argv[1]);
   (void)strcpy(retry_fifo, sys_log_fifo);
   (void)strcat(retry_fifo, RETRY_MON_FIFO);
   (void)strcat(retry_fifo, argv[1]);
   (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);

   /* Open and if necessary create system log fifo. */
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
   {
      if (errno == ENOENT)
      {
         if (make_fifo(sys_log_fifo) == SUCCESS)
         {
            if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
            {
               (void)fprintf(stderr,
                             "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                             sys_log_fifo, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not create fifo %s. (%s %d)\n",
                          sys_log_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                       sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open (create) monitor log fifo. */
   if ((stat(mon_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(mon_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       mon_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((mon_log_fd = open(mon_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                    mon_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Open (create) retry fifo. */
   if ((stat(retry_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(retry_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       retry_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((retry_fd = open(retry_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open() fifo %s : %s (%s %d)\n",
                    retry_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Do some cleanups when we exit */
   if (atexit(mon_exit) != 0)
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

   if (msa_attach() != SUCCESS)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to attach to MSA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   msa[afd_no].tr = 0;
   new_day_time = (time(NULL) / 86400) * 86400 + 86400;

   for (;;)
   {
      msa[afd_no].connect_status = CONNECTING;
      if ((status = tcp_connect(msa[afd_no].hostname, msa[afd_no].port)) != SUCCESS)
      {
         if (timeout_flag == OFF)
         {
            if (status != INCORRECT)
            {
               (void)rec(mon_log_fd, ERROR_SIGN, "%-*s: %s\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias, msg_str);
            }

            /*
             * The tcp_connect() function wrote to the log file if status
             * is INCORRECT. Thus it is not necessary to do it here again.
             */
         }
         else
         {
            (void)rec(mon_log_fd, ERROR_SIGN,
                      "%-*s: Failed to connect due to timeout. (%s %d)\n",
                      MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                      __FILE__, __LINE__);
         }
         msa[afd_no].connect_status = CONNECTION_DEFUNCT;
      }
      else
      {
         msa[afd_no].connect_status = CONNECTION_ESTABLISHED;
         (void)rec(mon_log_fd, INFO_SIGN,
                   "%-*s: ========> AFDD Connected <========\n",
                   MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                   __FILE__, __LINE__);
         if ((bytes_buffered = tcp_cmd(START_STAT_CMD)) < 0)
         {
            if (timeout_flag == OFF)
            {
               (void)rec(mon_log_fd, ERROR_SIGN,
                         "%-*s: Failed to send %s command. (%s %d)\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                         START_STAT_CMD, __FILE__, __LINE__);
               (void)rec(mon_log_fd, ERROR_SIGN, "%-*s: %s\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias, msg_str);
            }
            else
            {
               (void)rec(mon_log_fd, ERROR_SIGN,
                         "%-*s: Failed to send %s command due to timeout. (%s %d)\n",
                         MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias,
                         START_STAT_CMD, __FILE__, __LINE__);
            }
            (void)tcp_quit();
            msa[afd_no].connect_status = CONNECTION_DEFUNCT;
         }
         else
         {
            FD_ZERO(&rset);
            for (;;)
            {
               /*
                * Check if it is midnight so we can reset the top
                * rate counters.
                */
               if (time(&now) > new_day_time)
               {
                  int i;

                  for (i = (STORAGE_TIME - 1); i > 0; i--)
                  {
                     msa[afd_no].top_no_of_transfers[i] = msa[afd_no].top_no_of_transfers[i - 1];
                     msa[afd_no].top_tr[i] = msa[afd_no].top_tr[i - 1];
                     msa[afd_no].top_fr[i] = msa[afd_no].top_fr[i - 1];
                  }
                  msa[afd_no].top_no_of_transfers[0] = 0;
                  msa[afd_no].top_tr[0] = msa[afd_no].top_fr[0] = 0;
                  msa[afd_no].top_not_time = msa[afd_no].top_tr_time = msa[afd_no].top_fr_time = 0L;
                  new_day_time = (now / 86400) * 86400 + 86400;
               }

               while (bytes_buffered > 0)
               {
                  if ((bytes_buffered = read_msg()) == INCORRECT)
                  {
                     bytes_buffered -= bytes_done;
                     msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                     goto done;
                  }
                  else
                  {
                     if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                     {
                        bytes_buffered -= bytes_done;
                        timeout_flag = ON;
                        (void)tcp_quit();
                        timeout_flag = OFF;
                        msa[afd_no].connect_status = DISCONNECTED;
                        goto done;
                     }
                     bytes_buffered -= bytes_done;
                  }
               }

               /* Initialise descriptor set and timeout */
               FD_SET(sock_fd, &rset);
               timeout.tv_usec = 0;
               timeout.tv_sec = msa[afd_no].poll_interval;

               /* Wait for message x seconds and then continue. */
               status = select(sock_fd + 1, &rset, NULL, NULL, &timeout);

               if (FD_ISSET(sock_fd, &rset))
               {
                  msa[afd_no].last_data_time = time(NULL);
                  do
                  {
                     if ((bytes_buffered = read_msg()) == INCORRECT)
                     {
                        msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                        goto done;
                     }
                     else
                     {
                        if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                        {
                           bytes_buffered -= bytes_done;
                           timeout_flag = ON;
                           (void)tcp_quit();
                           timeout_flag = OFF;
                           msa[afd_no].connect_status = DISCONNECTED;
                           goto done;
                        }
                        bytes_buffered -= bytes_done;
                     }
                  } while (bytes_buffered > 0);
               }
               else if (status == 0)
                    {
                       if ((bytes_buffered = tcp_cmd(STAT_CMD)) < 0)
                       {
                          if (timeout_flag == OFF)
                          {
                             (void)rec(mon_log_fd, ERROR_SIGN,
                                       "%-*s: Failed to send %s command. (%s %d)\n",
                                       MAX_AFDNAME_LENGTH,
                                       msa[afd_no].afd_alias, STAT_CMD,
                                       __FILE__, __LINE__);
                             (void)rec(mon_log_fd, ERROR_SIGN, "%-*s: %s\n",
                                       MAX_AFDNAME_LENGTH,
                                       msa[afd_no].afd_alias, msg_str);
                          }
                          else
                          {
                             (void)rec(mon_log_fd, ERROR_SIGN,
                                       "%-*s: Failed to send %s command due to timeout. (%s %d)\n",
                                       MAX_AFDNAME_LENGTH,
                                       msa[afd_no].afd_alias, STAT_CMD,
                                       __FILE__, __LINE__);
                          }
                          (void)tcp_quit();
                          msa[afd_no].connect_status = CONNECTION_DEFUNCT;
                          break;
                       }
                       else
                       {
                          msa[afd_no].last_data_time = now + msa[afd_no].poll_interval;
                          if (evaluate_message(&bytes_done) == AFDD_SHUTTING_DOWN)
                          {
                             bytes_buffered -= bytes_done;
                             timeout_flag = ON;
                             (void)tcp_quit();
                             timeout_flag = OFF;
                             msa[afd_no].connect_status = DISCONNECTED;
                             break;
                          }
                          bytes_buffered -= bytes_done;
                       }
                    }
                    else
                    {
                       (void)rec(sys_log_fd, FATAL_SIGN,
                                 "select() error : %s (%s %d)\n",
                                 strerror(errno), __FILE__, __LINE__);
                       exit(SELECT_ERROR);
                    }
            } /* for (;;) */
         }
      }

done:

      msa[afd_no].tr = 0;

      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(retry_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = RETRY_INTERVAL;

      /* Wait for message x seconds and then continue. */
      status = select(retry_fd + 1, &rset, NULL, NULL, &timeout);

      if (FD_ISSET(retry_fd, &rset))
      {
         /*
          * This fifo is just here to wake up, so disregard whats
          * in it.
          */
         if (read(retry_fd, msg_str, MAX_RET_MSG_LENGTH) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "read() error on %s : %s (%s %d)\n",
                      retry_fifo, strerror(errno), __FILE__, __LINE__);
         }
      }
      else if (status == -1)
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "select() error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */
}


/*+++++++++++++++++++++++++++ evaluate_message() ++++++++++++++++++++++++*/
/*                            -----------------                          */
/* Description: Evaluates what is returned in msg_str. The first two     */
/*              characters always describe the content of the message:   */
/*                IS - Interval summary                                  */
/*                NH - Number of hosts                                   */
/*                MC - Maximum number of connections                     */
/*                SR - System Log Radar Information                      */
/*                AM - Status of AMG                                     */
/*                FD - Status of FD                                      */
/*                AW - Status of archive_watch                           */
/*                WD - remote AFD working directory                      */
/*                AV - AFD version                                       */
/*                HL - List of current host alias names and real         */
/*                     hostnames                                         */
/*                RH - Receive log history                               */
/*                SH - System log history                                */
/*                TH - Transfer log history                              */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
evaluate_message(int *bytes_done)
{
   char *ptr,
        *ptr_start;

   /* Evaluate what is returned and store in MSA. */
   ptr = msg_str;
   for (;;)
   {
      if (*ptr == '\0')
      {
         break;
      }
      ptr++;
   }
   *bytes_done = ptr - msg_str + 2;
   ptr = msg_str;
   if ((*ptr == 'I') && (*(ptr + 1) == 'S'))
   {
      ptr += 3;
      ptr_start = ptr;
      while ((*ptr != ' ') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ' ')
      {
         /* Store the number of files still to be send. */
         *ptr = '\0';
         msa[afd_no].fc = (unsigned int)strtoul(ptr_start, NULL, 10);
         ptr++;
         ptr_start = ptr;
         while ((*ptr != ' ') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == ' ')
         {
            /* Store the number of bytes still to be send. */
            *ptr = '\0';
            msa[afd_no].fs = (unsigned int)strtoul(ptr_start, NULL, 10);
            ptr++;
            ptr_start = ptr;
            while ((*ptr != ' ') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == ' ')
            {
               /* Store the transfer rate. */
               *ptr = '\0';
               msa[afd_no].tr = (unsigned int)strtoul(ptr_start, NULL, 10);
               if (msa[afd_no].tr > msa[afd_no].top_tr[0])
               {
                  msa[afd_no].top_tr[0] = msa[afd_no].tr;
                  msa[afd_no].top_tr_time = msa[afd_no].last_data_time;
               }
               ptr++;
               ptr_start = ptr;
               while ((*ptr != ' ') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == ' ')
               {
                  /* Store file rate. */
                  *ptr = '\0';
                  msa[afd_no].fr = (unsigned int)strtoul(ptr_start, NULL, 10);
                  if (msa[afd_no].fr > msa[afd_no].top_fr[0])
                  {
                     msa[afd_no].top_fr[0] = msa[afd_no].fr;
                     msa[afd_no].top_fr_time = msa[afd_no].last_data_time;
                  }
                  ptr++;
                  ptr_start = ptr;
                  while ((*ptr != ' ') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == ' ')
                  {
                     /* Store error counter. */
                     *ptr = '\0';
                     msa[afd_no].ec = (unsigned int)strtoul(ptr_start, NULL, 10);
                     ptr++;
                     ptr_start = ptr;
                     while ((*ptr != ' ') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == ' ')
                     {
                        /* Store error hosts. */
                        *ptr = '\0';
                        msa[afd_no].host_error_counter = atoi(ptr_start);
                        ptr++;
                        ptr_start = ptr;
                        while ((*ptr != ' ') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == ' ')
                        {
                           /* Store number of transfers. */
                           *ptr = '\0';
                           msa[afd_no].no_of_transfers = atoi(ptr_start);
                           if (msa[afd_no].no_of_transfers > msa[afd_no].top_no_of_transfers[0])
                           {
                              msa[afd_no].top_no_of_transfers[0] = msa[afd_no].no_of_transfers;
                              msa[afd_no].top_not_time = msa[afd_no].last_data_time;
                           }
                           ptr++;
                           ptr_start = ptr;
                           while ((*ptr != ' ') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\0')
                           {
                              /* Store number of jobs in queue. */
                              msa[afd_no].jobs_in_queue = atoi(ptr_start);
                           }
                        }
                     }
                  }
               }
            }
         }
      }
#ifdef _DEBUG_PRINT
      (void)fprintf(stderr, "IS %d %d %d %d %d %d %d\n",
                    msa[afd_no].fc, msa[afd_no].fs, msa[afd_no].tr,
                    msa[afd_no].fr, msa[afd_no].ec,
                    msa[afd_no].host_error_counter,
                    msa[afd_no].jobs_in_queue);
#endif /* _DEBUG_PRINT */
   }
   /*
    * Status of AMG
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'M'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].amg = (char)atoi(ptr_start);
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AM %d\n", msa[afd_no].amg);
#endif /* _DEBUG_PRINT */
        }
   /*
    * Status of FD
    */
   else if ((*ptr == 'F') && (*(ptr + 1) == 'D'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].fd = (char)atoi(ptr_start);
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "FD %d\n", msa[afd_no].fd);
#endif /* _DEBUG_PRINT */
        }
   /*
    * Status of archive watch
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'W'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].archive_watch = (char)atoi(ptr_start);
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AW %d\n", msa[afd_no].archive_watch);
#endif /* _DEBUG_PRINT */
        }
   /*
    * Number of hosts
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'H'))
        {
           int new_no_of_hosts;

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_hosts = atoi(ptr_start);
           if ((new_no_of_hosts != msa[afd_no].no_of_hosts) ||
               (ahl == NULL))
           {
              int  fd;
              char *ahl_ptr;

              if (ahl != NULL)
              {
                 if (munmap((void *)ahl, ahl_size) == -1)
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN,
                              "munmap() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                 }
              }
              ahl_size = new_no_of_hosts * sizeof(struct afd_host_list);
              msa[afd_no].no_of_hosts = new_no_of_hosts;
              if ((ahl_ptr = attach_buf(ahl_file_name, &fd,
                                        ahl_size, NULL)) == (caddr_t) -1)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Failed to mmap() %s : %s (%s %d)\n",
                           ahl_file_name, strerror(errno),
                           __FILE__, __LINE__);
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "close() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                 }
                 ahl = (struct afd_host_list *)ahl_ptr;
              }
           }
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "NH %d\n", msa[afd_no].no_of_hosts);
#endif /* _DEBUG_PRINT */
        }
   /*
    * Maximum number of connections.
    */
   else if ((*ptr == 'M') && (*(ptr + 1) == 'C'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].max_connections = atoi(ptr_start);
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "MC %d\n", msa[afd_no].max_connections);
#endif /* _DEBUG_PRINT */
        }
   /*
    * System log radar
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'R'))
        {
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              register int i = 0;
#ifdef _DEBUG_PRINT
              int  length = 2;
              char fifo_buf[(LOG_FIFO_SIZE * 4) + 4];

              fifo_buf[0] = 'S';
              fifo_buf[1] = 'R';
#endif /* _DEBUG_PRINT */

              /* Store system log entry counter. */
              *ptr = '\0';
              msa[afd_no].sys_log_ec = (unsigned int)strtoul(ptr_start, NULL, 10);
              ptr++;
              while ((*ptr != '\0') && (i < LOG_FIFO_SIZE))
              {
                 msa[afd_no].sys_log_fifo[i] = *ptr - ' ';
                 if (msa[afd_no].sys_log_fifo[i] > COLOR_POOL_SIZE)
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "Reading garbage for Transfer Log Radar entry <%d> (%s %d)\n",
                              (int)msa[afd_no].sys_log_fifo[i],
                              __FILE__, __LINE__);
                    msa[afd_no].sys_log_fifo[i] = CHAR_BACKGROUND;
                 }
#ifdef _DEBUG_PRINT
                 length += sprintf(&fifo_buf[length], " %d",
                                   (int)msa[afd_no].sys_log_fifo[i]);
#endif /* _DEBUG_PRINT */
                 ptr++; i++;
              }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif /* _DEBUG_PRINT */
           }
        }
   /*
    * Host list
    */
   else if ((*ptr == 'H') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: HL <host_number> <host alias> <real hostname> */;
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int i = 0,
                  pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_hosts)
              {
                 ptr = ptr + 1;
                 while ((*ptr != ' ') && (*ptr != '\0') &&
                        (i < MAX_HOSTNAME_LENGTH))
                 {
                    ahl[pos].host_alias[i] = *ptr;
                    ptr++; i++;
                 }
                 if (*ptr == ' ')
                 {
                    ahl[pos].host_alias[i] = '\0';
                    i = 0;
                    ptr = ptr + 1;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                    {
                       ahl[pos].real_hostname[0][i] = *ptr;
                       ptr++; i++;
                    }
                    ahl[pos].real_hostname[0][i] = '\0';
                    if (*ptr == ' ')
                    {
                       i = 0;
                       ptr = ptr + 1;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                       {
                          ahl[pos].real_hostname[1][i] = *ptr;
                          ptr++; i++;
                       }
                       ahl[pos].real_hostname[1][i] = '\0';
                    }
                    else
                    {
                       ahl[pos].real_hostname[1][0] = '\0';
                    }
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "HL %d %s %s %s\n",
                               pos, ahl[pos].host_alias,
                               ahl[pos].real_hostname[0],
                               ahl[pos].real_hostname[1]);
#endif /* _DEBUG_PRINT */
              }
              else
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Hmmm. Trying to insert at position %d, but there are only %d hosts. (%s %d)\n",
                           pos, msa[afd_no].no_of_hosts,
                           __FILE__, __LINE__);
              }
           }
        }
   /*
    * Receive Log History
    */
   else if ((*ptr == 'R') && (*(ptr + 1) == 'H'))
        {
           register int i = 0;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'R';
           fifo_buf[1] = 'H';
#endif /* _DEBUG_PRINT */

           /* Syntax: RH <history data> */;
           ptr += 3;
           while ((*ptr != '\0') && (i < MAX_LOG_HISTORY))
           {
              msa[afd_no].log_history[RECEIVE_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[RECEIVE_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Reading garbage for Receive Log History <%d> (%s %d)\n",
                           (int)msa[afd_no].log_history[RECEIVE_HISTORY][i],
                           __FILE__, __LINE__);
                 msa[afd_no].log_history[RECEIVE_HISTORY][i] = CHAR_BACKGROUND;
              }
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[RECEIVE_HISTORY][i]);
#endif /* _DEBUG_PRINT */
              ptr++; i++;
           }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif /* _DEBUG_PRINT */
        }
   /*
    * Transfer Log History
    */
   else if ((*ptr == 'T') && (*(ptr + 1) == 'H'))
        {
           register int i = 0;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'T';
           fifo_buf[1] = 'H';
#endif /* _DEBUG_PRINT */

           /* Syntax: TH <history data> */;
           ptr += 3;
           while ((*ptr != '\0') && (i < MAX_LOG_HISTORY))
           {
              msa[afd_no].log_history[TRANSFER_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[TRANSFER_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Reading garbage for Transfer Log History <%d> (%s %d)\n",
                           (int)msa[afd_no].log_history[TRANSFER_HISTORY][i],
                           __FILE__, __LINE__);
                 msa[afd_no].log_history[TRANSFER_HISTORY][i] = CHAR_BACKGROUND;
              }
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[TRANSFER_HISTORY][i]);
#endif /* _DEBUG_PRINT */
              ptr++; i++;
           }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif /* _DEBUG_PRINT */
        }
   /*
    * System Log History
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'H'))
        {
           register int i = 0;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'S';
           fifo_buf[1] = 'H';
#endif /* _DEBUG_PRINT */

           /* Syntax: SH <history data> */;
           ptr += 3;
           while ((*ptr != '\0') && (i < MAX_LOG_HISTORY))
           {
              msa[afd_no].log_history[SYSTEM_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[SYSTEM_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 (void)rec(sys_log_fd, DEBUG_SIGN,
                           "Reading garbage for System Log History <%d> (%s %d)\n",
                           (int)msa[afd_no].log_history[SYSTEM_HISTORY][i],
                           __FILE__, __LINE__);
                 msa[afd_no].log_history[SYSTEM_HISTORY][i] = CHAR_BACKGROUND;
              }
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[SYSTEM_HISTORY][i]);
#endif /* _DEBUG_PRINT */
              ptr++; i++;
           }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif /* _DEBUG_PRINT */
        }
   /*
    * AFD Version Number
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'V'))
        {
           if ((*bytes_done - 3) < MAX_VERSION_LENGTH)
           {
              (void)strcpy(msa[afd_no].afd_version, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "AV %s\n", msa[afd_no].afd_version);
#endif /* _DEBUG_PRINT */
           }
           else
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Version is %d Bytes long, but can handle only %d Bytes. (%s %d)\n",
                        *bytes_done - 3, MAX_VERSION_LENGTH,
                        __FILE__, __LINE__);
           }
        }
   /*
    * Remote AFD working directory.
    */
   else if ((*ptr == 'W') && (*(ptr + 1) == 'D'))
        {
           if ((*bytes_done - 3) < MAX_PATH_LENGTH)
           {
              (void)strcpy(msa[afd_no].r_work_dir, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "WD %s\n", msa[afd_no].r_work_dir);
#endif /* _DEBUG_PRINT */
           }
           else
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Path is %d Bytes long, but can handle only %d Bytes. (%s %d)\n",
                        *bytes_done - 3, MAX_PATH_LENGTH,
                        __FILE__, __LINE__);
           }
        }
   else if ((*ptr == '2') && (*(ptr + 1) == '1') && (*(ptr + 2) == '1') &&
            (*(ptr + 3) == '-'))
        {
           /* Ignore this, not of intrest! */;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif /* _DEBUG_PRINT */
        }
   else if (strcmp(ptr, AFDD_SHUTDOWN_MESSAGE) == 0)
        {
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif /* _DEBUG_PRINT */
           (void)rec(mon_log_fd, WARN_SIGN,
                     "%-*s: ========> AFDD SHUTDOWN <========\n",
                     MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias);
           return(AFDD_SHUTTING_DOWN);
        }
        else
        {
           (void)rec(mon_log_fd, ERROR_SIGN,
                     "%-*s: Failed to evaluate message [%s]. (%s %d)\n",
                     MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias, ptr,
                     __FILE__, __LINE__);
        }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ mon_exit() +++++++++++++++++++++++++++++*/
static void
mon_exit(void)
{
   if (tcp_quit() < 0)
   {
      (void)rec(mon_log_fd, DEBUG_SIGN,
                "%-*s: Failed to close TCP connection. (%s %d)\n",
                msa[afd_no].afd_alias, __FILE__, __LINE__);
   }
   if (msa[afd_no].connect_status == DISABLED)
   {
      msa[afd_no].fr = 0;
      msa[afd_no].fc = 0;
      msa[afd_no].fs = 0;
      msa[afd_no].ec = 0;
      msa[afd_no].no_of_transfers = 0;
      msa[afd_no].host_error_counter = 0;
   }
   else
   {
      msa[afd_no].connect_status = DISCONNECTED;
   }
   msa[afd_no].tr = 0;
   (void)rec(mon_log_fd, INFO_SIGN,
             "%-*s: ========> Disconnected <========\n",
              MAX_AFDNAME_LENGTH, msa[afd_no].afd_alias);

   if (msa_detach() != SUCCESS)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Failed to detach from MSA. (%s %d)\n",
                __FILE__, __LINE__);
   }

   (void)close(mon_log_fd);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN,
             "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);

   /* Dump core so we know what happened! */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);

   /* Dump core so we know what happened! */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
