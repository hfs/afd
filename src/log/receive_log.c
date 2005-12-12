/*
 *  receive_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   receive_log - logs all activity while receiving files
 **
 ** SYNOPSIS
 **   receive_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.01.2000 H.Kiehl Created
 **   22.09.2001 H.Kiehl Catch fifo buffer overflows.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc()                                 */
#include <time.h>            /* time()                                   */
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
int               sys_log_fd = STDERR_FILENO;
char              *iobuf = NULL,
                  *p_work_dir;
struct afd_status *p_afd_status;
const char        *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   FILE           *receive_file;
   int            bytes_buffered = 0,
                  log_number = 0,
                  n,
                  count,
                  length,
                  max_receive_log_files = MAX_RECEIVE_LOG_FILES,
                  no_of_buffered_writes = 0,
                  prev_length = 0,
                  log_pos,
                  dup_msg = 0,
                  status,
                  receive_fd;
   unsigned int   *p_log_counter;
   time_t         next_file_time,
                  next_his_time,
                  now;
   long           fifo_size;
   char           *ptr,
                  *p_end,
                  *fifo_buffer,
                  *p_log_fifo,
                  *p_log_his,
                  work_dir[MAX_PATH_LENGTH],
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH],
                  msg_str[MAX_LINE_LENGTH],
                  prev_msg_str[MAX_LINE_LENGTH];
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char receive_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Create and open fifos that we need */
      (void)strcpy(receive_log_fifo, work_dir);
      (void)strcat(receive_log_fifo, FIFO_DIR);
      (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
      if ((receive_fd = open(receive_log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (make_fifo(receive_log_fifo) == SUCCESS)
            {
               if ((receive_fd = open(receive_log_fifo, O_RDWR)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() fifo %s : %s",
                             receive_log_fifo, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to create fifo %s.", receive_log_fifo);
               exit(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() fifo %s : %s",
                       receive_log_fifo, strerror(errno));
            exit(INCORRECT);
         }
      }
   }
   
   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = fpathconf(receive_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value */
      fifo_size = DEFAULT_FIFO_SIZE;
   }

   /* Now lets allocate memory for the fifo buffer */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_number(&max_receive_log_files, MAX_RECEIVE_LOG_FILES_DEF,
                      MAX_RECEIVE_LOG_FILES);

   /* Attach to the AFD Status Area and position pointers. */
   if (attach_afd_status(NULL) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }
   p_log_counter = (unsigned int *)&p_afd_status->receive_log_ec;
   log_pos = p_afd_status->receive_log_ec % LOG_FIFO_SIZE;
   p_log_fifo = &p_afd_status->receive_log_fifo[0];
   p_log_his = &p_afd_status->receive_log_history[0];

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
#ifdef GROUP_CAN_WRITE
   (void)umask(S_IWOTH);
#else
   (void)umask(S_IWGRP | S_IWOTH);
#endif

   /*
    * Lets open the receive log file. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_receive_log_files - 1),
                  RECEIVE_LOG_NAME,
                  strlen(RECEIVE_LOG_NAME));
   (void)sprintf(current_log_file, "%s%s/%s0",
                 work_dir, LOG_DIR, RECEIVE_LOG_NAME);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s",
                    work_dir, LOG_DIR, RECEIVE_LOG_NAME);

   /* Calculate time when we have to start a new file. */
   next_file_time = (time(&now) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

   /* Calculate time when to shuffle the history array. */
   next_his_time = (now / HISTORY_LOG_INTERVAL) * HISTORY_LOG_INTERVAL +
                   HISTORY_LOG_INTERVAL;

   /* Is current log file already too old? */
   if (stat(current_log_file, &stat_buf) == 0)
   {
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
      {
         if (log_number < (max_receive_log_files - 1))
         {
            log_number++;
         }
         reshuffel_log_files(log_number, log_file, p_end);
      }
   }

   receive_file = open_log_file(current_log_file);

   /* Ignore any SIGHUP signal */
   if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   /*
    * Now lets wait for data to be written to the receive log.
    */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_SET(receive_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 1L;

      /* Wait for message x seconds and then continue. */
      status = select(receive_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(receive_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_receive_log_files - 1))
            {
               log_number++;
            }
            if (fclose(receive_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            reshuffel_log_files(log_number, log_file, p_end);
            receive_file = open_log_file(current_log_file);
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
         if (now > next_his_time)
         {
            (void)memmove(p_log_his, p_log_his + 1, MAX_LOG_HISTORY - 1);
            p_log_his[MAX_LOG_HISTORY - 1] = NO_INFORMATION;
            next_his_time = (now / HISTORY_LOG_INTERVAL) * HISTORY_LOG_INTERVAL +
                            HISTORY_LOG_INTERVAL;
         }
      }
      else if (FD_ISSET(receive_fd, &rset))
           {
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the receive log. The data in the 
               * fifo has no special format.
               */
              ptr = fifo_buffer;
              if ((n = read(receive_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
#ifdef _FIFO_DEBUG
                 show_fifo_data('R', RECEIVE_LOG_FIFO, fifo_buffer, n + bytes_buffered, __FILE__, __LINE__);
#endif
                 if (bytes_buffered != 0)
                 {
                    n += bytes_buffered;
                    bytes_buffered = 0;
                 }

                 /* Now evaluate all data read from fifo, byte after byte */
                 count = 0;
                 while (count < n)
                 {
                    length = 0;
                    while ((*ptr != '\n') && (*ptr != '\0') && (count != n))
                    {
                       msg_str[length] = *ptr;
                       ptr++; length++; count++;
                    }
                    if (*ptr == '\n')
                    {
                       ptr++; count++;
                       msg_str[length++] = '\n';
                       msg_str[length] = '\0';

                       if (p_log_counter != NULL)
                       {
                          if (log_pos == LOG_FIFO_SIZE)
                          {
                             log_pos = 0;
                          }
                          switch(msg_str[LOG_SIGN_POSITION])
                          {
                             case 'I' : p_log_fifo[log_pos] = INFO_ID;
                                        break;
                             case 'W' : p_log_fifo[log_pos] = WARNING_ID;
                                        break;
                             case 'E' : p_log_fifo[log_pos] = ERROR_ID;
                                        break;
                             case 'D' : /* Debug is NOT made vissible!!! */
                                        break;
                             case 'F' : p_log_fifo[log_pos] = FAULTY_ID;
                                        break;
                             default  : p_log_fifo[log_pos] = CHAR_BACKGROUND;
                                        break;
                          }
                          if (msg_str[LOG_SIGN_POSITION] != 'D')
                          {
                             if (p_log_fifo[log_pos] > p_log_his[MAX_LOG_HISTORY - 1])
                             {
                                p_log_his[MAX_LOG_HISTORY - 1] = p_log_fifo[log_pos];
                             }
                             log_pos++;
                             (*p_log_counter)++;
                          }
                       }

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
                                   (void)fprintf(receive_file, "%s",
                                                 prev_msg_str);
                                }
                                else
                                {
                                   (void)fprint_dup_msg(receive_file,
                                                        dup_msg,
                                                        &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                        &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                        MAX_DIR_ALIAS_LENGTH,
                                                        now);
                                }
                                dup_msg = 0;
                             }

                             (void)fprintf(receive_file, "%s", msg_str);
                             no_of_buffered_writes++;
                             if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                             {
                                (void)fflush(receive_file);
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
                                (void)fprintf(receive_file, "%s",
                                              prev_msg_str);
                             }
                             else
                             {
                                (void)fprint_dup_msg(receive_file,
                                                     dup_msg,
                                                     &prev_msg_str[LOG_SIGN_POSITION - 1],
                                                     &prev_msg_str[LOG_SIGN_POSITION + 3],
                                                     MAX_DIR_ALIAS_LENGTH,
                                                     now);
                             }
                             dup_msg = 0;
                          }

                          (void)fprintf(receive_file, "%s", msg_str);
                          no_of_buffered_writes++;
                          if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_FAST)
                          {
                             (void)fflush(receive_file);
                             no_of_buffered_writes = 0;
                          }
                          (void)strcpy(prev_msg_str, msg_str);
                          prev_length = length;
                       }
                    }
                    else
                    {
                       (void)memcpy(fifo_buffer, msg_str, length);
                       bytes_buffered = length;
                       count = n;
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
                 if (log_number < (max_receive_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(receive_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 reshuffel_log_files(log_number, log_file, p_end);
                 receive_file = open_log_file(current_log_file);
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;
              }
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

   /* Should never come to this point. */
   exit(SUCCESS);
}
