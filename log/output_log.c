/*
 *  output_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   output_log - logs all file names distributed by the AFD.
 **
 ** SYNOPSIS
 **   output_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo OUTPUT_LOG_FIFO any file name
 **   that was distributed by sf_xxx(). The data in the fifo has the
 **   following structure:
 **   <TD><FS><JN><FNL><HN>\0<LFN>[ /<RFN>]\0[<AD>\0]
 **     |   |   |   |    |     |       |       |
 **     |   |   |   |    |     |       |       +-> If FNL > 0 this contains
 **     |   |   |   |    |     |       |           a \0 terminated string of
 **     |   |   |   |    |     |       |           the Archive Directory.
 **     |   |   |   |    |     |       +---------> Remote file name.
 **     |   |   |   |    |     +-----------------> Local file name.
 **     |   |   |   |    +-----------------------> \0 terminated string of
 **     |   |   |   |                              the Host Name and protocol.
 **     |   |   |   +----------------------------> An unsigned short holding
 **     |   |   |                                  the File Name Length if
 **     |   |   |                                  the file has been archived.
 **     |   |   |                                  If not, FNL = 0.
 **     |   |   +--------------------------------> Unsigned int holding the
 **     |   |                                      Job Number.
 **     |   +------------------------------------> File Size of type off_t.
 **     +----------------------------------------> Transfer Duration of type
 **                                                time_t.
 **
 **   This data is then written to the output log file in the following
 **   format:
 **
 **   863021759 btx      1 dat.txt [/...] 9888 0.03 46 btx/emp/0/9_863042081_0239
 **      |       |       |    |      |     |     |  |               |
 **      |       |       |    |      |     |     |  +-----+         |
 **      |       |       |    |      |     |     |        |         |
 **   Transfer Host Transfer Local Remote File Transfer  Job     Archive
 **    time    name   type   file   file  size duration number  directory
 **                          name   name
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.01.1997 H.Kiehl Created
 **   08.05.1997 H.Kiehl Added reading archive directory.
 **   14.02.1998 H.Kiehl Support for local and remote file name.
 **   07.01.2001 H.Kiehl Build in some checks when fifo buffer overflows.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc()                                 */
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
char *p_work_dir = NULL;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _OUTPUT_LOG
   FILE           *output_file;
   int            bytes_buffered = 0,
                  log_number = 0,
                  n,
                  length,
                  offset,
                  no_of_buffered_writes = 0,
                  check_size,
                  status,
                  log_fd;
   off_t          *file_size;
   time_t         *transfer_duration,
                  next_file_time,
                  now;
   unsigned int   *job_number;
   clock_t        clktck;
   long           fifo_size;
   char           *p_end,
                  *fifo_buffer,
                  *p_host_name,
                  *p_file_name,
                  work_dir[MAX_PATH_LENGTH],
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH];
   unsigned short *file_name_length;
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
      char output_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Create and open fifos that we need */
      (void)strcpy(output_log_fifo, work_dir);
      (void)strcat(output_log_fifo, FIFO_DIR);
      (void)strcat(output_log_fifo, OUTPUT_LOG_FIFO);
      if ((log_fd = open(output_log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (make_fifo(output_log_fifo) == SUCCESS)
            {
               if ((log_fd = open(output_log_fifo, O_RDWR)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() fifo %s : %s",
                             output_log_fifo, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to create fifo %s.", output_log_fifo);
               exit(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() fifo %s : %s",
                       output_log_fifo, strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   offset = sizeof(clock_t);
   if (sizeof(off_t) > offset)
   {
      offset = sizeof(off_t);
   }
   if (sizeof(unsigned int) > offset)
   {
      offset = sizeof(unsigned int);
   }
   
   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = (int)fpathconf(log_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (fifo_size < (offset + offset + offset + MAX_HOSTNAME_LENGTH + 2 +
                    1 + sizeof(unsigned short) + MAX_FILENAME_LENGTH +
                    MAX_FILENAME_LENGTH + 2 + MAX_FILENAME_LENGTH))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!!!");
   }

   /* Now lets allocate memory for the fifo buffer */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Get clock ticks per second, so we can calculate the transfer time.
    */
   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not get clock ticks per second : %s", strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
   (void)umask(S_IWGRP | S_IWOTH);

   /*
    * Lets open the output file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (MAX_OUTPUT_LOG_FILES - 1),
                  OUTPUT_BUFFER_FILE,
                  strlen(OUTPUT_BUFFER_FILE));
   (void)sprintf(current_log_file, "%s%s/%s0",
                 work_dir, LOG_DIR, OUTPUT_BUFFER_FILE);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s",
                    work_dir, LOG_DIR, OUTPUT_BUFFER_FILE);

   /* Calculate time when we have to start a new file */
   next_file_time = (time(NULL) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

   /* Is current log file already too old? */
   if (stat(current_log_file, &stat_buf) == 0)
   {
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
      {
         if (log_number < (MAX_OUTPUT_LOG_FILES - 1))
         {
            log_number++;
         }
         reshuffel_log_files(log_number, log_file, p_end);
      }
   }

   output_file = open_log_file(current_log_file);

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   transfer_duration = (clock_t *)fifo_buffer;
   file_size = (off_t *)(fifo_buffer + offset);
   job_number = (unsigned int *)(fifo_buffer + offset + offset);
   file_name_length = (unsigned short *)(fifo_buffer + offset + offset + offset);
   p_host_name = (char *)(fifo_buffer + offset + offset + offset + sizeof(unsigned short));
   p_file_name = (char *)(fifo_buffer + offset + offset + offset + sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 2 + 1);
   check_size = offset + offset + offset + sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 2 + 1 + 1 + 1;

   /* Ignore any SIGHUP signal */
   if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   /*
    * Now lets wait for data to be written to the file name
    * output log.
    */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_SET(log_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 3L;

      /* Wait for message x seconds and then continue. */
      status = select(log_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(output_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (MAX_OUTPUT_LOG_FILES - 1))
            {
               log_number++;
            }
            if (fclose(output_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            reshuffel_log_files(log_number, log_file, p_end);
            output_file = open_log_file(current_log_file);
            next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                             SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(log_fd, &rset))
           {
              /*
               * It is accurate enough to look at the time once only,
               * even though we are writting in a loop to the output
               * file.
               */
              (void)time(&now);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the file name output log. The data in the 
               * fifo always has the following format:
               *
               *   <transfer duration><file size><job number><host name>
               *   <file name length><file name + archive dir>
               */
              if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
                 if (n >= check_size)
                 {
                    do
                    {
                       if (*file_name_length > 0)
                       {
                          if (*file_name_length > MAX_FILENAME_LENGTH)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Hmmm, assuming that I am reading garbage from fifo since a filename cannot be %d Bytes long! [%d]",
                                        *file_name_length, n);
                             break;
                          }
                          else
                          {
                             (void)fprintf(output_file,
                                           "%-10ld %s %s %lu %.2f %u %s\n",
                                           now,
                                           p_host_name,
                                           p_file_name,
                                           (unsigned long)*file_size,
                                           *transfer_duration / (double)clktck,
                                           *job_number,
                                           &p_file_name[*file_name_length + 1]);
                             length = check_size + *file_name_length +
                                      strlen(&p_file_name[*file_name_length + 1]);
                          }
                       }
                       else
                       {
                          (void)fprintf(output_file, "%-10ld %s %s %lu %.2f %u\n",
                                        now,
                                        p_host_name,
                                        p_file_name,
                                        (unsigned long)*file_size,
                                        *transfer_duration / (double)clktck,
                                        *job_number);
                          length = check_size + strlen(p_file_name);
                       }
                       n -= length;
                       if (n > 0)
                       {
                          (void)memmove(fifo_buffer, &fifo_buffer[length], n);
                          if ((n < check_size) ||
                              (*file_name_length > MAX_FILENAME_LENGTH) ||
                              ((*file_name_length > 0) &&
                               ((check_size + *file_name_length + strlen(&p_file_name[*file_name_length + 1])) > n)) ||
                              ((*file_name_length == 0) &&
                               ((check_size + strlen(p_file_name)) > n)))
                          {
                             bytes_buffered = n;
                             no_of_buffered_writes++;
#ifdef _REPORT_BUFFER_OVERFLOW
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Fifo buffer for output_log overflowed!");
#endif /* _REPORT_BUFFER_OVERFLOW */
                             break;
                          }
                       }
                       no_of_buffered_writes++;
                    } while (n >= check_size);

                    if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_SLOW)
                    {
                       (void)fflush(output_file);
                       no_of_buffered_writes = 0;
                    }
                 }
                 else
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Hmmm. Seems like I am reading garbage from the fifo (%d).",
                               n);
                 }
              }

              /*
               * Since we can receive a constant stream of data
               * on the fifo, we might never get that select() returns 0.
               * Thus we always have to check if it is time to create
               * a new log file.
               */
              if (now > next_file_time)
              {
                 if (log_number < (MAX_OUTPUT_LOG_FILES - 1))
                 {
                    log_number++;
                 }
                 if (fclose(output_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 reshuffel_log_files(log_number, log_file, p_end);
                 output_file = open_log_file(current_log_file);
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME + SWITCH_FILE_TIME;
              }
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Select error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

#endif /* _OUTPUT_LOG */
   /* Should never come to this point. */
   exit(SUCCESS);
}
