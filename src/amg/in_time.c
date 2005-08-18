/*
 *  in_time.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000, 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   in_time - function that checks if the given time falls into
 *              the given crontab like entry
 **
 ** SYNOPSIS
 **   int in_time(time_t current_time, struct bd_time_entry *te);
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The function will return YES when current_time falls into
 **   the specified time range. Otherwise NO will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.09.2000 H.Kiehl Created
 **   12.05.2001 H.Kiehl Failed to see sunday. Fixed.
 **
 */
DESCR__E_M3

#include <time.h>
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif
#include "amgdefs.h"
#include "bit_array.h"


/*############################# in_time() ###############################*/
int
in_time(time_t current_time, struct bd_time_entry *te)
{
   int       wday;
   struct tm *bd_time;     /* Broken-down time */

   bd_time = localtime(&current_time);

   if (te->month & bit_array[bd_time->tm_mon])
   {
      if (te->day_of_month & bit_array[bd_time->tm_mday - 1])
      {
         /*
          * In struct tm tm_wday is 0 for Sunday. But we use 7
          * for Sunday.
          */
         wday = bd_time->tm_wday;
         if (wday == 0)
         {
            wday = 7;
         }
         if (te->day_of_week & bit_array[wday - 1])
         {
            if (te->hour & bit_array[bd_time->tm_hour])
            {
               /* Evaluate minute (0-59) [0-59] */
#ifdef _WORKING_LONG_LONG
               if (((ALL_MINUTES & te->minute) == ALL_MINUTES) ||
                   ((ALL_MINUTES & te->continuous_minute) == ALL_MINUTES))
               {
                  return(YES);
               }
               else
               {
                  if ((te->minute & bit_array_long[bd_time->tm_min]) ||
                      (te->continuous_minute & bit_array_long[bd_time->tm_min]))
                  {
                     return(YES);
                  }
               }
#else
               if (bittest(te->minute, bd_time->tm_min) ||
                   bittest(te->continuous_minute, bd_time->tm_min))
               {
                  return(YES);
               }
#endif
            }
         }
      }
   }

   return(NO);
}
