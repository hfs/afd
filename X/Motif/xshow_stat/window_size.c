/*
 *  window_size.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001, 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   window_size - calculates the new window size
 **
 ** SYNOPSIS
 **   int window_size(int *window_width, int *window_height)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   YES - Window size must be changed
 **   NO  - Window size must not be changed
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.12.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                 /* strerror()                       */
#include <stdlib.h>                 /* malloc(), free()                 */
#include <errno.h>
#include "xshow_stat.h"
#include "statdefs.h"

extern Display      *display;
extern int          data_height,
                    data_length,
                    no_of_x_data_points,
                    no_of_y_data_points,
                    stat_type,
                    time_type,
                    x_data_spacing,
                    x_offset_left_xaxis,
                    x_offset_right_xaxis,
                    y_offset_top_yaxis,
                    y_offset_bottom_yaxis;
extern unsigned int glyph_height,
                    glyph_width;


/*########################### window_size() ############################*/
int
window_size(int *window_width,
            int *window_height,
            int new_window_width,
            int new_window_height)
{
   int calc_window_width,
       calc_window_height,
       window_size_changed;

   /*
    * The window size depends on the display size and the type of
    * statistic information we want to display.
    */
   if ((new_window_width == 0) && (new_window_height == 0))
   {
      new_window_width = DisplayWidth(display, DefaultScreen(display)) / 2;
      new_window_height = DisplayHeight(display, DefaultScreen(display)) / 2;
   }

   /*
    * Determine all values for the X-axis.
    */
   data_length = new_window_width - x_offset_left_xaxis - x_offset_right_xaxis;
   no_of_x_data_points = data_length / x_data_spacing;
   switch(time_type)
   {
      case HOUR_STAT : /* Statistic for the last hour. */
         if (no_of_x_data_points > (SECS_PER_HOUR / (60 /STAT_RESCAN_TIME)))
         {
            no_of_x_data_points = (SECS_PER_HOUR / (60 /STAT_RESCAN_TIME));
            x_data_spacing = data_length / no_of_x_data_points;
         }
         break;

      case DAY_STAT : /* Statistic for the last day. */
         if (no_of_x_data_points > HOURS_PER_DAY)
         {
            no_of_x_data_points = HOURS_PER_DAY;
            x_data_spacing = data_length / no_of_x_data_points;
         }
         break;

      case YEAR_STAT : /* Statistic for the last year. */
         if (no_of_x_data_points > DAYS_PER_YEAR)
         {
            no_of_x_data_points = DAYS_PER_YEAR;
            x_data_spacing = data_length / no_of_x_data_points;
         }
         break;

      default : /* Wrong value. */
         (void)fprintf(stderr, "Unknown time type %d. (%s %d)\n",
                       time_type, __FILE__, __LINE__);
         exit(INCORRECT);
   }
   get_data_points();

   /*
    * Determine all values for the Y-axis.
    */
   data_height = new_window_height - y_offset_top_yaxis - y_offset_bottom_yaxis;
   no_of_y_data_points = data_height / y_data_spacing;
   max_y_value = 0;
   for (i = 0; i < host_counter; i++)
   {
      switch(stat_type)
      {
         case SHOW_KBYTE_STAT : /* Volume that has been tansmitted. */
            break;
         case SHOW_FILE_STAT : /* Number of files transmitted. */
            break;
         case SHOW_CONNECT_STAT : /* Number of network connections. */
            break;
         default : /* Wrong value. */
            (void)fprintf(stderr, "Unknown statistic type %d.\n", stat_type);
            exit(INCORRECT);
      }
   }

   /* Window resize necessary ? */
   if ((new_window_width  != *window_width) ||
       (new_window_height != *window_height))
   {
      window_size_changed = YES;
   }
   else
   {
      window_size_changed = NO;
   }

   *window_width  = new_window_width;
   *window_height = new_window_height;

   return(window_size_changed);
}
