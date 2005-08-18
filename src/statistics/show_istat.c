/*
 *  show_istat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_istat - shows all input statistic information of the AFD
 **
 ** SYNOPSIS
 **   show_istat [options] [DIR_1 DIR_2 .... DIR_n]
 **               -w <work dir>   Working directory of the AFD.
 **               -f <name>       Path and name of the statistics file.
 **               -d [<x>]        Show information of all days [or day
 **                               minus x].
 **               -D              Show total summary on a per day basis.
 **               -h [<x>]        Show information of all hours [or hour
 **                               minus x].
 **               -H              Show total summary of last 24 hours.
 **               -m [<x>]        Show information of all minutes [or
 **                               minute minus x].
 **               -mr <x>         Show the last x minutes.
 **               -M [<x>]        Show total summary of last hour.
 **               -t[u]           Put in a timestamp when the output is valid.
 **               -y [<x>]        Show information of all years [or year
 **                               minus x].
 **               --version       Show version.
 **
 ** DESCRIPTION
 **   This program shows all input statistic information of the number
 **   of files and bytes received, the number of files and bytes queued
 **   and the number of files and bytes in the directory for each
 **   directory and a total for all directories, depending on the options
 **   that where used when calling 'show_istat'.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.03.2003 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror(), strcmp()    */
#include <time.h>                   /* time()                            */
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>               /* struct tm                         */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* getcwd(), exit(), close(),        */
                                    /* STDERR_FILENO                     */
#include <stdlib.h>                 /* getenv(), free(), malloc()        */
#ifdef HAVE_MMAP
#include <sys/mman.h>               /* mmap(), munmap()                  */
#endif
#include <fcntl.h>
#include <errno.h>
#include "statdefs.h"
#include "version.h"

#ifdef HAVE_MMAP
#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif
#endif

