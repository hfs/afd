/*
 *  trans_db_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996, 1997 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
                                   /* strerror()                         */
#include <string.h>                /* strcmp(), strcpy(), strcat(),      */
                                   /* strlen()                           */
#include <stdlib.h>                /* getenv(), exit()                   */
#include <unistd.h>                /* STDERR_FILENO                      */
#include <sys/types.h>
#include <sys/stat.h>              /* stat()                             */
#include <fcntl.h>                 /* O_RDWR, O_CREAT, O_WRONLY, open()  */
#include <signal.h>                /* signal()                           */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

/* Global variables */
int          *p_log_counter = NULL, /* Disable writing to AFD status area. */
             sys_log_fd = STDERR_FILENO;
unsigned int total_length;
char         *p_work_dir,
             *p_log_his = NULL,
             *p_log_fifo;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         log_number = 0,
               log_stat = START,
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
         (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                       trans_db_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
   (void)umask(S_IWGRP | S_IWOTH);

   get_log_number(&log_number,
                  (MAX_TRANS_DB_LOG_FILES - 1),
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
      (void)rec(sys_log_fd, DEBUG_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
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
         if (stat_buf.st_size > MAX_LOGFILE_SIZE)
         {
            if (log_number < (MAX_TRANS_DB_LOG_FILES - 1))
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
         (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                       current_log_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef _FIFO_DEBUG
      log_stat = logger(p_log_file, trans_db_log_fd, "trans_db_log",
                        TRANS_DB_LOG_RESCAN_TIME);
#else
      log_stat = logger(p_log_file, trans_db_log_fd, TRANS_DB_LOG_RESCAN_TIME);
#endif

      if (fclose(p_log_file) == EOF)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "fclose() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   exit(SUCCESS);
}
