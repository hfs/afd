/*
 *  show_bench_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_bench_stat - shows statistic information of one or more AFD's
 **
 ** SYNOPSIS
 **   show_bench_stat <common dir> <interval> <loops> <sub dir 1>...<sub dir n>
 **               --version       Show version.
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.10.1999 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror(), strcmp()    */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>                 /* sleep(), exit(), close(),         */
                                    /* STDERR_FILENO                     */
#include <stdlib.h>                 /* free(), malloc()                  */
#include <sys/mman.h>               /* mmap(), munmap()                  */
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
int            loops,
               loops_to_do,
               no_of_afds,
               *no_of_hosts,
               sys_log_fd = STDERR_FILENO;
unsigned int   interval_time;
double         nfs = 0.0, nbs = 0.0, nc = 0.0, ne = 0.0, fps, bps,
               tmp_nfs = 0.0, tmp_nbs = 0.0, tmp_nc = 0.0, tmp_ne = 0.0;
char           **p_afd_stat;
struct afdstat *afd_stat;

/* Some local definitions */
#define KILOBYTE                       1024.0
#define MEGABYTE                       1048576.0
#define GIGABYTE                       1073741824.0

/* Function prototypes */
static void display_data(double, double, double, double, double, double),
       summary(int),
       timeout(int, void (*func)(int)),
       usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int i;
   int          stat_fd;
   time_t       now;
   char         **afd_dir,
                **statistic_file;
   struct stat  stat_buf;
   struct tm    *p_ts;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   if (argc < 5)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   interval_time = atoi(argv[2]);
   loops_to_do = atoi(argv[3]);
   no_of_afds = argc - 4;
   RT_ARRAY(afd_dir, no_of_afds, MAX_PATH_LENGTH, char);
   RT_ARRAY(statistic_file, no_of_afds, MAX_PATH_LENGTH, char);
   if ((p_afd_stat = malloc((no_of_afds * sizeof(char *)))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((no_of_hosts = malloc((no_of_afds * sizeof(int)))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   i = 4;
   do
   {
      (void)sprintf(afd_dir[i - 4], "%s/%s", argv[1], argv[i]);
      i++;
   } while (i != argc);

   /* Initialize variables */
   (void)time(&now);
   p_ts = gmtime(&now);
   for (i = 0; i < no_of_afds; i++)
   {
      (void)sprintf(statistic_file[i], "%s%s%s.%d",
                    afd_dir[i], LOG_DIR, STATISTIC_FILE,
                    p_ts->tm_year + 1900);

      do
      {
         errno = 0;
         while ((stat(statistic_file[i], &stat_buf) != 0) &&
                (errno == ENOENT))
         {
            my_usleep(100000L);
         }
         if ((errno != 0) && (errno != ENOENT))
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to stat() %s : %s (%s %d)\n",
                          statistic_file[i], strerror(errno),
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (stat_buf.st_size == 0)
         {
            my_usleep(100000L);
         }
      } while (stat_buf.st_size == 0);

      /* Open file */
      if ((stat_fd = open(statistic_file[i], O_RDONLY)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to open() %s : %s (%s %d)\n",
                       statistic_file[i], strerror(errno),
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if ((p_afd_stat[i] = mmap(0, stat_buf.st_size,
                                PROT_READ, (MAP_FILE | MAP_SHARED),
                                stat_fd, 0)) == (void *) -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not mmap() file %s : %s (%s %d)\n",
                       statistic_file[i], strerror(errno),
                       __FILE__, __LINE__);
         (void)close(stat_fd);
         exit(INCORRECT);
      }
      no_of_hosts[i] = stat_buf.st_size / sizeof(struct afdstat);
      (void)close(stat_fd);
   }

   tmp_nfs = tmp_nbs = tmp_nc = tmp_ne = 0.0;
   loops = 0;
   timeout(interval_time, summary);

   do
   {
      (void)sleep(1);
   } while (loops < loops_to_do);

   (void)fprintf(stdout, "---------------------------------------------------------------------------\n");
   (void)fflush(stdout);
   fps = nfs / (double)(interval_time * loops);
   bps = nbs / (double)(interval_time * loops);
   (void)fprintf(stdout, "Total:");
   display_data(nfs, nbs, nc, ne, fps, bps);
   (void)fprintf(stdout, "===========================================================================\n");

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++++ summary() ++++++++++++++++++++++++++++++*/
static void
summary(int dummy)
{
   register int i, j, k;

   /* Make a summary of everything */
   nfs = nbs = nc = ne = 0.0;
   for (k = 0; k < no_of_afds; k++)
   {
      afd_stat = (struct afdstat *)p_afd_stat[k];
      for (i = 0; i < no_of_hosts[k]; i++)
      {
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
      }
   }
   fps = (nfs - tmp_nfs) / (double)interval_time;
   bps = (nbs - tmp_nbs) / (double)interval_time;
   (void)fprintf(stdout, "      ");
   display_data(nfs - tmp_nfs, nbs - tmp_nbs, nc - tmp_nc, ne - tmp_ne,
                fps, bps);
   tmp_nfs = nfs;
   tmp_nbs = nbs;
   tmp_nc = nc;
   tmp_ne = ne;

   loops++;
   if (loops < loops_to_do)
   {
      timeout(interval_time, summary);
   }

   return;
}


/*+++++++++++++++++++++++++++++ timeout() +++++++++++++++++++++++++++++++*/
static void
timeout(int sec, void (*func)(int))
{
   (void)signal((char)SIGALRM, func);
   (void)alarm(sec);

   return;
}


/*++++++++++++++++++++++++++++ display_data() +++++++++++++++++++++++++++*/
static void
display_data(double nfs, double nbs, double nc, double ne,
             double fps, double bps)
{
   (void)fprintf(stdout, "%12.0f   ", nfs);
   if (nbs >= GIGABYTE)
   {
      (void)fprintf(stdout, "%6.2f GB", nbs / GIGABYTE);
   }
   else if (nbs >= MEGABYTE)
        {
           (void)fprintf(stdout, "%6.2f MB", nbs / MEGABYTE);
        }
   else if (nbs >= KILOBYTE)
        {
           (void)fprintf(stdout, "%6.2f KB", nbs / KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "%6.0f B ", nbs);
        }
   (void)fprintf(stdout, "%8.0f", nc);
   (void)fprintf(stdout, "%6.0f", ne);
   if (bps >= GIGABYTE)
   {
      (void)fprintf(stdout, "  => %8.2f fps %8.2f GB/s\n",
                    fps, bps / GIGABYTE);
   }
   else if (bps >= MEGABYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f MB/s\n",
                         fps, bps / MEGABYTE);
        }
   else if (bps >= KILOBYTE)
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f KB/s\n",
                         fps, bps / KILOBYTE);
        }
        else
        {
           (void)fprintf(stdout, "  => %8.2f fps %8.2f  B/s\n", fps, bps);
        }
   (void)fflush(stdout);

   return;
}


/*++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s <common dir> <interval> <loops> <sub dir 1>...<sub dir n>\n",
                 progname);
   return;
}
