/*
 *  show_stat.c - Part of AFD, an automatic file distribution program.
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
 **   show_stat - shows all statistic information of the AFD
 **
 ** SYNOPSIS
 **   show_stat [options] [hostname_1 hostname_2 .... hostname_n]
 **               -w <work dir>   Working directory of the AFD.
 **               -f <name>       Path and name of the statistics file.
 **               -d [<x>]        Show information of all days [or day
 **                               minus x].
 **               -D              Show total summary on a per day basis.
 **               -h [<x>]        Show information of all hours [or hour
 **                               minus x].
 **               -H              Show total summary of last 24 hours.
 **               -y [<x>]        Show information of all years [or year
 **                               minus x].
 **               --version       Show version.
 **
 ** DESCRIPTION
 **   This program shows all statistic information of the number of
 **   files transfered, the number of bytes transfered, the number
 **   of connections and the number of errors that occured for each
 **   host and a total for all hosts, depending on the options
 **   that where used when calling 'show_stat'.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.09.1996 H.Kiehl Created
 **   19.04.1998 H.Kiehl Added -D option.
 **   26.05.1998 H.Kiehl Added -H option.
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror(), strcmp()    */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* getcwd(), exit(), close(),        */
                                    /* STDERR_FILENO                     */
#include <stdlib.h>                 /* getenv(), free(), malloc()        */
#ifndef _NO_MMAP
#include <sys/mman.h>               /* mmap(), munmap()                  */
#endif
#include <fcntl.h>
#include <errno.h>
#include "statdefs.h"
#include "version.h"

#ifndef _NO_MMAP
#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif
#endif

/* Global variables */
int  sys_log_fd = STDERR_FILENO; /* Used by get_afd_path() */
char *p_work_dir,
     **hosts;

/* Some local definitions */
#define KILOBYTE                       1024.0
#define MEGABYTE                       1048576.0
#define GIGABYTE                       1073741824.0