/* Global variables */
int        sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
char       *p_work_dir,
           **arglist; /* Holds list of dirs from command line when given. */
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Function prototypes */
static void display_data(double, double);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int i,
                j;
   int          current_year,
                position,
                show_min_range = 0,
                show_min = -1,
                show_min_summary = -1,
                show_hour = -1,
                show_hour_summary = -1,
                show_day = -1,
                show_day_summary = -1,
                show_year = -1,
                show_time_stamp = 0,
                show_old_year = NO,
                no_of_dirs,
                dir_counter = -1,
                stat_fd,
                year;
   time_t       now;
   double       nfr = 0.0, nbr = 0.0,
                tmp_nfr = 0.0, tmp_nbr = 0.0,
                total_nfr = 0.0, total_nbr = 0.0;
   char         work_dir[MAX_PATH_LENGTH],
                statistic_file_name[MAX_FILENAME_LENGTH],
                statistic_file[MAX_PATH_LENGTH];
   struct tm    *p_ts;
   struct stat  stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, ISTATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, statistic_file_name, &show_day, &show_day_summary,
                 &show_hour, &show_hour_summary, &show_min_range, &show_min,
                 &show_min_summary, &show_year, &dir_counter, &show_time_stamp);

   /* Initialize variables */
   p_work_dir = work_dir;
   now = time(NULL);
   p_ts = localtime(&now);
   current_year = p_ts->tm_year + 1900;
   if (strcmp(statistic_file_name, ISTATISTIC_FILE) == 0)
   {
      if (show_day > 0)
      {
         now -= (86400 * show_day);
      }
      else if (show_hour > 0)
           {
              now -= (3600 * show_hour);
           }
      else if (show_min > 0)
           {
              now -= (60 * show_min);
           }
      else if (show_year > 0)
           {
              now -= (31536000 * show_year);
           }
      p_ts = gmtime(&now);
      year = p_ts->tm_year + 1900;
      if (year < current_year)
      {
         show_old_year = YES;
         if (show_day > 0)
         {
            show_day = p_ts->tm_yday;
         }
      }
      (void)sprintf(statistic_file, "%s%s%s.%d",
                    work_dir, LOG_DIR, statistic_file_name, year);
   }
   else
   {
      char *ptr;

      (void)strcpy(statistic_file, statistic_file_name);

      ptr = statistic_file_name + strlen(statistic_file_name) - 1;
      i = 0;
      while ((isdigit(*ptr)) && (i < MAX_INT_LENGTH))
      {
         ptr--; i++;
      }
      if (*ptr == '.')
      {
         ptr++;
         year = atoi(ptr);
         if (year < current_year)
         {
            show_old_year = YES;
            if (show_day > 0)
            {
               show_day = p_ts->tm_yday;
            }
         }
      }
      else
      {
         /* We cannot know from what year this file is, so set to zero. */
         year = 0;
      }
   }

   if (stat(statistic_file, &stat_buf) != 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to stat() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (stat_buf.st_size > 0)
   {
      /* Open file */
      if ((stat_fd = open(statistic_file, O_RDONLY)) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (show_old_year == YES)
      {
         char                  *ptr;
         struct afd_year_istat *afd_istat;
         
#ifdef HAVE_MMAP
         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         (MAP_FILE | MAP_SHARED),
                         stat_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            free(ptr);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* HAVE_MMAP */
         afd_istat = (struct afd_year_istat *)(ptr + AFD_WORD_OFFSET);

         no_of_dirs = (stat_buf.st_size - AFD_WORD_OFFSET) /
                      sizeof(struct afd_year_istat);
         if (show_year != -1)
         {
            /*
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

            if (show_time_stamp > 0)
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               p_ts = localtime(&now);
               p_ts->tm_year = year - 1900;
               p_ts->tm_mon = 0;
               p_ts->tm_mday = 1;
               p_ts->tm_hour = 0;
               p_ts->tm_min = 0;
               p_ts->tm_sec = 0;
               first_time = mktime(p_ts);
               p_ts->tm_year = year + 1 - 1900;
               last_time = mktime(p_ts);
               if (show_time_stamp == 1)
               {
                  char first_time_str[25],
                       last_time_str[25];

                  (void)strftime(first_time_str, 25, "%c",
                                 localtime(&first_time));
                  (void)strftime(last_time_str, 25, "%c",
                                 localtime(&last_time));
                  (void)fprintf(stdout, "          [time span %s -> %s]\n",
                                first_time_str, last_time_str);
               }
               else
               {
                  (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                "                   [time span %ld -> %ld]\n",
#else
                                "                   [time span %lld -> %lld]\n",
#endif
                                first_time, last_time);
               }
            }

            (void)fprintf(stdout, "                       ===================================\n");
            (void)fprintf(stdout, "======================> AFD INPUT STATISTICS SUMMARY %d <========================\n", year);
            (void)fprintf(stdout, "                       ===================================\n");

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir_year(afd_istat, arglist[i],
                                                  no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No directory %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-12s",
                                   afd_istat[position].dir_alias);
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfr += (double)afd_istat[position].year[j].nfr;
                        nbr +=         afd_istat[position].year[j].nbr;
                     }
                     display_data(nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_dirs; i++)
               {
                  nfr = nbr = 0.0;
                  (void)fprintf(stdout, "%-12s", afd_istat[i].dir_alias);
                  for (j = 0; j < DAYS_PER_YEAR; j++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }

            (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
            (void)fprintf(stdout, "Total       ");
            display_data(tmp_nfr, tmp_nbr);
            (void)fprintf(stdout, "================================================================================\n");
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                          ==========================\n");
               (void)fprintf(stdout, "=========================> AFD INPUT STATISTICS DAY <===========================\n");
               (void)fprintf(stdout, "                          ==========================\n");
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-*s", MAX_DIR_ALIAS_LENGTH,
                                                   afd_istat[i].dir_alias);
                     if (show_day == 0) /* Show all days */
                     {
                        for (j = 0; j < DAYS_PER_YEAR; j++)
                        {
                           if (j == 0)
                           {
                              (void)fprintf(stdout, "%4d:", j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                           display_data(afd_istat[i].year[j].nfr,
                                        afd_istat[i].year[j].nbr);
                        }
                     }
                     else /* Show a specific day */
                     {
                        (void)fprintf(stdout, "     ");
                        nfr += (double)afd_istat[i].year[show_day].nfr;
                        nbr +=         afd_istat[i].year[show_day].nbr;
                        display_data(afd_istat[i].year[show_day].nfr,
                                     afd_istat[i].year[show_day].nbr);
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir_year(afd_istat, arglist[i],
                                                     no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No directory %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        (void)fprintf(stdout, "%-*s", MAX_DIR_ALIAS_LENGTH,
                                      afd_istat[position].dir_alias);
                        if (show_day == 0) /* Show all days */
                        {
                           for (j = 0; j < DAYS_PER_YEAR; j++)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                              display_data(afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           nfr += (double)afd_istat[position].year[show_day].nfr;
                           nbr +=         afd_istat[position].year[show_day].nbr;
                           display_data(afd_istat[position].year[show_day].nfr,
                                        afd_istat[position].year[show_day].nbr);
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               if (show_time_stamp > 0)
               {
                  time_t first_time, last_time;

                  p_ts = localtime(&now);
                  p_ts->tm_year = year - 1900;
                  p_ts->tm_mon = 0;
                  p_ts->tm_mday = 1;
                  p_ts->tm_hour = 0;
                  p_ts->tm_min = 0;
                  p_ts->tm_sec = 0;
                  first_time = mktime(p_ts);
                  p_ts->tm_year = year + 1 - 1900;
                  last_time = mktime(p_ts);
                  if (show_time_stamp == 1)
                  {
                     char first_time_str[25],
                          last_time_str[25];

                     (void)strftime(first_time_str, 25, "%c",
                                    localtime(&first_time));
                     (void)strftime(last_time_str, 25, "%c",
                                    localtime(&last_time));
                     (void)fprintf(stdout, "        [time span %s -> %s]\n",
                                   first_time_str, last_time_str);
                  }
                  else
                  {
                     (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                   "                 [time span %ld -> %ld]\n",
#else
                                   "                 [time span %lld -> %lld]\n",
#endif
                                   first_time, last_time);
                  }
               }

               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                       ================================\n");
               (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS DAY SUMMARY <=======================\n");
               (void)fprintf(stdout, "                       ================================\n");
               for (j = 0; j < DAYS_PER_YEAR; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr;
                  tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr;
                  total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_day_summary > -1) */

            (void)fprintf(stdout, "Total        ");
            display_data(total_nfr, total_nbr);
         }

#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#else
         free(ptr);
#endif /* HAVE_MMAP */
      }
      else /* Show data of current year */
      {
         char            *ptr;
         struct afdistat *afd_istat;

#ifdef HAVE_MMAP
         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         (MAP_FILE | MAP_SHARED), stat_fd, 0)) == (caddr_t) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((ptr = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, ptr, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            free(ptr);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* HAVE_MMAP */
         afd_istat = (struct afdistat *)(ptr + AFD_WORD_OFFSET);
         no_of_dirs = (stat_buf.st_size - AFD_WORD_OFFSET) /
                      sizeof(struct afdistat);

         if (show_min_range)
         {
            int left,
                sec_ints = (show_min_range * 60) / STAT_RESCAN_TIME;

            if (show_time_stamp > 0)
            {
               time_t    first_time,
                         last_time;
               struct tm *p_ts;

               p_ts = localtime(&now);
               p_ts->tm_year = year - 1900;
               p_ts->tm_mon = 0;
               p_ts->tm_mday = 1;
               p_ts->tm_hour = afd_istat[0].hour_counter;
               p_ts->tm_min = (afd_istat[0].sec_counter * STAT_RESCAN_TIME) / 60;
               p_ts->tm_sec = (afd_istat[0].sec_counter * STAT_RESCAN_TIME) % 60;
               last_time = mktime(p_ts) + (86400 * afd_istat[0].day_counter);
               first_time = last_time - (sec_ints * STAT_RESCAN_TIME);
               if (show_time_stamp == 1)
               {
                  char first_time_str[25],
                       last_time_str[25];

                  (void)strftime(first_time_str, 25, "%c",
                                 localtime(&first_time));
                  (void)strftime(last_time_str, 25, "%c",
                                 localtime(&last_time));
                  (void)fprintf(stdout, "        [time span %s -> %s]\n",
                                first_time_str, last_time_str);
               }
               else
               {
                  (void)fprintf(stdout,
#if SIZEOF_TIME_T == 4
                                "                 [time span %ld -> %ld]\n",
#else
                                "                 [time span %lld -> %lld]\n",
#endif
                                first_time, last_time);
               }
            }
            tmp_nfr = tmp_nbr = 0.0;
            (void)fprintf(stdout, "                    ========================================\n");
            (void)fprintf(stdout, "===================> AFD INPUT STATISTICS LAST %2d MINUTE(S) <===================\n", show_min_range);
            (void)fprintf(stdout, "                    ========================================\n");
            if (dir_counter < 0)
            {
               for (i = 0; i < no_of_dirs; i++)
               {
                  nfr = nbr = 0.0;
                  left = afd_istat[i].sec_counter - sec_ints;
                  if (left < 0)
                  {
                     for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                     for (j = 0; j < (sec_ints + left); j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  else
                  {
                     for (j = left; j < afd_istat[i].sec_counter; j++)
                     {
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                     }
                  }
                  (void)fprintf(stdout, "%-12s", afd_istat[i].dir_alias);
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }
            else
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No host %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     left = afd_istat[position].sec_counter - sec_ints;
                     if (left < 0)
                     {
                        for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                        for (j = 0; j < (sec_ints + left); j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                     }
                     else
                     {
                        for (j = left; j < afd_istat[position].sec_counter; j++)
                        {
                           nfr += (double)afd_istat[position].hour[j].nfr;
                           nbr +=         afd_istat[position].hour[j].nbr;
                        }
                     }
                     (void)fprintf(stdout, "%-12s",
                                   afd_istat[position].dir_alias);
                     display_data(nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
            (void)fprintf(stdout, "Total       ");
            display_data(tmp_nfr, tmp_nbr);
            (void)fprintf(stdout, "================================================================================\n");

#ifdef HAVE_MMAP
            if (munmap(ptr, stat_buf.st_size) < 0)
            {
               (void)fprintf(stderr,
                             "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                             statistic_file, strerror(errno),
                             __FILE__, __LINE__);
               exit(INCORRECT);
            }
#else
            free(ptr);
#endif /* HAVE_MMAP */
            if (close(stat_fd) == -1)
            {
               (void)fprintf(stderr,
                             "WARNING : Could not close() file %s : %s (%s %d)\n",
                             statistic_file, strerror(errno),
                             __FILE__, __LINE__);
            }
            exit(SUCCESS);
         }

         if ((show_day == -1) && (show_year == -1) && (show_hour == -1) &&
             (show_min == -1) && (show_hour_summary == -1) &&
             (show_day_summary == -1) && (show_min_summary == -1))
         {
            /*
             * Show total for all directories.
             */
            tmp_nfr = tmp_nbr = 0.0;

            (void)fprintf(stdout, "                         ==============================\n");
            (void)fprintf(stdout, "========================> AFD INPUT STATISTICS SUMMARY <========================\n");
            (void)fprintf(stdout, "                         ==============================\n");

            if (dir_counter > 0)
            {
               for (i = 0; i < dir_counter; i++)
               {
                  if ((position = locate_dir(afd_istat, arglist[i],
                                             no_of_dirs)) < 0)
                  {
                     (void)fprintf(stdout,
                                   "No directory %s found in statistic database.\n",
                                   arglist[i]);
                  }
                  else
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-12s",
                                   afd_istat[position].dir_alias);
                     for (j = 0; j < afd_istat[position].sec_counter; j++)
                     {
                        nfr += (double)afd_istat[position].hour[j].nfr;
                        nbr +=         afd_istat[position].hour[j].nbr;
                     }
                     for (j = 0; j < afd_istat[position].hour_counter; j++)
                     {
                        nfr += (double)afd_istat[position].day[j].nfr;
                        nbr +=         afd_istat[position].day[j].nbr;
                     }
                     for (j = 0; j < afd_istat[position].day_counter; j++)
                     {
                        nfr += (double)afd_istat[position].year[j].nfr;
                        nbr +=         afd_istat[position].year[j].nbr;
                     }
                     display_data(nfr, nbr);
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_dirs; i++)
               {
                  nfr = nbr = 0.0;
                  (void)fprintf(stdout, "%-12s", afd_istat[i].dir_alias);
                  for (j = 0; j < afd_istat[i].sec_counter; j++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  for (j = 0; j < afd_istat[i].hour_counter; j++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  for (j = 0; j < afd_istat[i].day_counter; j++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }
            (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
            (void)fprintf(stdout, "Total       ");
            display_data(tmp_nfr, tmp_nbr);
            (void)fprintf(stdout, "================================================================================\n");
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                          ==========================\n");
               (void)fprintf(stdout, "=========================> AFD INPUT STATISTICS DAY <===========================\n");
               (void)fprintf(stdout, "                          ==========================\n");
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_istat[i].dir_alias);
                     if (show_day == 0) /* Show all days */
                     {
                        for (j = 0; j < afd_istat[i].day_counter; j++)
                        {
                           if (j == 0)
                           {
                              (void)fprintf(stdout, "%4d:", j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                           display_data(afd_istat[i].year[j].nfr,
                                        afd_istat[i].year[j].nbr);
                        }
                        if (afd_istat[i].day_counter == 0)
                        {
                           (void)fprintf(stdout, "%4d:", 0);
                           display_data(0.0, 0.0);
                        }
                     }
                     else /* Show a specific day */
                     {
                        (void)fprintf(stdout, "     ");
                        if (show_day < DAYS_PER_YEAR)
                        {
                           if (afd_istat[i].day_counter < show_day)
                           {
                              j = DAYS_PER_YEAR -
                                  (show_day - afd_istat[i].day_counter);
                           }
                           else
                           {
                              j = afd_istat[i].day_counter - show_day;
                           }
                           nfr += (double)afd_istat[i].year[j].nfr;
                           nbr +=         afd_istat[i].year[j].nbr;
                           display_data(afd_istat[i].year[j].nfr,
                                        afd_istat[i].year[j].nbr);
                        }
                        else
                        {
                           display_data(0.0, 0.0);
                        }
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No direcotry %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        (void)fprintf(stdout, "%-8s",
                                      afd_istat[position].dir_alias);
                        if (show_day == 0) /* Show all days */
                        {
                           for (j = 0; j < afd_istat[position].day_counter; j++)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                              display_data(afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           if (show_day < DAYS_PER_YEAR)
                           {
                              if (afd_istat[position].day_counter < show_day)
                              {
                                 j = DAYS_PER_YEAR -
                                     (show_day - afd_istat[position].day_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].day_counter - show_day;
                              }
                              nfr += (double)afd_istat[position].year[j].nfr;
                              nbr +=         afd_istat[position].year[j].nbr;
                              display_data(afd_istat[position].year[j].nfr,
                                           afd_istat[position].year[j].nbr);
                           }
                           else
                           {
                              display_data(0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               struct tm *p_ts;

               p_ts = localtime(&now);
               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                       ================================\n");
               (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS DAY SUMMARY <=======================\n");
               (void)fprintf(stdout, "                       ================================\n");
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].year[j].nfr;
                     nbr +=         afd_istat[i].year[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfr, sec_nbr;

               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                          ===========================\n");
               (void)fprintf(stdout, "=========================> AFD INPUT STATISTICS HOUR <==========================\n");
               (void)fprintf(stdout, "                          ===========================\n");
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_istat[i].dir_alias);
                     if (show_hour == 0) /* Show all hours of the day */
                     {
                        for (j = 0; j < afd_istat[i].hour_counter; j++)
                        {
                           if (j == 0)
                           {
                              (void)fprintf(stdout, "%4d:", j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfr += (double)afd_istat[i].day[j].nfr;
                           nbr +=         afd_istat[i].day[j].nbr;
                           display_data(afd_istat[i].day[j].nfr,
                                        afd_istat[i].day[j].nbr);
                        }
                        if (afd_istat[i].hour_counter == 0)
                        {
                           (void)fprintf(stdout, "* %2d:",
                                         afd_istat[i].hour_counter);
                        }
                        else
                        {
                           (void)fprintf(stdout, "        * %2d:",
                                         afd_istat[i].hour_counter);
                        }
                        sec_nfr = sec_nbr = 0.0;
                        for (j = 0; j < afd_istat[i].sec_counter; j++)
                        {
                           sec_nfr += (double)afd_istat[i].hour[j].nfr;
                           sec_nbr +=         afd_istat[i].hour[j].nbr;
                        }
                        display_data(sec_nfr, sec_nbr);
                        nfr += sec_nfr; nbr += sec_nbr;
                        for (j = (afd_istat[i].hour_counter + 1);
                             j < HOURS_PER_DAY; j++)
                        {
                           (void)fprintf(stdout, "%12d:", j);
                           nfr += (double)afd_istat[i].day[j].nfr;
                           nbr +=         afd_istat[i].day[j].nbr;
                           display_data(afd_istat[i].day[j].nfr,
                                        afd_istat[i].day[j].nbr);
                        }
                     }
                     else /* Show a specific hour */
                     {
                        (void)fprintf(stdout, "     ");
                        if (show_hour < HOURS_PER_DAY)
                        {
                           if (afd_istat[i].hour_counter < show_hour)
                           {
                              j = HOURS_PER_DAY -
                                  (show_hour - afd_istat[i].hour_counter);
                           }
                           else
                           {
                              j = afd_istat[i].hour_counter - show_hour;
                           }
                           nfr += (double)afd_istat[i].day[j].nfr;
                           nbr +=         afd_istat[i].day[j].nbr;
                           display_data(afd_istat[i].day[j].nfr,
                                        afd_istat[i].day[j].nbr);
                        }
                        else
                        {
                           display_data(0.0, 0.0);
                        }
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (dir_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                     "No directory %s found in statistic database.\n",
                                     arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        (void)fprintf(stdout, "%-8s",
                                      afd_istat[position].dir_alias);
                        if (show_hour == 0) /* Show all hours of the day */
                        {
                           for (j = 0; j < afd_istat[position].hour_counter; j++)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfr += (double)afd_istat[position].day[j].nfr;
                              nbr +=         afd_istat[position].day[j].nbr;
                              display_data(afd_istat[position].day[j].nfr,
                                           afd_istat[position].day[j].nbr);
                           }
                           if (afd_istat[position].hour_counter == 0)
                           {
                              (void)fprintf(stdout, "* %2d:",
                                            afd_istat[position].hour_counter);
                           }
                           else
                           {
                              (void)fprintf(stdout, "        * %2d:",
                                            afd_istat[position].hour_counter);
                           }
                           sec_nfr = sec_nbr = 0.0;
                           for (j = 0; j < afd_istat[position].sec_counter; j++)
                           {
                              sec_nfr += (double)afd_istat[position].hour[j].nfr;
                              sec_nbr +=         afd_istat[position].hour[j].nbr;
                           }
                           display_data(sec_nfr, sec_nbr);
                           nfr += sec_nfr; nbr += sec_nbr;
                           for (j = (afd_istat[position].hour_counter + 1);
                                j < HOURS_PER_DAY; j++)
                           {
                              (void)fprintf(stdout, "%12d:", j);
                              nfr += (double)afd_istat[position].day[j].nfr;
                              nbr +=         afd_istat[position].day[j].nbr;
                              display_data(afd_istat[position].day[j].nfr,
                                           afd_istat[position].day[j].nbr);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           if (show_hour < HOURS_PER_DAY)
                           {
                              if (afd_istat[position].hour_counter < show_hour)
                              {
                                 j = HOURS_PER_DAY -
                                     (show_hour - afd_istat[position].hour_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].hour_counter -
                                     show_hour;
                              }
                              nfr += (double)afd_istat[position].day[j].nfr;
                              nbr +=         afd_istat[position].day[j].nbr;
                              display_data(afd_istat[position].day[j].nfr,
                                           afd_istat[position].day[j].nbr);
                           }
                           else
                           {
                              display_data(0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                       =================================\n");
               (void)fprintf(stdout, "=====================> AFD INPUT STATISTICS HOUR SUMMARY <======================\n");
               (void)fprintf(stdout, "                       =================================\n");
               for (j = 0; j < afd_istat[0].hour_counter; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
               if (afd_istat[0].hour_counter == 0)
               {
                  (void)fprintf(stdout, "* %2d:", afd_istat[0].hour_counter);
               }
               else
               {
                  (void)fprintf(stdout, "        * %2d:",
                                afd_istat[0].hour_counter);
               }
               nfr = nbr = 0.0;
               for (i = 0; i < no_of_dirs; i++)
               {
                  for (j = 0; j < afd_istat[i].sec_counter; j++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
               }
               display_data(nfr, nbr);
               tmp_nfr += nfr; tmp_nbr += nbr;
               for (j = (afd_istat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].day[j].nfr;
                     nbr +=         afd_istat[i].day[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_hour_summary > -1) */

            /*
             * Show data for one or all minutes for this hour.
             */
            if (show_min > -1)
            {
               int tmp;

               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                          ===========================\n");
               (void)fprintf(stdout, "========================> AFD INPUT STATISTICS MINUTE <=========================\n");
               (void)fprintf(stdout, "                          ===========================\n");
               if (dir_counter < 0)
               {
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr = nbr = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_istat[i].dir_alias);
                     if (show_min == 0) /* Show all minutes of the hour */
                     {
                        for (j = 0; j < afd_istat[i].sec_counter; j++)
                        {
                           tmp = j * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              (void)fprintf(stdout, "%7d %4d:", tmp / 60, j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfr += (double)afd_istat[i].hour[j].nfr;
                           nbr +=         afd_istat[i].hour[j].nbr;
                           display_data(afd_istat[i].hour[j].nfr,
                                        afd_istat[i].hour[j].nbr);
                        }
                        tmp = afd_istat[0].sec_counter * STAT_RESCAN_TIME;
                        if ((tmp % 60) == 0)
                        {
                           (void)fprintf(stdout, "%7d*%4d:",
                                         tmp / 60, afd_istat[i].sec_counter);
                        }
                        else
                        {
                           (void)fprintf(stdout, "        *%3d:",
                                         afd_istat[i].sec_counter);
                        }
                        display_data(0.0, 0.0);
                        for (j = (afd_istat[i].sec_counter + 1);
                             j < SECS_PER_HOUR; j++)
                        {
                           tmp = j * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              (void)fprintf(stdout, "%7d %4d:", tmp / 60, j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfr += (double)afd_istat[i].hour[j].nfr;
                           nbr +=         afd_istat[i].hour[j].nbr;
                           display_data(afd_istat[i].hour[j].nfr,
                                        afd_istat[i].hour[j].nbr);
                        }
                     }
                     else /* Show a specific minute */
                     {
                        int sec = (show_min * 60) / STAT_RESCAN_TIME;

                        if (afd_istat[i].sec_counter < sec)
                        {
                           j = SECS_PER_HOUR - (sec - afd_istat[i].sec_counter);
                        }
                        else
                        {
                           j = afd_istat[i].sec_counter - sec;
                        }
                        (void)fprintf(stdout, "     ");
                        nfr += (double)afd_istat[i].hour[j].nfr;
                        nbr +=         afd_istat[i].hour[j].nbr;
                        display_data(afd_istat[i].hour[j].nfr,
                                     afd_istat[i].hour[j].nbr);
                     }
                     tmp_nfr += nfr; tmp_nbr += nbr;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific directory */
               {
                  for (i = 0; i < dir_counter; i++)
                  {
                     if ((position = locate_dir(afd_istat, arglist[i],
                                                no_of_dirs)) < 0)
                     {
                        (void)fprintf(stdout,
                                      "No directory %s found in statistic database.\n",
                                      arglist[i]);
                     }
                     else
                     {
                        nfr = nbr = 0.0;
                        (void)fprintf(stdout, "%-8s",
                                      afd_istat[position].dir_alias);
                        if (show_min == 0) /* Show all minutes of the hour */
                        {
                           for (j = 0; j < afd_istat[position].sec_counter; j++)
                           {
                              tmp = j * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 (void)fprintf(stdout, "%7d %4d:", tmp / 60, j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfr += (double)afd_istat[position].hour[j].nfr;
                              nbr +=         afd_istat[position].hour[j].nbr;
                              display_data(afd_istat[position].hour[j].nfr,
                                           afd_istat[position].hour[j].nbr);
                           }
                           tmp = afd_istat[position].sec_counter * STAT_RESCAN_TIME;
                           if ((tmp % 60) == 0)
                           {
                              (void)fprintf(stdout, "%7d*%4d:",
                                            tmp / 60,
                                            afd_istat[position].sec_counter);
                           }
                           else
                           {
                              (void)fprintf(stdout, "        *%3d:",
                                            afd_istat[position].sec_counter);
                           }
                           display_data(0.0, 0.0);
                           for (j = (afd_istat[position].sec_counter + 1);
                                j < SECS_PER_HOUR; j++)
                           {
                              tmp = j * STAT_RESCAN_TIME;
                              if ((tmp % 60) == 0)
                              {
                                 (void)fprintf(stdout, "%7d %4d:", tmp / 60, j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfr += (double)afd_istat[position].hour[j].nfr;
                              nbr +=         afd_istat[position].hour[j].nbr;
                              display_data(afd_istat[position].hour[j].nfr,
                                           afd_istat[position].hour[j].nbr);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           if (show_min < 60)
                           {
                              int sec = (show_min * 60) / STAT_RESCAN_TIME;

                              if (afd_istat[position].sec_counter < sec)
                              {
                                 j = SECS_PER_HOUR -
                                     (sec - afd_istat[position].sec_counter);
                              }
                              else
                              {
                                 j = afd_istat[position].sec_counter - sec;
                              }
                              nfr += (double)afd_istat[position].hour[j].nfr;
                              nbr +=         afd_istat[position].hour[j].nbr;
                              display_data(afd_istat[position].hour[j].nfr,
                                           afd_istat[position].hour[j].nbr);
                           }
                           else
                           {
                              display_data(0.0, 0.0);
                           }
                        }
                        tmp_nfr += nfr; tmp_nbr += nbr;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) || (show_hour) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            } /* if (show_min > -1) */

            /*
             * Show summary on a per minute basis for the last hour.
             */
            if (show_min_summary > -1)
            {
               tmp_nfr = tmp_nbr = 0.0;
               (void)fprintf(stdout, "                      ===================================\n");
               (void)fprintf(stdout, "====================> AFD INPUT STATISTICS MINUTE SUMMARY <=====================\n");
               (void)fprintf(stdout, "                      ===================================\n");
            }
            if (show_min_summary == 0)
            {
               int tmp;

               for (j = 0; j < afd_istat[0].sec_counter; j++)
               {
                  tmp = j * STAT_RESCAN_TIME;
                  if ((tmp % 60) == 0)
                  {
                     (void)fprintf(stdout, "%8d %3d:", tmp / 60, j);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%12d:", j);
                  }
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
               tmp = afd_istat[0].sec_counter * STAT_RESCAN_TIME;
               if ((tmp % 60) == 0)
               {
                  (void)fprintf(stdout, "%8d*%3d:",
                                tmp / 60, afd_istat[0].sec_counter);
               }
               else
               {
                  (void)fprintf(stdout, "        *%3d:",
                                afd_istat[0].sec_counter);
               }
               display_data(0.0, 0.0);
               for (j = (afd_istat[0].sec_counter + 1); j < SECS_PER_HOUR; j++)
               {
                  tmp = j * STAT_RESCAN_TIME;
                  if ((tmp % 60) == 0)
                  {
                     (void)fprintf(stdout, "%8d %3d:", tmp / 60, j);
                  }
                  else
                  {
                     (void)fprintf(stdout, "%12d:", j);
                  }
                  nfr = nbr = 0.0;
                  for (i = 0; i < no_of_dirs; i++)
                  {
                     nfr += (double)afd_istat[i].hour[j].nfr;
                     nbr +=         afd_istat[i].hour[j].nbr;
                  }
                  display_data(nfr, nbr);
                  tmp_nfr += nfr; tmp_nbr += nbr;
               }
            }
            else if (show_min_summary > 0)
                 {
                    int left,
                        sec_ints = (show_min_summary * 60) / STAT_RESCAN_TIME,
                        tmp;

                    left = afd_istat[0].sec_counter - sec_ints;
                    if (left < 0)
                    {
                       for (j = (SECS_PER_HOUR + left); j < SECS_PER_HOUR; j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             (void)fprintf(stdout, "%8d %3d:", tmp / 60, j);
                          }
                          else
                          {
                             (void)fprintf(stdout, "%12d:", j);
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                       for (j = 0; j < (sec_ints + left); j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             (void)fprintf(stdout, "%8d %3d:", tmp / 60, j);
                          }
                          else
                          {
                             (void)fprintf(stdout, "%12d:", j);
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                    }
                    else
                    {
                       for (j = left; j < afd_istat[0].sec_counter; j++)
                       {
                          tmp = j * STAT_RESCAN_TIME;
                          if ((tmp % 60) == 0)
                          {
                             (void)fprintf(stdout, "%8d %3d:", tmp / 60, j);
                          }
                          else
                          {
                             (void)fprintf(stdout, "%12d:", j);
                          }
                          nfr = nbr = 0.0;
                          for (i = 0; i < no_of_dirs; i++)
                          {
                             nfr += (double)afd_istat[i].hour[j].nfr;
                             nbr +=         afd_istat[i].hour[j].nbr;
                          }
                          display_data(nfr, nbr);
                          tmp_nfr += nfr; tmp_nbr += nbr;
                       }
                    }
                 }

            if (show_min_summary > -1)
            {
               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfr, tmp_nbr);
               }
               else
               {
                  total_nfr += tmp_nfr; total_nbr += tmp_nbr;
               }
               (void)fprintf(stdout, "================================================================================\n");
            }

            (void)fprintf(stdout, "Total        ");
            display_data(total_nfr, total_nbr);
         }

#ifdef HAVE_MMAP
         if (munmap(ptr, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#else
         free(ptr);
#endif /* HAVE_MMAP */
      }

      if (close(stat_fd) == -1)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not close() file %s : %s (%s %d)\n",
                       statistic_file, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)fprintf(stderr, "ERROR   : No data in %s (%s %d)\n",
                    statistic_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(double nfr, double nbr)
{
   (void)fprintf(stdout, "%12.0f ", nfr);
   if (nbr >= F_TERABYTE)
   {
      (void)fprintf(stdout, "%8.3f TB\n", nbr / F_TERABYTE);
   }
   else if (nbr >= F_GIGABYTE)
        {
           (void)fprintf(stdout, "%8.3f GB\n", nbr / F_GIGABYTE);
        }
   else if (nbr >= F_MEGABYTE)
        {
           (void)fprintf(stdout, "%8.3f MB\n", nbr / F_MEGABYTE);
        }
   else if (nbr >= F_KILOBYTE)
        {
           (void)fprintf(stdout, "%8.3f KB\n", nbr / F_KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "%8.0f B\n", nbr);
        }

   return;
}
