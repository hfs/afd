/*
 *  afd_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_stat - keeps track on the number files and the files size
 **              that is send to each host.
 **
 ** SYNOPSIS
 **   afd_stat
 **
 ** DESCRIPTION
 **   Logs the number of files, the size of the files, the number of
 **   connections and the number of errors on a per hosts basis.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.04.1996 H.Kiehl Created
 **   18.10.1997 H.Kiehl Initialize day, hour and second counter.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>         /* strcpy(), strcat(), strerror(), strcmp()  */
#include <limits.h>         /* UINT_MAX                                  */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>       /* struct timeval                            */
#include <sys/mman.h>       /* msync(), munmap()                         */
#include <unistd.h>         /* STDERR_FILENO                             */
#include <stdlib.h>         /* getenv()                                  */
#include <fcntl.h>
#include <signal.h>         /* signal()                                  */
#include <errno.h>
#include "statdefs.h"
#include "version.h"

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,
                           fsa_fd = -1,
                           fsa_id,
                           lock_fd,
                           no_of_hosts = 0,
                           other_file = NO;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
size_t                     stat_db_size = 0;
char                       *p_work_dir,
                           statistic_file[MAX_PATH_LENGTH],
                           new_statistic_file[MAX_PATH_LENGTH];
struct afdstat             *stat_db = NULL;
struct filetransfer_status *fsa;