/* Function prototypes */
static void display_data(double, double, double, double);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int   i,
                  j;
   int            position,
                  show_hour = -1,
                  show_hour_summary = -1,
                  show_day = -1,
                  show_day_summary = -1,
                  show_year = -1,
                  show_old_year = NO,
                  no_of_hosts,
                  host_counter = -1,
                  stat_fd,
                  year;
   double         nfs = 0.0, nbs = 0.0, nc = 0.0, ne = 0.0,
                  tmp_nfs = 0.0, tmp_nbs = 0.0, tmp_nc = 0.0, tmp_ne = 0.0,
                  total_nfs = 0.0, total_nbs = 0.0, total_nc = 0.0, total_ne = 0.0;
   char           work_dir[MAX_PATH_LENGTH],
                  statistic_file_name[MAX_FILENAME_LENGTH],
                  statistic_file[MAX_PATH_LENGTH];
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   (void)strcpy(statistic_file_name, STATISTIC_FILE);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_ss(argc, argv, work_dir, statistic_file_name,
                 &show_day, &show_day_summary, &show_hour,
                 &show_hour_summary, &show_year, &host_counter);

   /* Initialize variables */
   p_work_dir = work_dir;
   if (strcmp(statistic_file_name, STATISTIC_FILE) == 0)
   {
      int       current_year;
      time_t    now;
      char      str_year[10];
      struct tm *p_ts;

      (void)time(&now);
      p_ts = gmtime(&now);
      current_year = p_ts->tm_year + 1900;
      if (show_day > 0)
      {
         now -= (86400 * show_day);
      }
      else if (show_hour > 0)
           {
              now -= (3600 * show_hour);
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
      (void)sprintf(str_year, ".%d", year);
      (void)strcpy(statistic_file, work_dir);
      (void)strcat(statistic_file, LOG_DIR);
      (void)strcat(statistic_file, statistic_file_name);
      (void)strcat(statistic_file, str_year);
   }
   else
   {
      (void)strcpy(statistic_file, statistic_file_name);

      /* We cannot know from what year this file is, so set to zero. */
      year = 0;
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
         (void)fprintf(stderr,
                       "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file, strerror(errno),
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (show_old_year == YES)
      {
         struct afd_year_stat *afd_stat;
         
#ifdef _NO_MMAP
         if ((afd_stat = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, afd_stat, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno),
                          __FILE__, __LINE__);
            free(afd_stat);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((afd_stat = (struct afd_year_stat *)mmap(0, stat_buf.st_size,
                                                PROT_READ,
                                                (MAP_FILE | MAP_SHARED),
                                                stat_fd, 0)) == (struct afd_year_stat *) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* _NO_MMAP */

         no_of_hosts = stat_buf.st_size / sizeof(struct afd_year_stat);
         if (show_year != -1)
         {
            /*
             * Show total for all host.
             */
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;

            (void)fprintf(stdout, "                     =============================\n");
            (void)fprintf(stdout, "====================> AFD STATISTICS SUMMARY %d <===================\n", year);
            (void)fprintf(stdout, "                     =============================\n");

            if (host_counter > 0)
            {
               for (i = 0; i < host_counter; i++)
               {
                  if ((position = locate_host_year(afd_stat, hosts[i],
                                                   no_of_hosts)) < 0)
                  {
                     (void)fprintf(stdout, "No host %s found in statistic database.\n", hosts[i]);
                  }
                  else
                  {
                     nfs = nbs = nc = ne = 0.0;
                     (void)fprintf(stdout, "%-12s", afd_stat[position].hostname);
                     for (j = 0; j < DAYS_PER_YEAR; j++)
                     {
                        nfs += (double)afd_stat[position].year[j].nfs;
                        nbs +=         afd_stat[position].year[j].nbs;
                        nc  += (double)afd_stat[position].year[j].nc;
                        ne  += (double)afd_stat[position].year[j].ne;
                     }
                     display_data(nfs, nbs, nc, ne);
                     tmp_nfs += nfs;
                     tmp_nbs += nbs;
                     tmp_nc += nc;
                     tmp_ne += ne;
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_hosts; i++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  (void)fprintf(stdout, "%-12s", afd_stat[i].hostname);
                  for (j = 0; j < DAYS_PER_YEAR; j++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);
                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc += nc;
                  tmp_ne += ne;
               }
            }

            (void)fprintf(stdout, "----------------------------------------------------------------------\n");
            (void)fprintf(stdout, "Total       ");
            display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
            (void)fprintf(stdout, "======================================================================\n");
         }
         else
         {
            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                        ====================\n");
               (void)fprintf(stdout, "=======================> AFD STATISTICS DAY <==========================\n");
               (void)fprintf(stdout, "                        ====================\n");
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_stat[i].hostname);
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
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           display_data(afd_stat[i].year[j].nfs,
                                        afd_stat[i].year[j].nbs,
                                        afd_stat[i].year[j].nc,
                                        afd_stat[i].year[j].ne);
                        }
                     }
                     else /* Show a specific day */
                     {
                        (void)fprintf(stdout, "     ");
                        nfs += (double)afd_stat[i].year[show_day].nfs;
                        nbs +=         afd_stat[i].year[show_day].nbs;
                        nc  += (double)afd_stat[i].year[show_day].nc;
                        ne  += (double)afd_stat[i].year[show_day].ne;
                        display_data(afd_stat[i].year[show_day].nfs,
                                     afd_stat[i].year[show_day].nbs,
                                     afd_stat[i].year[show_day].nc,
                                     afd_stat[i].year[show_day].ne);
                     }

                     tmp_nfs += nfs;
                     tmp_nbs += nbs;
                     tmp_nc  += nc;
                     tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host_year(afd_stat, hosts[i],
                                                      no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout, "No host %s found in statistic database.\n", hosts[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        (void)fprintf(stdout, "%-8s", afd_stat[position].hostname);
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
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              display_data(afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           nfs += (double)afd_stat[position].year[show_day].nfs;
                           nbs +=         afd_stat[position].year[show_day].nbs;
                           nc  += (double)afd_stat[position].year[show_day].nc;
                           ne  += (double)afd_stat[position].year[show_day].ne;
                           display_data(afd_stat[position].year[show_day].nfs,
                                        afd_stat[position].year[show_day].nbs,
                                        afd_stat[position].year[show_day].nc,
                                        afd_stat[position].year[show_day].ne);
                        }

                        tmp_nfs += nfs;
                        tmp_nbs += nbs;
                        tmp_nc  += nc;
                        tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               time_t    now;
               struct tm *p_ts;

               (void)time(&now);
               p_ts = gmtime(&now);
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                     ==========================\n");
               (void)fprintf(stdout, "===================> AFD STATISTICS DAY SUMMARY <======================\n");
               (void)fprintf(stdout, "                     ==========================\n");
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_day_summary > -1) */

            (void)fprintf(stdout, "Total        ");
            display_data(total_nfs, total_nbs, total_nc, total_ne);
         }

#ifdef _NO_MMAP
         free((void *)afd_stat);
#else
         if (munmap((void *)afd_stat, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
#endif /* _NO_MMAP */
      }
      else /* Show data of current year */
      {
         struct afdstat *afd_stat;

#ifdef _NO_MMAP
         if ((afd_stat = malloc(stat_buf.st_size)) == NULL)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to allocate memory : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }

         if (read(stat_fd, afd_stat, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to read() %s : %s (%s %d)\n",
                          statistic_file, strerror(errno),
                          __FILE__, __LINE__);
            free(afd_stat);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#else
         if ((afd_stat = (struct afdstat *)mmap(0, stat_buf.st_size,
                                                PROT_READ,
                                                (MAP_FILE | MAP_SHARED),
                                                stat_fd, 0)) == (struct afdstat *) -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(stat_fd);
            exit(INCORRECT);
         }
#endif /* _NO_MMAP */

         if ((show_day == -1) && (show_year == -1) && (show_hour == -1) &&
             (show_hour_summary == -1) && (show_day_summary == -1))
         {
            /*
             * Show total for all host.
             */
            no_of_hosts = stat_buf.st_size / sizeof(struct afdstat);
            tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;

            (void)fprintf(stdout, "                       ========================\n");
            (void)fprintf(stdout, "======================> AFD STATISTICS SUMMARY <======================\n");
            (void)fprintf(stdout, "                       ========================\n");

            if (host_counter > 0)
            {
               for (i = 0; i < host_counter; i++)
               {
                  if ((position = locate_host(afd_stat, hosts[i], no_of_hosts)) < 0)
                  {
                     (void)fprintf(stdout, "No host %s found in statistic database.\n", hosts[i]);
                  }
                  else
                  {
                     nfs = nbs = nc = ne = 0.0;
                     (void)fprintf(stdout, "%-12s", afd_stat[position].hostname);
                     for (j = 0; j < afd_stat[position].sec_counter; j++)
                     {
                        nfs += (double)afd_stat[position].hour[j].nfs;
                        nbs +=         afd_stat[position].hour[j].nbs;
                        nc  += (double)afd_stat[position].hour[j].nc;
                        ne  += (double)afd_stat[position].hour[j].ne;
                     }
                     for (j = 0; j < afd_stat[position].hour_counter; j++)
                     {
                        nfs += (double)afd_stat[position].day[j].nfs;
                        nbs +=         afd_stat[position].day[j].nbs;
                        nc  += (double)afd_stat[position].day[j].nc;
                        ne  += (double)afd_stat[position].day[j].ne;
                     }
                     for (j = 0; j < afd_stat[position].day_counter; j++)
                     {
                        nfs += (double)afd_stat[position].year[j].nfs;
                        nbs +=         afd_stat[position].year[j].nbs;
                        nc  += (double)afd_stat[position].year[j].nc;
                        ne  += (double)afd_stat[position].year[j].ne;
                     }
                     display_data(nfs, nbs, nc, ne);
                     tmp_nfs += nfs;
                     tmp_nbs += nbs;
                     tmp_nc += nc;
                     tmp_ne += ne;
                  }
               }
            }
            else
            {
               /* Make a summary of everything */
               for (i = 0; i < no_of_hosts; i++)
               {
                  nfs = nbs = nc = ne = 0.0;
                  (void)fprintf(stdout, "%-12s", afd_stat[i].hostname);
                  for (j = 0; j < afd_stat[i].sec_counter; j++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
                  for (j = 0; j < afd_stat[i].hour_counter; j++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  for (j = 0; j < afd_stat[i].day_counter; j++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);
                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc += nc;
                  tmp_ne += ne;
               }
            }

            (void)fprintf(stdout, "----------------------------------------------------------------------\n");
            (void)fprintf(stdout, "Total       ");
            display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
            (void)fprintf(stdout, "======================================================================\n");
         }
         else
         {
            no_of_hosts = stat_buf.st_size / sizeof(struct afdstat);

            /*
             * Show data for one or all days for this year.
             */
            if (show_day > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                        ====================\n");
               (void)fprintf(stdout, "=======================> AFD STATISTICS DAY <==========================\n");
               (void)fprintf(stdout, "                        ====================\n");
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_stat[i].hostname);
                     if (show_day == 0) /* Show all days */
                     {
                        for (j = 0; j < afd_stat[i].day_counter; j++)
                        {
                           if (j == 0)
                           {
                              (void)fprintf(stdout, "%4d:", j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           display_data(afd_stat[i].year[j].nfs,
                                        afd_stat[i].year[j].nbs,
                                        afd_stat[i].year[j].nc,
                                        afd_stat[i].year[j].ne);
                        }
                        if (afd_stat[i].day_counter == 0)
                        {
                           (void)fprintf(stdout, "%4d:", 0);
                           display_data(0, 0.0, 0, 0);
                        }
                     }
                     else /* Show a specific day */
                     {
                        (void)fprintf(stdout, "     ");
                        if (afd_stat[i].day_counter < show_day)
                        {
                           display_data(0, 0.0, 0, 0);
                        }
                        else
                        {
                           j = afd_stat[i].day_counter - show_day;
                           nfs += (double)afd_stat[i].year[j].nfs;
                           nbs +=         afd_stat[i].year[j].nbs;
                           nc  += (double)afd_stat[i].year[j].nc;
                           ne  += (double)afd_stat[i].year[j].ne;
                           display_data(afd_stat[i].year[j].nfs,
                                        afd_stat[i].year[j].nbs,
                                        afd_stat[i].year[j].nc,
                                        afd_stat[i].year[j].ne);
                        }
                     }

                     tmp_nfs += nfs;
                     tmp_nbs += nbs;
                     tmp_nc  += nc;
                     tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host(afd_stat, hosts[i], no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout, "No host %s found in statistic database.\n", hosts[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        (void)fprintf(stdout, "%-8s", afd_stat[position].hostname);
                        if (show_day == 0) /* Show all days */
                        {
                           for (j = 0; j < afd_stat[position].day_counter; j++)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              display_data(afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           if (afd_stat[i].day_counter < show_day)
                           {
                              display_data(0, 0.0, 0, 0);
                           }
                           else
                           {
                              j = afd_stat[i].day_counter - show_year;
                              nfs += (double)afd_stat[position].year[j].nfs;
                              nbs +=         afd_stat[position].year[j].nbs;
                              nc  += (double)afd_stat[position].year[j].nc;
                              ne  += (double)afd_stat[position].year[j].ne;
                              display_data(afd_stat[position].year[j].nfs,
                                           afd_stat[position].year[j].nbs,
                                           afd_stat[position].year[j].nc,
                                           afd_stat[position].year[j].ne);
                           }
                        }

                        tmp_nfs += nfs;
                        tmp_nbs += nbs;
                        tmp_nc  += nc;
                        tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_hour > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_day > -1) */

            /*
             * Show total summary on a per day basis for this year.
             */
            if (show_day_summary > -1)
            {
               time_t    now;
               struct tm *p_ts;

               (void)time(&now);
               p_ts = gmtime(&now);
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                     ==========================\n");
               (void)fprintf(stdout, "===================> AFD STATISTICS DAY SUMMARY <======================\n");
               (void)fprintf(stdout, "                     ==========================\n");
               for (j = 0; j < p_ts->tm_yday; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].year[j].nfs;
                     nbs +=         afd_stat[i].year[j].nbs;
                     nc  += (double)afd_stat[i].year[j].nc;
                     ne  += (double)afd_stat[i].year[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_hour > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_day_summary > -1) */

            /*
             * Show data for one or all hours for this day.
             */
            if (show_hour > -1)
            {
               double sec_nfs, sec_nbs, sec_nc, sec_ne;

               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                        =====================\n");
               (void)fprintf(stdout, "=======================> AFD STATISTICS HOUR <=========================\n");
               (void)fprintf(stdout, "                        =====================\n");
               if (host_counter < 0)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs = nbs = nc = ne = 0.0;
                     (void)fprintf(stdout, "%-8s", afd_stat[i].hostname);
                     if (show_hour == 0) /* Show all hours of the day */
                     {
                        for (j = 0; j < afd_stat[i].hour_counter; j++)
                        {
                           if (j == 0)
                           {
                              (void)fprintf(stdout, "%4d:", j);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%12d:", j);
                           }
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           display_data(afd_stat[i].day[j].nfs,
                                        afd_stat[i].day[j].nbs,
                                        afd_stat[i].day[j].nc,
                                        afd_stat[i].day[j].ne);
                        }
                        if (afd_stat[i].hour_counter == 0)
                        {
                           (void)fprintf(stdout, "* %2d:", afd_stat[i].hour_counter);
                        }
                        else
                        {
                           (void)fprintf(stdout, "        * %2d:", afd_stat[i].hour_counter);
                        }
                        sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                        for (j = 0; j < afd_stat[i].sec_counter; j++)
                        {
                           sec_nfs += (double)afd_stat[i].hour[j].nfs;
                           sec_nbs +=         afd_stat[i].hour[j].nbs;
                           sec_nc  += (double)afd_stat[i].hour[j].nc;
                           sec_ne  += (double)afd_stat[i].hour[j].ne;
                        }
                        display_data(sec_nfs, sec_nbs, sec_nc, sec_ne);
                        nfs += sec_nfs;
                        nbs += sec_nbs;
                        nc  += sec_nc;
                        ne  += sec_ne;
                        for (j = (afd_stat[i].hour_counter + 1); j < HOURS_PER_DAY; j++)
                        {
                           (void)fprintf(stdout, "%12d:", j);
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           display_data(afd_stat[i].day[j].nfs,
                                        afd_stat[i].day[j].nbs,
                                        afd_stat[i].day[j].nc,
                                        afd_stat[i].day[j].ne);
                        }
                     }
                     else /* Show a specific hour */
                     {
                        (void)fprintf(stdout, "     ");
                        if (afd_stat[i].hour_counter < show_hour)
                        {
                           display_data(0, 0.0, 0, 0);
                        }
                        else
                        {
                           j = afd_stat[i].hour_counter - show_hour;
                           nfs += (double)afd_stat[i].day[j].nfs;
                           nbs +=         afd_stat[i].day[j].nbs;
                           nc  += (double)afd_stat[i].day[j].nc;
                           ne  += (double)afd_stat[i].day[j].ne;
                           display_data(afd_stat[i].day[j].nfs,
                                        afd_stat[i].day[j].nbs,
                                        afd_stat[i].day[j].nc,
                                        afd_stat[i].day[j].ne);
                        }
                     }
                     tmp_nfs += nfs;
                     tmp_nbs += nbs;
                     tmp_nc  += nc;
                     tmp_ne  += ne;
                  }
               } /* if (host_counter < 0) */
               else /* Show some specific hosts */
               {
                  for (i = 0; i < host_counter; i++)
                  {
                     if ((position = locate_host(afd_stat, hosts[i], no_of_hosts)) < 0)
                     {
                        (void)fprintf(stdout, "No host %s found in statistic database.\n", hosts[i]);
                     }
                     else
                     {
                        nfs = nbs = nc = ne = 0.0;
                        (void)fprintf(stdout, "%-8s", afd_stat[position].hostname);
                        if (show_hour == 0) /* Show all hours of the day */
                        {
                           for (j = 0; j < afd_stat[position].hour_counter; j++)
                           {
                              if (j == 0)
                              {
                                 (void)fprintf(stdout, "%4d:", j);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%12d:", j);
                              }
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              display_data(afd_stat[position].day[j].nfs,
                                           afd_stat[position].day[j].nbs,
                                           afd_stat[position].day[j].nc,
                                           afd_stat[position].day[j].ne);
                           }
                           if (afd_stat[position].hour_counter == 0)
                           {
                              (void)fprintf(stdout, "* %2d:", afd_stat[position].hour_counter);
                           }
                           else
                           {
                              (void)fprintf(stdout, "        * %2d:", afd_stat[position].hour_counter);
                           }
                           sec_nfs = sec_nbs = sec_nc = sec_ne = 0.0;
                           for (j = 0; j < afd_stat[position].sec_counter; j++)
                           {
                              sec_nfs += (double)afd_stat[position].hour[j].nfs;
                              sec_nbs +=         afd_stat[position].hour[j].nbs;
                              sec_nc  += (double)afd_stat[position].hour[j].nc;
                              sec_ne  += (double)afd_stat[position].hour[j].ne;
                           }
                           display_data(sec_nfs, sec_nbs, sec_nc, sec_ne);
                           nfs += sec_nfs;
                           nbs += sec_nbs;
                           nc  += sec_nc;
                           ne  += sec_ne;
                           for (j = (afd_stat[position].hour_counter + 1); j < HOURS_PER_DAY; j++)
                           {
                              (void)fprintf(stdout, "%12d:", j);
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              display_data(afd_stat[position].day[j].nfs,
                                           afd_stat[position].day[j].nbs,
                                           afd_stat[position].day[j].nc,
                                           afd_stat[position].day[j].ne);
                           }
                        }
                        else /* Show a specific interval */
                        {
                           (void)fprintf(stdout, "     ");
                           if (afd_stat[i].day_counter < show_day)
                           {
                              display_data(0, 0.0, 0, 0);
                           }
                           else
                           {
                              j = afd_stat[i].hour_counter - show_hour;
                              nfs += (double)afd_stat[position].day[j].nfs;
                              nbs +=         afd_stat[position].day[j].nbs;
                              nc  += (double)afd_stat[position].day[j].nc;
                              ne  += (double)afd_stat[position].day[j].ne;
                              display_data(afd_stat[position].day[j].nfs,
                                           afd_stat[position].day[j].nbs,
                                           afd_stat[position].day[j].nc,
                                           afd_stat[position].day[j].ne);
                           }
                        }
                        tmp_nfs += nfs;
                        tmp_nbs += nbs;
                        tmp_nc  += nc;
                        tmp_ne  += ne;
                     }
                  }
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour_summary > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_hour > -1) */

            /*
             * Show total summary on a per hour basis for the last 24 hours.
             */
            if (show_hour_summary > -1)
            {
               tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
               (void)fprintf(stdout, "                     ===========================\n");
               (void)fprintf(stdout, "===================> AFD STATISTICS HOUR SUMMARY <=====================\n");
               (void)fprintf(stdout, "                     ===========================\n");
               for (j = 0; j < afd_stat[0].hour_counter; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }
               if (afd_stat[0].hour_counter == 0)
               {
                  (void)fprintf(stdout, "* %2d:", afd_stat[0].hour_counter);
               }
               else
               {
                  (void)fprintf(stdout, "        * %2d:", afd_stat[0].hour_counter);
               }
               nfs = nbs = nc = ne = 0.0;
               for (i = 0; i < no_of_hosts; i++)
               {
                  for (j = 0; j < afd_stat[i].sec_counter; j++)
                  {
                     nfs += (double)afd_stat[i].hour[j].nfs;
                     nbs +=         afd_stat[i].hour[j].nbs;
                     nc  += (double)afd_stat[i].hour[j].nc;
                     ne  += (double)afd_stat[i].hour[j].ne;
                  }
               }
               display_data(nfs, nbs, nc, ne);
               tmp_nfs += nfs;
               tmp_nbs += nbs;
               tmp_nc  += nc;
               tmp_ne  += ne;
               for (j = (afd_stat[0].hour_counter + 1); j < HOURS_PER_DAY; j++)
               {
                  (void)fprintf(stdout, "%12d:", j);
                  nfs = nbs = nc = ne = 0.0;
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     nfs += (double)afd_stat[i].day[j].nfs;
                     nbs +=         afd_stat[i].day[j].nbs;
                     nc  += (double)afd_stat[i].day[j].nc;
                     ne  += (double)afd_stat[i].day[j].ne;
                  }
                  display_data(nfs, nbs, nc, ne);

                  tmp_nfs += nfs;
                  tmp_nbs += nbs;
                  tmp_nc  += nc;
                  tmp_ne  += ne;
               }

               if ((show_year > -1) || (show_day > -1) ||
                   (show_day_summary > -1) || (show_hour > -1))
               {
                  (void)fprintf(stdout, "Total        ");
                  display_data(tmp_nfs, tmp_nbs, tmp_nc, tmp_ne);
               }
               else
               {
                  total_nfs += tmp_nfs;
                  total_nbs += tmp_nbs;
                  total_nc  += tmp_nc;
                  total_ne  += tmp_ne;
               }
               (void)fprintf(stdout, "=======================================================================\n");
            } /* if (show_hour_summary > -1) */

            (void)fprintf(stdout, "Total        ");
            display_data(total_nfs, total_nbs, total_nc, total_ne);
         }

#ifdef _NO_MMAP
         free((void *)afd_stat);
#else
         if (munmap((void *)afd_stat, stat_buf.st_size) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not munmap() file %s : %s (%s %d)\n",
                          statistic_file, strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
#endif /* _NO_MMAP */
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
display_data(double nfs, double nbs, double nc, double ne)
{
   (void)fprintf(stdout, "%14.0f   ", nfs);
   if (nbs >= GIGABYTE)
   {
      (void)fprintf(stdout, "%12.3f GB", nbs / GIGABYTE);
   }
   else if (nbs >= MEGABYTE)
        {
           (void)fprintf(stdout, "%12.3f MB", nbs / MEGABYTE);
        }
   else if (nbs >= KILOBYTE)
        {
           (void)fprintf(stdout, "%12.3f KB", nbs / KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "%12.0f B ", nbs);
        }
   (void)fprintf(stdout, "%14.0f", nc);
   (void)fprintf(stdout, "%12.0f\n", ne);

   return;
}
