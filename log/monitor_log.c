/*
 *  monitor_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   monitor_log - logs all monitor activities of AFD_MON
 **
 ** SYNOPSIS
 **   monitor_log [--version][-w <working directory>]
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
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* calloc()                                 */
#include <sys/types.h>       /* fdset                                    */
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval, time()                   */
#include <unistd.h>          /* fpathconf(), sysconf()                   */
#include <fcntl.h>           /* O_RDWR, open()                           */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "version.h"


/* External global variables */
int  sys_log_fd = STDERR_FILENO;
char *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   FILE           *monitor_file;
   int            log_number = 0,
                  n,
                  count,
                  length,
                  no_of_buffered_writes = 0,
                  prev_length = 0,
                  dup_msg = 0,
                  status,
                  monitor_fd;
   time_t         next_file_time,
                  now;
   long           fifo_size;
   char           *ptr,
                  *p_end,
                  *fifo_buffer,
                  work_dir[MAX_PATH_LENGTH],
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH],
                  msg_str[MAX_LINE_LENGTH],
                  prev_msg_str[MAX_LINE_LENGTH];
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char monitor_log_fifo[MAX_PATH_LENGTH],
           sys_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Create and open fifos that we need */
      (void)strcpy(monitor_log_fifo, work_dir);
      (void)strcat(monitor_log_fifo, FIFO_DIR);
      (void)strcpy(sys_log_fifo, monitor_log_fifo);
      (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
      (void)strcat(monitor_log_fifo, MON_LOG_FIFO);
      if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
      {
         if (make_fifo(sys_log_fifo) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN, "Failed to create fifo %s. (%s %d)\n",
                      sys_log_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "Failed to open() fifo %s : %s (%s %d)\n",
                   sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if ((stat(monitor_log_fifo, &stat_buf) < 0) ||
          (!S_ISFIFO(stat_buf.st_mode)))
      {
         if (make_fifo(monitor_log_fifo) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to create fifo %s. (%s %d)\n",
                      monitor_log_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if ((monitor_fd = open(monitor_log_fifo, O_RDWR)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to open() fifo %s : %s (%s %d)\n",
                   monitor_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   
   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = (int)fpathconf(monitor_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   fifo_size++;  /* Just in case */

   /* Now lets allocate memory for the fifo buffer */
   if ((fifo_buffer = calloc((size_t)fifo_size, sizeof(char))) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not allocate memory for the fifo buffer : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
   (void)umask(S_IWGRP | S_IWOTH);

   /*
    * Lets open the monitor log file. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (MAX_MON_LOG_FILES - 1),
                  MON_LOG_NAME,
                  strlen(MON_LOG_NAME));
   (void)sprintf(current_log_file, "%s%s/%s0",
                 work_dir, LOG_DIR, MON_LOG_NAME);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s",
                    work_dir, LOG_DIR, MON_LOG_NAME);

   /* Calculate time when we have to start a new file */
   next_file_time = (time(NULL) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;

   /* Is current log file already too old? */
   if (stat(current_log_file, &stat_buf) == 0)
   {
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
      {
         if (log_number < (MAX_MON_LOG_FILES - 1))
         {
            log_number++;
         }
         reshuffel_log_files(log_number, log_file, p_end);
      }
   }

   monitor_file = open_log_file(current_log_file);

   /* Ignore any SIGHUP signal */
   if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Now lets wait for data to be written to the monitor log.
    */
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(monitor_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 1L;

      /* Wait for message x seconds and then continue. */
      status = select(monitor_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(monitor_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (MAX_MON_LOG_FILES - 1))
            {
               log_number++;
            }
            if (fclose(monitor_file) == EOF)
            {
               (void)rec(sys_log_fd, ERROR_SIGN, "fclose() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
            reshuffel_log_files(log_number, log_file, p_end);
            monitor_file = open_log_file(current_log_file);
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(monitor_fd, &rset))
           {
              time(&now);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the monitor log. The data in the 
               * fifo has no special format.
               */
              ptr = fifo_buffer;
              if ((n = read(monitor_fd, fifo_buffer, fifo_size)) > 0)
              {
#ifdef _FIFO_DEBUG
                 show_fifo_data('R', monitor_log_fifo, fifo_buffer, n, __FILE__, __LINE__);
#endif

                 /* Now evaluate all data read from fifo, byte after byte */
                 count = 0;
                 while (count < n)
                 {
                    length = 0;
                    while ((*ptr != '\n') && (*ptr != '\0') && (count != n))
                    {
                       msg_str[length] = *ptr;
                       ptr++; count++; length++;
                    }
                    if (*ptr == '\n')
                    {
                       ptr++; count++;
                    }
                    msg_str[length++] = '\n';
                    msg_str[length] = '\0';

                    if (length == prev_length)
                    {
                       if (strcmp(msg_str, prev_msg_str) == 0)
                       {
                          dup_msg++;
                       }
                       else
                       {
                          if (dup_msg > 0)
                          {
                             if (dup_msg == 1)
                             {
                                (void)fprintf(monitor_file, "%s",
                                              prev_msg_str);
                             }
                             else
                             {
                                (void)fprint_dup_msg(monitor_file,
                                                     dup_msg,
                                                     &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                     &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                     now);
                             }
                             dup_msg = 0;
                          }

                          (void)fprintf(monitor_file, "%s", msg_str);
                          no_of_buffered_writes++;
                          if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                          {
                             (void)fflush(monitor_file);
                             no_of_buffered_writes = 0;
                          }
                          (void)strcpy(prev_msg_str, msg_str);
                          prev_length = length;
                       }
                    }
                    else
                    {
                       if (dup_msg > 0)
                       {
                          if (dup_msg == 1)
                          {
                             (void)fprintf(monitor_file, "%s",
                                           prev_msg_str);
                          }
                          else
                          {
                             (void)fprint_dup_msg(monitor_file,
                                                  dup_msg,
                                                  &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                  &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                  now);
                          }
                          dup_msg = 0;
                       }

                       (void)fprintf(monitor_file, "%s", msg_str);
                       no_of_buffered_writes++;
                       if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                       {
                          (void)fflush(monitor_file);
                          no_of_buffered_writes = 0;
                       }
                       (void)strcpy(prev_msg_str, msg_str);
                       prev_length = length;
                    }
                 } /* while (count < n) */
              }

              /*
               * Since we can receive a constant stream of data
               * on the fifo, we might never get that select() returns 0.
               * Thus we always have to check if it is time to create
               * a new log file.
               */
              if (now > next_file_time)
              {
                 if (log_number < (MAX_MON_LOG_FILES - 1))
                 {
                    log_number++;
                 }
                 if (fclose(monitor_file) == EOF)
                 {
                    (void)rec(sys_log_fd, ERROR_SIGN, "fclose() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                 }
                 reshuffel_log_files(log_number, log_file, p_end);
                 monitor_file = open_log_file(current_log_file);
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;
              }
           }
           else
           {
              (void)rec(sys_log_fd, FATAL_SIGN, "Select error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}
