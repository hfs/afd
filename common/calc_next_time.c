/*
 *  calc_next_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999, 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   calc_next_time - function to calculate the next time from a crontab
 **                    like entry
 **
 ** SYNOPSIS
 **   time_t calc_next_time(struct bd_time_entry *te);
 **
 ** DESCRIPTION
 **   The function calc_next_time() calculates from the structure
 **   bd_time_entry te the next time as a time_t value.
 **
 ** RETURN VALUES
 **   The function will return the next time as a time_t value
 **   when successful. On error it will return 0.
 **
 ** BUGS
 **   It does NOT handle the case "* * 31 2 *" (returns March 2nd).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>                 /* exit()                            */
#include <time.h>
#include "bit_array.h"

/* External global variables */
extern int sys_log_fd;

/* Local function prototypes */
static int check_day(struct bd_time_entry *, struct tm *),
           check_month(struct bd_time_entry *, struct tm *),
           get_greatest_dom(int, int);


/*######################### calc_next_time() ############################*/
time_t
calc_next_time(struct bd_time_entry *te)
{
   int       gotcha,
             i;
   time_t    current_time;
   struct tm *bd_time;     /* Broken-down time */

   current_time = time(NULL);
   bd_time = localtime(&current_time);
   bd_time->tm_min++;
   if (bd_time->tm_min == 60)
   {
      bd_time->tm_min = 0;
   }

   if (check_month(te, bd_time) == INCORRECT)
   {
      return(0);
   }

   if (check_day(te, bd_time) == INCORRECT)
   {
      return(0);
   }

   /* Evaluate minute (0-59) [0-59] */
#ifdef _WORKING_LONG_LONG
   if (((ALL_MINUTES & te->minute) == ALL_MINUTES) ||
       ((ALL_MINUTES & te->continuous_minute) == ALL_MINUTES))
   {
      /* Leave minute as is. */;
   }
   else
   {
      gotcha = NO;
      for (i = bd_time->tm_min; i < 60; i++)
      {
         if ((te->minute & bit_array_long[i]) ||
             (te->continuous_minute & bit_array_long[i]))
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_min; i++)
         {
            if ((te->minute & bit_array_long[i]) ||
                (te->continuous_minute & bit_array_long[i]))
            {
               bd_time->tm_hour++;
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to locate any valid minute!? (%s %d)\n",
                   __FILE__, __LINE__);
         return(0);
      }
      bd_time->tm_min = i;
   }
#else
   gotcha = NO;
   for (i = bd_time->tm_min; i < 60; i++)
   {
      if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
      {
         gotcha = YES;
         break;
      }
   }
   if (gotcha == NO)
   {
      for (i = 0; i < bd_time->tm_min; i++)
      {
         if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
         {
            bd_time->tm_hour++;
            gotcha = YES;
            break;
         }
      }
   }
   if (gotcha == NO)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to locate any valid minute!? (%s %d)\n",
                __FILE__, __LINE__);
      return(0);
   }
   bd_time->tm_min = i;
#endif

   /* Evaluate hour (0-23) [0-23] */
   if ((ALL_HOURS & te->hour) != ALL_HOURS)
   {
      gotcha = NO;
      for (i = bd_time->tm_hour; i < 24; i++)
      {
         if (te->hour & bit_array[i])
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_hour; i++)
         {
            if (te->hour & bit_array[i])
            {
               bd_time->tm_mday++;
               bd_time->tm_wday++;
               if (bd_time->tm_wday == 7)
               {
                  bd_time->tm_wday = 0;
               }
               if (check_day(te, bd_time) == INCORRECT)
               {
                  return(0);
               }
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to locate any valid hour!? (%s %d)\n",
                   __FILE__, __LINE__);
         return(0);
      }
      if (bd_time->tm_hour != i)
      {
         bd_time->tm_hour = i;
#ifdef _WORKING_LONG_LONG
         if (((ALL_MINUTES & te->minute) == ALL_MINUTES) ||
             ((ALL_MINUTES & te->continuous_minute) == ALL_MINUTES))
         {
            bd_time->tm_min = 0;
         }
         else
         {
            for (i = 0; i < bd_time->tm_min; i++)
            {
               if ((te->minute & bit_array_long[i]) ||
                   (te->continuous_minute & bit_array_long[i]))
               {
                  break;
               }
            }
            bd_time->tm_min = i;
         }
#else
         for (i = 0; i < bd_time->tm_min; i++)
         {
            if (bittest(te->minute, i) || bittest(te->continuous_minute, i))
            {
               break;
            }
         }
         bd_time->tm_min = i;
#endif
      }
   }
   bd_time->tm_sec = 0;

   return(mktime(bd_time));
}


