/*
 *  archive_watch.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   archive_watch - removes old archives
 **
 ** SYNOPSIS
 **   archive_watch
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.02.1996 H.Kiehl Created
 **   22.08.1998 H.Kiehl Added some more exit handlers.
 **   12.03.2000 H.Kiehl Check so archive_watch cannot be started twice.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
#include <string.h>                /* strcpy(), strcat()                 */
#include <stdlib.h>                /* exit()                             */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>              /* struct timeval                     */
#include <fcntl.h>                 /* O_RDWR, O_RDONLY, O_NONBLOCK, etc  */
#include <unistd.h>                /* read()                             */
#include <signal.h>                /* signal()                           */
#include <errno.h>
#include "awdefs.h"
#include "version.h"

/* Global variables */
int          sys_log_fd = STDERR_FILENO;
unsigned int removed_archives = 0,
             removed_files = 0;
char         *p_work_dir;
time_t       current_time;

/* Local function prototypes. */
static void  aw_exit(void),
             sig_bus(int),
             sig_exit(int),
             sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            n,
                  status,
                  aw_cmd_fd;
   time_t         diff_time,
                  next_report_time,
                  next_rescan_time = 0L,
                  now;
   char           archive_dir[MAX_PATH_LENGTH],
                  aw_cmd_fifo[MAX_PATH_LENGTH],
                  buffer[DEFAULT_BUFFER_SIZE],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  work_dir[MAX_PATH_LENGTH];
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char *ptr;

      p_work_dir = work_dir;

      /*
       * Lock archive_watch so no other archive_watch can be started!
       */
      if ((ptr = lock_proc(AW_LOCK_ID, NO)) != NULL)
      {
         (void)fprintf(stderr,
                       "Process archive_watch already started by %s : (%s %d)\n",
                       ptr, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Initialise fifo to communicate with AFD */
   (void)strcpy(aw_cmd_fifo, work_dir);
   (void)strcat(aw_cmd_fifo, FIFO_DIR);
   (void)strcpy(sys_log_fifo, aw_cmd_fifo);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcat(aw_cmd_fifo, AW_CMD_FIFO);
   (void)strcpy(archive_dir, work_dir);
   (void)strcat(archive_dir, AFD_ARCHIVE_DIR);

   /* Create and open system log fifo */
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
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Now lets open the fifo to receive commands from the AFD */
   if ((stat(aw_cmd_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(aw_cmd_fifo) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not create fifo %s. (%s %d)\n",
                       aw_cmd_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   if ((aw_cmd_fd = coe_open(aw_cmd_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    aw_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Do some cleanups when we exit */
   if (atexit(aw_exit) != 0)
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
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Could not set signal handlers : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

#ifdef PRE_RELEASE
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (PRE %d.%d.%d-%d)\n",
             ARCHIVE_WATCH, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
   (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
             ARCHIVE_WATCH, MAJOR, MINOR, BUG_FIX);
#endif

   next_report_time = (time(NULL) / 3600) * 3600 + 3600;

   for (;;)
   {
      if (time(&now) >= next_rescan_time)
      {
         next_rescan_time = (now / ARCHIVE_STEP_TIME) *
                            ARCHIVE_STEP_TIME + ARCHIVE_STEP_TIME;
      }

      /* Initialise descriptor set and timeout */
      FD_ZERO(&rset);
      FD_SET(aw_cmd_fd, &rset);
      timeout.tv_usec = 0;
      if ((diff_time = (next_rescan_time - now)) < 0)
      {
         diff_time = 0L;
      }
      timeout.tv_sec = diff_time;

      /* Wait for message x seconds and then continue. */
      status = select(aw_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      /* Report every hour how many archives have been deleted. */
      if ((now + diff_time) >= next_report_time)
      {
         next_report_time = ((now + diff_time) / 3600) * 3600 + 3600;
#ifdef _NO_ZERO_DELETION_REPORT
         if ((removed_archives > 0) || (removed_files > 0))
         {
            (void)rec(sys_log_fd, INFO_SIGN,
                      "Removed %u archives with %u files.\n",
                      removed_archives, removed_files);
         }
#else
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Removed %u archives with %u files.\n",
                   removed_archives, removed_files);
#endif
         removed_archives = removed_files = 0;
      }

      /* Huh? Did we just sleep? */
      if (status == 0)
      {
         /* Lets go to work! */
         current_time = now + diff_time;
         inspect_archive(archive_dir);
      }
      else if (FD_ISSET(aw_cmd_fd, &rset))
           {
              /* Read the message */
              if ((n = read(aw_cmd_fd, buffer, DEFAULT_BUFFER_SIZE)) > 0)
              {
                 while (n > 0)
                 {
#ifdef _FIFO_DEBUG
                    show_fifo_data('R', "aw_cmd", buffer, n, __FILE__, __LINE__);
#endif
                    if (buffer[0] == STOP)
                    {
                       (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s\n",
                                 ARCHIVE_WATCH);
                       exit(SUCCESS);
                    }
                    else if (buffer[0] == RETRY)
                         {
                            (void)rec(sys_log_fd, INFO_SIGN,
                                      "Rescaning archive directories.\n",
                                      ARCHIVE_WATCH);
                            inspect_archive(archive_dir);
                         }
                         else
                         {
                            (void)rec(sys_log_fd, DEBUG_SIGN,
                                      "Hmmm..., reading garbage [%d] on fifo %s. (%s %d)\n",
                                      buffer[0], AW_CMD_FIFO,
                                      __FILE__, __LINE__);
                         }
                    n--;
                 } /* while (n > 0) */
              }
           }
      else if (status < 0)
           {
              (void)rec(sys_log_fd, FATAL_SIGN, "Select error : %s (%s %d)\n",
                        strerror(errno),  __FILE__, __LINE__);
              exit(INCORRECT);
           }
           else
           {
              (void)rec(sys_log_fd, FATAL_SIGN,
                        "Huh? Maybe YOU have a clue whats going on here! (%s %d)\n",
                        __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ aw_exit() +++++++++++++++++++++++++++++*/
static void
aw_exit(void)
{
   if ((removed_archives > 0) || (removed_files > 0))
   {
      (void)rec(sys_log_fd, INFO_SIGN, "Removed %u archives with %u files.\n",
                removed_archives, removed_files);
   }
   (void)rec(sys_log_fd, INFO_SIGN, "Stopped %s.\n", ARCHIVE_WATCH);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);
   aw_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   aw_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
