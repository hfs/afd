/*
 *  mon_sys_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_sys_log - logs all system activity of the AFD monitor
 **
 ** SYNOPSIS
 **   mon_sys_log
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.1997 H.Kiehl Created
 **   27.12.2003 H.Kiehl Catch fifo buffer overflow.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
                                   /* strerror()                         */
#include <string.h>                /* strcmp(), strcpy(), strcat()       */
#include <stdlib.h>                /* getenv(), exit(), malloc()         */
#include <unistd.h>                /* fpathconf(), STDERR_FILENO         */
#include <signal.h>                /* signal()                           */
#include <sys/types.h>
#include <sys/stat.h>              /* stat()                             */
#include <fcntl.h>                 /* O_RDWR, O_CREAT, O_WRONLY, open()  */
#include <errno.h>
#include "logdefs.h"
#include "mondefs.h"
#include "version.h"

/* Global variables */
int                   bytes_buffered = 0,
                      sys_log_fd = STDERR_FILENO;
unsigned int          *p_log_counter,
                      total_length;
long                  fifo_size;
char                  *fifo_buffer,
                      *msg_str,
                      *p_log_fifo,
                      *p_log_his = NULL,
                      *prev_msg_str,
                      *p_work_dir;
struct afd_mon_status *p_afd_mon_status;
const char            *sys_log_name = MON_SYS_LOG_FIFO;

/* Local functions */
static void           sig_bus(int),
                      sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         log_number = 0,
               log_stat = START,
               max_mon_sys_log_files = MAX_MON_SYS_LOG_FILES,
               mon_sys_log_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int         writefd;
#endif
   char        *p_end = NULL,
               work_dir[MAX_PATH_LENGTH],
               log_file[MAX_PATH_LENGTH],
               current_log_file[MAX_PATH_LENGTH];
   FILE        *p_log_file;
   struct stat stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char mon_sys_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Initialise variables for fifo stuff */
      (void)strcpy(mon_sys_log_fifo, work_dir);
      (void)strcat(mon_sys_log_fifo, FIFO_DIR);
      (void)strcat(mon_sys_log_fifo, MON_SYS_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(mon_sys_log_fifo, &mon_sys_log_fd, &writefd) < 0)
#else
      if ((mon_sys_log_fd = open(mon_sys_log_fifo, O_RDWR)) < 0)
#endif
      {
         (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                       mon_sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if ((fifo_size = fpathconf(mon_sys_log_fd, _PC_PIPE_BUF)) < 0)
      {
         /* If we cannot determine the size of the fifo set default value */
         fifo_size = DEFAULT_FIFO_SIZE;
      }
      if (((fifo_buffer = malloc((size_t)fifo_size)) == NULL) ||
          ((msg_str = malloc((size_t)fifo_size)) == NULL) ||
          ((prev_msg_str = malloc((size_t)fifo_size)) == NULL))
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Initialise signal handlers */
   if ((signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_number(&max_mon_sys_log_files, MAX_MON_SYS_LOG_FILES_DEF,
                      MAX_MON_SYS_LOG_FILES);

   /* Attach to AFD_MON Status Area. */
   if (attach_afd_mon_status() < 0)
   {
      (void)fprintf(stderr,
                    "Failed to attach to AFD_MON status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

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

   p_log_counter = (unsigned int *)&p_afd_mon_status->mon_sys_log_ec;
   p_log_fifo = &p_afd_mon_status->mon_sys_log_fifo[0];
   get_log_number(&log_number,
                  (max_mon_sys_log_files - 1),
                  MON_SYS_LOG_NAME,
                  MON_SYS_LOG_NAME_LENGTH,
                  NULL);
   (void)sprintf(current_log_file, "%s%s/%s0", p_work_dir, LOG_DIR,
                 MON_SYS_LOG_NAME);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s", p_work_dir, LOG_DIR, MON_SYS_LOG_NAME);

   while (log_stat == START)
   {
      if (stat(current_log_file, &stat_buf) < 0)
      {
         /* The log file does not yet exist */
         total_length = 0;
      }
      else
      {
         if (stat_buf.st_size > MAX_SYS_LOGFILE_SIZE)
         {
            if (log_number < (max_mon_sys_log_files - 1))
            {
               log_number++;
            }
            reshuffel_log_files(log_number, log_file, p_end, 0, 0);
            total_length = 0;
         }
         else
         {
            total_length = stat_buf.st_size;
         }
      }

      /* Open mon_sys log file for writing */
      if ((p_log_file = fopen(current_log_file, "a+")) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : Could not open %s : %s (%s %d)\n",
                       current_log_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      log_stat = logger(p_log_file, MAX_SYS_LOGFILE_SIZE,
                        mon_sys_log_fd,  MON_SYS_LOG_RESCAN_TIME);

      (void)fclose(p_log_file);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   exit(INCORRECT);
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   exit(INCORRECT);
}