/*++++++++++++++++++++++++++++ check_month() ++++++++++++++++++++++++++++*/
static int
check_month(struct bd_time_entry *te, struct tm *bd_time)
{
   int gotcha,
       i;

   /* Evaluate month (1-12) [0-11] */
   if ((ALL_MONTH & te->month) != ALL_MONTH)
   {
      gotcha = NO;
      for (i = bd_time->tm_mon; i < 12; i++)
      {
         if (te->month & bit_array[i])
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         for (i = 0; i < bd_time->tm_mon; i++)
         {
            if (te->month & bit_array[i])
            {
               bd_time->tm_year++;
               gotcha = YES;
               break;
            }
         }
      }
      if (gotcha == NO)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to locate any valid month!? (%s %d)\n",
                   __FILE__, __LINE__);
         return(INCORRECT);
      }
      if (bd_time->tm_mon != i)
      {
         bd_time->tm_mon = i;
         bd_time->tm_mday = 1;
         bd_time->tm_hour = 0;
         bd_time->tm_min = 0;
         if (te->day_of_week != ALL_DAY_OF_WEEK)
         {
            time_t time_val;

            time_val = mktime(bd_time);
            bd_time = localtime(&time_val);
         }
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_day() +++++++++++++++++++++++++++++*/
static int
check_day(struct bd_time_entry *te, struct tm *bd_time)
{
   int gotcha,
       i;

   if ((te->day_of_week != ALL_DAY_OF_WEEK) &&
       (te->day_of_month != ALL_DAY_OF_MONTH))
   {
      int dow = bd_time->tm_wday,
          greatest_dom;

      gotcha = NO;
      do
      {
         greatest_dom = get_greatest_dom(bd_time->tm_mon, bd_time->tm_year + 1900);
         for (i = (bd_time->tm_mday - 1); i < greatest_dom; i++)
         {
            if ((te->day_of_month & bit_array[i]) &&
                (te->day_of_week & bit_array[dow]))
            {
               gotcha = YES;
               break;
            }
            dow++;
            if (dow == 7)
            {
               dow = 0;
            }
         }
         if (gotcha == NO)
         {
            bd_time->tm_mon++;
            if (bd_time->tm_mon == 12)
            {
               bd_time->tm_mon = 0;
               bd_time->tm_year++;
            }
            bd_time->tm_wday = dow;
            if (check_month(te, bd_time) == INCORRECT)
            {
               return(INCORRECT);
            }
            dow = bd_time->tm_wday;
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      } while (gotcha == NO);
      if (gotcha == NO)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to locate any valid day of month!? (%s %d)\n",
                   __FILE__, __LINE__);
         return(INCORRECT);
      }
      if (bd_time->tm_mday != (i + 1))
      {
         bd_time->tm_mday = i + 1;
         bd_time->tm_hour = 0;
         bd_time->tm_min = 0;
      }
   }
   else
   {
      /* Evaluate day of week (1-7) [0-6] */
      if ((ALL_DAY_OF_WEEK & te->day_of_week) != ALL_DAY_OF_WEEK)
      {
         gotcha = NO;
         for (i = (bd_time->tm_wday - 1); i < 7; i++)
         {
            if (te->day_of_week & bit_array[i])
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            for (i = 0; i < (bd_time->tm_wday - 1); i++)
            {
               if (te->day_of_week & bit_array[i])
               {
                  gotcha = NEITHER;
                  break;
               }
            }
         }
         if (gotcha == NO)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to locate any valid day of week!? (%s %d)\n",
                      __FILE__, __LINE__);
            return(INCORRECT);
         }
         if ((bd_time->tm_wday - 1) != i)
         {
            if (gotcha == NEITHER)
            {
               bd_time->tm_mday += (7 - bd_time->tm_wday + i + 1);
            }
            else
            {
               bd_time->tm_mday += (i + 1 - bd_time->tm_wday);
            }
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      }

      /* Evaluate day of month (1-31) [1-31] */
      if ((ALL_DAY_OF_MONTH & te->day_of_month) != ALL_DAY_OF_MONTH)
      {
         gotcha = NO;
         for (i = (bd_time->tm_mday - 1); i < 31; i++)
         {
            if (te->day_of_month & bit_array[i])
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            for (i = 0; i < (bd_time->tm_mday - 1); i++)
            {
               if (te->day_of_month & bit_array[i])
               {
                  bd_time->tm_mon++;
                  gotcha = YES;
                  break;
               }
            }
         }
         if (gotcha == NO)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to locate any valid day of month!? (%s %d)\n",
                      __FILE__, __LINE__);
            return(INCORRECT);
         }
         if (bd_time->tm_mday != (i + 1))
         {
            bd_time->tm_mday = i + 1;
            bd_time->tm_hour = 0;
            bd_time->tm_min = 0;
         }
      }
   }

   return(SUCCESS);
}


/*-------------------------- get_greatest_dom() -------------------------*/
static int
get_greatest_dom(int month, int year)
{
   int dom;

   switch (month)
   {
      case 0:
         dom = 31;
         break;
      case 1:
         if (((year % 4) == 0) &&
             (((year % 100) != 0) || ((year % 400) == 0)))
         {
            dom = 29;
         }
         else
         {
            dom = 28;
         }
         break;
      case 2:
         dom = 31;
         break;
      case 3:
         dom = 30;
         break;
      case 4:
         dom = 31;
         break;
      case 5:
         dom = 30;
         break;
      case 6:
         dom = 31;
         break;
      case 7:
         dom = 31;
         break;
      case 8:
         dom = 30;
         break;
      case 9:
         dom = 31;
         break;
      case 10:
         dom = 30;
         break;
      case 11:
         dom = 31;
         break;
      default :
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Get a new operating system! (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
   }

   return(dom);
}
