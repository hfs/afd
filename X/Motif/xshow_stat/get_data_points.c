/*
 *  get_data_points.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data_points - Determines the values to put on the X-axis.
 **
 ** SYNOPSIS
 **   void get_data_points(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.01.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strerror()                       */
#include <stdlib.h>                  /* exit()                           */
#include <errno.h>
#include "xshow_stat.h"
#include "statdefs.h"

extern char           line_style,
                      **x_data_point;
extern int            data_length,
                      first_data_pos,
                      no_of_x_data_points,
                      time_type,
                      x_data_spacing;
extern struct afdstat *stat_db;


/*########################## get_data_points() ##########################*/
void
get_data_points(void)
{
   int current_value,
       i,
       max_value,
       spacing,
       value = 0;

   if (time_type == HOUR_STAT)
   {
      current_value = (stat_db[0].sec_counter * STAT_RESCAN_TIME) / 60;
      max_value = SECS_PER_HOUR / (60 / STAT_RESCAN_TIME);
   }
   else if (time_type == DAY_STAT)
        {
           current_value = stat_db[0].hour_counter;
           max_value = HOURS_PER_DAY;
        }
   else if (time_type == YEAR_STAT)
        {
           current_value = stat_db[0].day_counter;
           max_value = DAYS_PER_YEAR;
        }
        else
        {
           (void)fprintf(stderr, "Wrong time_type <%d>. (%s %d)\n",
                         time_type, __FILE__, __LINE__);
           exit(INCORRECT);
        }
   spacing = max_value / no_of_x_data_points;
   while ((max_value % no_of_x_data_points) != 0)
   {
      no_of_x_data_points--;
      x_data_spacing = data_length / no_of_x_data_points;
      spacing = max_value / no_of_x_data_points;
   }

   /* Allocate memory for the data points. */
   if (x_data_point != NULL)
   {
      FREE_RT_ARRAY(x_data_point);
   }
   RT_ARRAY(x_data_point, no_of_x_data_points, 4, char);

   first_data_pos = 0;
   for (i = 0; i < no_of_x_data_points; i++)
   {
      if (time_type == YEAR_STAT)
      {
         (void)sprintf(x_data_point[i], "%3d",  value);
      }
      else
      {
         (void)sprintf(x_data_point[i], "%2d",  value);
      }
      if (value <= current_value)
      {
         first_data_pos = i;
      }
      value += spacing;
   }
   first_data_pos++;

   return;
}
