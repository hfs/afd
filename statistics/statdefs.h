/*
 *  statdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __statdefs_h
#define __statdefs_h

#define STAT_RESCAN_TIME                              5
#define DAYS_PER_YEAR                               366
#define HOURS_PER_DAY                                24
#define SECS_PER_HOUR         (3600 / STAT_RESCAN_TIME)
#define MAX_FILES_PER_SCAN      (STAT_RESCAN_TIME * 10) /* just assuming */
/**********************************************/
/* Assuming 8 Byte for double and 4 Bytes for */
/* integer we get the following size for one  */
/* statistic entry per host :                 */
/* ------------------------------------------ */
/* STAT_RESCAN_TIME    size required          */
/*       10         => 182100 (145680) Bytes  */
/*       20         =>  95700 ( 76560) Bytes  */
/*       30         =>  66900 ( 53520) Bytes  */
/*       40         =>  52500 ( 42000) Bytes  */
/*       50         =>  43860 ( 35088) Bytes  */
/*       60         =>  38100 ( 30480) Bytes  */
/* (in brackets without errors/connections)   */
/**********************************************/

#define STATISTIC_FILE         "/afd_statistic_file"
#define NEW_STATISTIC_FILE     "/.afd_statistic_file.NEW"

struct statistics
       {
          unsigned int nfs;      /* Number of files send  */
          double       nbs;      /* Number of bytes send  */
          unsigned int ne;       /* Number of errors      */
          unsigned int nc;       /* Number of connections */
       };

struct afdstat
       {
          char              hostname[MAX_HOSTNAME_LENGTH + 1];
          time_t            start_time;                 /* Time when acc. for */
                                                        /* this host starts.  */
          int               day_counter;                /* Position in year.  */
          struct statistics year[DAYS_PER_YEAR];        /* Per day.           */
          int               hour_counter;               /* Position in day.   */
          struct statistics day[HOURS_PER_DAY];         /* Per hour.          */
          int               sec_counter;                /* Position in hour.  */
          struct statistics hour[SECS_PER_HOUR];        /* Per                */
                                                        /* STAT_RESCAN_TIME   */
                                                        /* seconds.           */
          unsigned int      prev_nfs;
          double            prev_nbs;
          unsigned int      prev_ne;
          unsigned int      prev_nc;
       };
struct afd_year_stat
       {
          char              hostname[MAX_HOSTNAME_LENGTH + 1];
          time_t            start_time;                 /* Time when acc. for */
                                                        /* this host starts.  */
          struct statistics year[DAYS_PER_YEAR];
       };

extern void eval_input_as(int, char **, char *, char *),
            eval_input_ss(int, char **, char *, char *, int *,
                          int *, int *, int *, int *, int *, int *),
            read_afd_stat_db(int),
            save_old_year(int);
extern int  locate_host(struct afdstat *, char *, int),
            locate_host_year(struct afd_year_stat *, char *, int);

#endif /* __statdefs_h */
