/*
 *  trans_db_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_db_log - logs all transfer debug activity of the AFD
 **
 ** SYNOPSIS
 **   trans_db_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.02.1996 H.Kiehl Created
 **   12.01.1997 H.Kiehl Stop creating endless number of log files.
 **   27.12.2003 H.Kiehl Catch fifo buffer overflow.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
                                   /* strerror()                         */
#include <string.h>                /* strcmp(), strcpy(), strcat(),      */
                                   /* strlen()                           */
#include <stdlib.h>                /* getenv(), exit(), malloc()         */
#include <unistd.h>                /* fpathconf(), STDERR_FILENO         */
#include <sys/types.h>
#include <sys/stat.h>              /* stat()                             */
#include <fcntl.h>                 /* O_RDWR, O_CREAT, O_WRONLY, open()  */
#include <signal.h>                /* signal()                           */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

/* Global variables */
int          bytes_buffered = 0,
             sys_log_fd = STDERR_FILENO;
unsigned int *p_log_counter = NULL, /* Disable writing to AFD status area. */
             total_length;
long         fifo_size;
char         *fifo_buffer,
             *msg_str,
             *p_work_dir,
             *p_log_his = NULL,
             *p_log_fifo,
             *prev_msg_str;
const char   *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         log_number = 0,
               log_stat = START,
               max_trans_db_log_files = MAX_TRANS_DB_LOG_FILES,
               trans_db_log_fd;
   char        *p_end = NULL,
               work_dir[MAX_PATH_LENGTH],
               log_file[MAX_PATH_LENGTH],
               current_log_file[MAX_PATH_LENGTH];
   FILE        *p_log_file;
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char trans_db_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Initialise variables for fifo stuff */
      (void)strcpy(trans_db_log_fifo, work_dir);
      (void)strcat(trans_db_log_fifo, FIFO_DIR);
      (void)strcat(trans_db_log_fifo, TRANS_DEBUG_LOG_FIFO);
      if ((trans_db_log_fd = open(trans_db_log_fifo, O_RDWR)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() fifo %s : %s",
                    trans_db_log_fifo, strerror(errno));
         exit(INCORRECT);
      }

      if ((fifo_size = fpathconf(trans_db_log_fd, _PC_PIPE_BUF)) < 0)
      {
         /* If we cannot determine the size of the fifo set default value */
         fifo_size = DEFAULT_FIFO_SIZE;
      }
      if (((fifo_buffer = malloc((size_t)fifo_size)) == NULL) ||
          ((msg_str = malloc((size_t)fifo_size)) == NULL) ||
          ((prev_msg_str = malloc((size_t)fifo_size)) == NULL))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                     "Could not allocate memory for the fifo buffer : %s",
                     strerror(errno));
         exit(INCORRECT);
      }
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_number(&max_trans_db_log_files, MAX_TRANS_DB_LOG_FILES_DEF,
                      MAX_TRANS_DB_LOG_FILES);

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

   get_log_number(&log_number,
                  (max_trans_db_log_files - 1),
                  TRANS_DB_LOG_NAME,
                  strlen(TRANS_DB_LOG_NAME));
   (void)sprintf(current_log_file, "%s%s/%s0",
                 p_work_dir, LOG_DIR, TRANS_DB_LOG_NAME);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s",
                    p_work_dir, LOG_DIR, TRANS_DB_LOG_NAME);

   /* Ignore any SIGHUP signal */
   if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   while (log_stat == START)
   {
      if (stat(current_log_file, &stat_buf) < 0)
      {
         /* The log file does not yet exist */
         total_length = 0;
      }
      else
      {
         if (stat_buf.st_size > MAX_TRANS_DB_LOGFILE_SIZE)
         {
            if (log_number < (max_trans_db_log_files - 1))
            {
               log_number++;
            }
            reshuffel_log_files(log_number, log_file, p_end);
            total_length = 0;
         }
         else
         {
            total_length = stat_buf.st_size;
         }
      }

      /* Open debug file for writing */
      if ((p_log_file = fopen(current_log_file, "a+")) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not fopen() %s : %s",
                    current_log_file, strerror(errno));
         exit(INCORRECT);
      }

      log_stat = logger(p_log_file, MAX_TRANS_DB_LOGFILE_SIZE,
                        trans_db_log_fd, TRANS_DB_LOG_RESCAN_TIME);

      if (fclose(p_log_file) == EOF)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Could not fclose() %s : %s",
                    current_log_file, strerror(errno));
      }
   }

   exit(SUCCESS);
}