/* Local functions prototypes. */
static void                sig_exit(int),
                           sig_segv(int),
                           sig_bus(int),
                           stat_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int   i,
                  j;
   int            current_year,
                  hour,
                  status,
                  test_sec_counter,
                  test_hour_counter;
   unsigned int   ui_value;          /* Temporary storage for uns. ints  */
   time_t         next_rescan_time = 0L,
                  now;
   long int       sleep_time;
   double         d_value;           /* Temporary storage for doubles    */
   char           work_dir[MAX_PATH_LENGTH],
                  statistic_file_name[MAX_FILENAME_LENGTH];
   struct timeval timeout;
   struct stat    stat_buf;
   struct tm      *p_ts;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   statistic_file_name[0] = '\0';
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_as(argc, argv, work_dir, statistic_file_name);

   /* Initialize variables */
   now = time(NULL);
   p_ts = gmtime(&now);

   /*
    * NOTE: We must put the hour into a temporary storage since
    *       function rec() uses the function localtime which seems
    *       to overwrite our p_ts->tm_hour with local time!?
    */
   hour = p_ts->tm_hour;
   current_year = p_ts->tm_year + 1900;
   p_work_dir = work_dir;
   if (statistic_file_name[0] == '\0')
   {
      char str_year[10];

      (void)sprintf(str_year, ".%d", current_year);
      (void)strcpy(statistic_file_name, STATISTIC_FILE);
      (void)strcpy(statistic_file, work_dir);
      (void)strcat(statistic_file, LOG_DIR);
      (void)strcpy(new_statistic_file, statistic_file);
      (void)strcat(new_statistic_file, NEW_STATISTIC_FILE);
      (void)strcat(new_statistic_file, str_year);
      (void)strcat(statistic_file, statistic_file_name);
      (void)strcat(statistic_file, str_year);
   }
   else
   {
      (void)strcpy(statistic_file, statistic_file_name);
      (void)strcpy(new_statistic_file, statistic_file);
      (void)strcat(new_statistic_file, ".NEW");
   }

   if (other_file == NO)
   {
      char sys_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(sys_log_fifo, work_dir);
      (void)strcat(sys_log_fifo, FIFO_DIR);
      (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);

      /* If the process AFD has not yet created the */
      /* system log fifo create it now.             */
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

      /* Open system log fifo */
      if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                       sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Get the fsa_id and no of host of the FSA */
   if (fsa_attach() < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to attach to FSA. (%s %d)\n",
                __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Read old AFD statistics database file if it is there. If not creat it!
    */
   read_afd_stat_db(no_of_hosts);

   /* Tell user we are starting the AFD_STAT */
   if (other_file == NO)
   {
#ifdef PRE_RELEASE
      (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d-pre%d)\n",
                AFD_STAT, MAJOR, MINOR, BUG_FIX, PRE_RELEASE);
#else
      (void)rec(sys_log_fd, INFO_SIGN, "Starting %s (%d.%d.%d)\n",
                AFD_STAT, MAJOR, MINOR, BUG_FIX);
#endif
   }

   /* Do some cleanups when we exit */
   if (atexit(stat_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);            
      exit(INCORRECT);
   }

   /* Ignore any SIGHUP signal. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   next_rescan_time = (now / STAT_RESCAN_TIME) *
                      STAT_RESCAN_TIME + STAT_RESCAN_TIME;

   /* Initialize sec_counter, hour_counter and day_counter. */
   test_sec_counter = (((p_ts->tm_min * 60) + p_ts->tm_sec) /
                       STAT_RESCAN_TIME) + 1;
   for (i = 0; i < no_of_hosts; i++)
   {
      stat_db[i].sec_counter = test_sec_counter;
      stat_db[i].hour_counter = hour;
      stat_db[i].day_counter = p_ts->tm_yday;
   }

   for (;;)
   {
      if ((sleep_time = (next_rescan_time - time(NULL))) < 0)
      {
         sleep_time = 0L;
      }
      timeout.tv_usec = 0;
      timeout.tv_sec = sleep_time;
      status = select(0, NULL, NULL, NULL, &timeout);

      /* Did we get a timeout? */
      if (status == 0)
      {
         now = time(NULL);
         if (now != next_rescan_time)
         {
            now = (now + (STAT_RESCAN_TIME / 2)) / STAT_RESCAN_TIME *
                  STAT_RESCAN_TIME;
         }
         next_rescan_time = (now / STAT_RESCAN_TIME) *
                            STAT_RESCAN_TIME + STAT_RESCAN_TIME;
         p_ts = gmtime(&now);
         test_sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) /
                            STAT_RESCAN_TIME;
         test_hour_counter = p_ts->tm_hour;
         if (test_sec_counter != stat_db[0].sec_counter)
         {
            if ((((stat_db[0].sec_counter - test_sec_counter) == 1) &&
                 (test_hour_counter == stat_db[0].hour_counter)) ||
                 ((stat_db[0].sec_counter == 0) && (test_sec_counter == 719)))
            {
               (void)sleep(STAT_RESCAN_TIME);
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Hmmm..., second counter wrong [%d -> %d]. Correcting. (%s %d)\n",
                         stat_db[0].sec_counter, test_sec_counter,
                         __FILE__, __LINE__);
               for (i = 0; i < no_of_hosts; i++)
               {
                  stat_db[i].sec_counter = test_sec_counter;
               }
            }
         }
         if (test_hour_counter != stat_db[0].hour_counter)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmmm..., hour counter wrong [%d -> %d]. Correcting. (%s %d)\n",
                      stat_db[0].hour_counter, test_hour_counter,
                      __FILE__, __LINE__);
            for (i = 0; i < no_of_hosts; i++)
            {
               stat_db[i].hour_counter = test_hour_counter;
               stat_db[i].day[stat_db[i].hour_counter].nfs = 0;
               stat_db[i].day[stat_db[i].hour_counter].nbs = 0.0;
               stat_db[i].day[stat_db[i].hour_counter].ne  = 0;
               stat_db[i].day[stat_db[i].hour_counter].nc  = 0;
            }
         }

         /*
          * If the FSA canges we have to reread everything. This
          * is easier then trying to find out where the change took
          * place and change only that part. This is not very effective.
          * But since changes in the FSA are very seldom and the
          * size of the status is relative small, it seems to be
          * the best solution at this point. (Time and experience
          * will tell if this is correct).
          */
         if (check_fsa() == YES)
         {
            read_afd_stat_db(no_of_hosts);
         }

         /*
          * Now lets update the statistics. There are two methods that
          * can be used to update the data. The first one is to have
          * a pointer telling which is the newest data. In the second
          * method we always move everything in the array one position
          * backwards. The advantage of the first method is that we
          * don't have to move around any data. However it is more
          * difficult to evaluate the data when we want to display
          * the information graphicaly. This can be done better with the
          * second method.
          * Lets try the first method first and see how it works.
          */
         for (i = 0; i < no_of_hosts; i++)
         {
            /***********************************/
            /* Handle structure entry for day. */
            /***********************************/

            /* Store number of files send. */
            ui_value = fsa[i].file_counter_done;
            if (ui_value >= stat_db[i].prev_nfs)
            {
               stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value - stat_db[i].prev_nfs;
            }
            else
            {
               /* Check if an overflow has occured */
               if ((UINT_MAX - stat_db[i].prev_nfs) <= MAX_FILES_PER_SCAN)
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value + UINT_MAX - stat_db[i].prev_nfs;
               }
               else /* To large. Lets assume it was a reset of the AFD. */
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value;
               }
            }
            stat_db[i].day[stat_db[i].hour_counter].nfs += stat_db[i].hour[stat_db[i].sec_counter].nfs;
            stat_db[i].prev_nfs = ui_value;

            /* Store number of bytes send. */
            d_value = fsa[i].bytes_send;
            if (d_value >= stat_db[i].prev_nbs)
            {
               stat_db[i].hour[stat_db[i].sec_counter].nbs = d_value - stat_db[i].prev_nbs;
            }
            else
            {
               stat_db[i].hour[stat_db[i].sec_counter].nbs = d_value;
            }
            if (stat_db[i].hour[stat_db[i].sec_counter].nbs < 0.0)
            {
               stat_db[i].hour[stat_db[i].sec_counter].nbs = 0.0;
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Hmm.... Byte counter less then zero?!? (%s %d)\n",
                         __FILE__, __LINE__);
            }
            stat_db[i].day[stat_db[i].hour_counter].nbs += stat_db[i].hour[stat_db[i].sec_counter].nbs;
            stat_db[i].prev_nbs = d_value;

            /* Store number of errors. */
            ui_value = fsa[i].total_errors;
            if (ui_value >= stat_db[i].prev_ne)
            {
               stat_db[i].hour[stat_db[i].sec_counter].ne = ui_value - stat_db[i].prev_ne;
            }
            else
            {
               stat_db[i].hour[stat_db[i].sec_counter].ne = ui_value;
            }
            stat_db[i].day[stat_db[i].hour_counter].ne += stat_db[i].hour[stat_db[i].sec_counter].ne;
            stat_db[i].prev_ne = ui_value;

            /* Store number of connections. */
            ui_value = fsa[i].connections;
            if (ui_value >= stat_db[i].prev_nc)
            {
               stat_db[i].hour[stat_db[i].sec_counter].nc = ui_value - stat_db[i].prev_nc;
            }
            else
            {
               stat_db[i].hour[stat_db[i].sec_counter].nc = ui_value;
            }
            stat_db[i].day[stat_db[i].hour_counter].nc += stat_db[i].hour[stat_db[i].sec_counter].nc;
            stat_db[i].prev_nc = ui_value;
            stat_db[i].sec_counter++;
         } /* for (i = 0; i < no_of_hosts; i++) */

         /* Did we reach another hour? */
         if (stat_db[0].sec_counter == SECS_PER_HOUR)
         {
            for (i = 0; i < no_of_hosts; i++)
            {
               /* Reset the counter for the day structure */
               stat_db[i].sec_counter = 0;
               stat_db[i].hour_counter++;

               if (stat_db[i].hour_counter == HOURS_PER_DAY)
               {
                  stat_db[i].hour_counter = 0;
                  stat_db[i].year[stat_db[i].day_counter].nfs = 0;
                  stat_db[i].year[stat_db[i].day_counter].nbs = 0.0;
                  stat_db[i].year[stat_db[i].day_counter].ne  = 0;
                  stat_db[i].year[stat_db[i].day_counter].nc  = 0;
                  for (j = 0; j < HOURS_PER_DAY; j++)
                  {
                     stat_db[i].year[stat_db[i].day_counter].nfs += stat_db[i].day[j].nfs;
                     stat_db[i].year[stat_db[i].day_counter].nbs += stat_db[i].day[j].nbs;
                     stat_db[i].year[stat_db[i].day_counter].ne  += stat_db[i].day[j].ne;
                     stat_db[i].year[stat_db[i].day_counter].nc  += stat_db[i].day[j].nc;
                  }
                  stat_db[i].day_counter++;
               }
               stat_db[i].day[stat_db[i].hour_counter].nfs = 0;
               stat_db[i].day[stat_db[i].hour_counter].nbs = 0.0;
               stat_db[i].day[stat_db[i].hour_counter].ne  = 0;
               stat_db[i].day[stat_db[i].hour_counter].nc  = 0;
            }
         } /* if (stat_db[i].sec_counter == SECS_PER_HOUR) */

         /* Did we reach another year? */
         if (current_year != (p_ts->tm_year + 1900))
         {
            if (other_file == NO)
            {
               save_old_year((p_ts->tm_year + 1900));
            }
            current_year = p_ts->tm_year + 1900;

            /*
             * Reset all values in current memory mapped file.
             */
            test_sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) /
                               STAT_RESCAN_TIME;
            for (i = 0; i < no_of_hosts; i++)
            {
               stat_db[i].sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) / STAT_RESCAN_TIME;
               stat_db[i].hour_counter = p_ts->tm_hour;
               stat_db[i].day_counter = p_ts->tm_yday;
               (void)memset(&stat_db[i].year, 0, (DAYS_PER_YEAR * sizeof(struct statistics)));
               (void)memset(&stat_db[i].day, 0, (HOURS_PER_DAY * sizeof(struct statistics)));
               (void)memset(&stat_db[i].hour, 0, (SECS_PER_HOUR * sizeof(struct statistics)));
            }
         }
      }
           /* An error has occured */
      else if (status < 0)
           {
              (void)rec(sys_log_fd, FATAL_SIGN, "Select error : %s (%s %d)\n",
                        strerror(errno),  __FILE__, __LINE__);
              exit(INCORRECT);
           }
           else
           {
              (void)rec(sys_log_fd, FATAL_SIGN, "Unknown condition. (%s %d)\n",
                        __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ stat_exit() +++++++++++++++++++++++++++++*/
static void
stat_exit(void)
{
   if (msync((void *)stat_db, stat_db_size, MS_ASYNC) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "msync() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   if (munmap((void *)stat_db, stat_db_size) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void                                                                
sig_segv(int signo)
{                  
   (void)rec(sys_log_fd, FATAL_SIGN, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
             __FILE__, __LINE__);                                             
   stat_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)rec(sys_log_fd, FATAL_SIGN, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   stat_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}

