/*
 *  setup_window.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   setup_window - determines the initial size for the window
 **
 ** SYNOPSIS
 **   void setup_window(char *font_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.04.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf(), stderr                       */
#include <math.h>             /* log10()                                 */
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "permission.h"

extern Display        *display;
extern XFontStruct    *font_struct;
extern XmFontList     fontlist;
extern Widget         mw[],
                      ow[];
extern GC             letter_gc,
                      normal_letter_gc,
                      locked_letter_gc,
                      color_letter_gc,
                      default_bg_gc,
                      normal_bg_gc,
                      locked_bg_gc,
                      label_bg_gc,
                      button_bg_gc,
                      tr_bar_gc,
                      color_gc,
                      black_line_gc,
                      white_line_gc,
                      led_gc;
extern int            no_of_hosts,
                      line_length,
                      line_height;
extern unsigned int   glyph_height,
                      glyph_width,
                      text_offset;
extern unsigned long  color_pool[];
extern char           stat_type;
extern struct afdstat *stat_db;


/*########################### setup_window() ###########################*/
void
setup_window(char *font_name)
{
   int             i,
                   new_max_bar_length;
   XmFontListEntry entry;

   /* Get width and height of font and fid for the GC */
   if ((font_struct = XLoadQueryFont(display, font_name)) == NULL)
   {
      (void)fprintf(stderr, "Could not load %s font.\n", font_name);
      if ((font_struct = XLoadQueryFont(display, "fixed")) == NULL)
      {
         (void)fprintf(stderr, "Could not load %s font.\n", "fixed");
         exit(INCORRECT);
      }
   }
   if ((entry = XmFontListEntryLoad(display, font_name, XmFONT_IS_FONT,
                                    "TAG1")) == NULL)
   {
       (void)fprintf(stderr,
                     "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                     strerror(errno), __FILE__, __LINE__);
       exit(INCORRECT);
   }
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   if (line_height != 0)
   {
      XtVaSetValues(mw[HOST_W], XmNfontList, fontlist, NULL);
      XtVaSetValues(ow[0], XmNfontList, fontlist, NULL);

      XmFontListFree(fontlist);
   }

   glyph_height       = font_struct->ascent + font_struct->descent;
   glyph_width        = font_struct->per_char->width;
   new_max_bar_length = glyph_width * BAR_LENGTH_MODIFIER;

   /* We now have to recalculate the length of all */
   /* bars and the scale, because a font change    */
   /* might have occurred.                         */
   if (new_max_bar_length != max_bar_length)
   {
      unsigned int new_bar_length;

      max_bar_length = new_max_bar_length;
      step_size = MAX_INTENSITY / max_bar_length;

      /* NOTE: We do not care what the line style is because the */
      /*       following could happen: font size = 7x13 style =  */
      /*       chars + bars, the user now wants chars only and   */
      /*       then reduces the font to 5x7. After a while he    */
      /*       wants the bars again. Thus we always need to re-  */
      /*       calculate the bar length and queue scale!         */
      for (i = 0; i < no_of_hosts; i++)
      {
         /* Calculate new scale for error bar */
         if (connect_data[i].error_counter > 0)
         {
            if (fsa[i].max_errors < 1)
            {
               connect_data[i].scale = (double)max_bar_length;
            }
            else
            {
               connect_data[i].scale = max_bar_length / fsa[i].max_errors;
            }
            new_bar_length = connect_data[i].error_counter * connect_data[i].scale;
            if (new_bar_length >= max_bar_length)
            {
               connect_data[i].bar_length[ERROR_BAR_NO] = max_bar_length;
               connect_data[i].red_color_offset = MAX_INTENSITY;
               connect_data[i].green_color_offset = 0;
            }
            else
            {
               connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
               connect_data[i].red_color_offset = new_bar_length * step_size;
               connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;
            }
         }
         else
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = 0;
            connect_data[i].red_color_offset = 0;
            connect_data[i].green_color_offset = MAX_INTENSITY;
         }

         /* Calculate new bar length for the transfer rate */
         if (connect_data[i].average_tr > 1.0)
         {
            /* First ensure we do not divide by zero */
            if (connect_data[i].max_average_tr < 2.0)
            {
               connect_data[i].bar_length[TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length /
                 log10((double) 2.0);
            }
            else
            {
               connect_data[i].bar_length[TR_BAR_NO] =
                 log10(connect_data[i].average_tr) *
                 max_bar_length /
                 log10(connect_data[i].max_average_tr);
            }
         }
         else
         {
            connect_data[i].bar_length[TR_BAR_NO] = 0;
         }
      }
   }

   text_offset     = font_struct->ascent;
   line_height     = SPACE_ABOVE_LINE + glyph_height + SPACE_BELOW_LINE;
   bar_thickness_2 = glyph_height / 2;
   button_width    = 2 * glyph_width;
   y_offset_led    = (glyph_height - glyph_width) / 2;
   led_width       = glyph_height / 3;
   line_length     = DEFAULT_FRAME_SPACE + (MAX_HOSTNAME_LENGTH * glyph_width) +
                     DEFAULT_FRAME_SPACE + glyph_width +
                     (2 * (led_width + LED_SPACING)) + DEFAULT_FRAME_SPACE +
                     (MAX_NO_PARALLEL_JOBS * (button_width + BUTTON_SPACING)) +
                     glyph_width + DEFAULT_FRAME_SPACE;

   if (line_style == BARS_ONLY)
   {
      line_length += (int)max_bar_length + DEFAULT_FRAME_SPACE;
   }
   else  if (line_style == CHARACTERS_ONLY)
         {
            line_length += (17 * glyph_width) + DEFAULT_FRAME_SPACE;
         }
         else
         {
            line_length += (17 * glyph_width) + DEFAULT_FRAME_SPACE +
                           (int)max_bar_length + DEFAULT_FRAME_SPACE;
         }

   x_offset_debug_led = DEFAULT_FRAME_SPACE +
                        (MAX_HOSTNAME_LENGTH * glyph_width) +
                        DEFAULT_FRAME_SPACE;
   x_offset_led = x_offset_debug_led + DEFAULT_FRAME_SPACE + glyph_width;
   x_offset_proc = x_offset_led + (2 * (led_width + LED_SPACING));
   if (line_style == BARS_ONLY)
   {
      x_offset_bars = x_offset_proc + (MAX_NO_PARALLEL_JOBS *
                      (button_width + BUTTON_SPACING)) +
                      glyph_width + DEFAULT_FRAME_SPACE;
   }
   else  if (line_style == CHARACTERS_ONLY)
         {
            x_offset_characters = x_offset_proc + (MAX_NO_PARALLEL_JOBS *
                                  (button_width + BUTTON_SPACING)) +
                                  glyph_width + DEFAULT_FRAME_SPACE;
         }
         else
         {
            x_offset_characters = x_offset_proc + (MAX_NO_PARALLEL_JOBS *
                                  (button_width + BUTTON_SPACING)) +
                                  glyph_width + DEFAULT_FRAME_SPACE;
            x_offset_bars = x_offset_characters + (17 * glyph_width) +
                            DEFAULT_FRAME_SPACE;
         }

   if (no_of_columns != 0)
   {
      calc_but_coord();
   }

   return;
}


/*############################# init_gcs() #############################*/
void
init_gcs(void)
{
   XGCValues  gc_values;
   Window     window = RootWindow(display, DefaultScreen(display));

   /* GC for drawing letters on default background */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[FG];
   gc_values.background = color_pool[DEFAULT_BG];
   letter_gc = XCreateGC(display, window, GCFont | GCForeground |
                         GCBackground, &gc_values);
   XSetFunction(display, letter_gc, GXcopy);

   /* GC for drawing letters for normal selection */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[WHITE];
   gc_values.background = color_pool[BLACK];
   normal_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                GCForeground | GCBackground, &gc_values);
   XSetFunction(display, normal_letter_gc, GXcopy);

   /* GC for drawing letters for locked selection */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[WHITE];
   gc_values.background = color_pool[LOCKED_INVERSE];
   locked_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                GCForeground | GCBackground, &gc_values);
   XSetFunction(display, locked_letter_gc, GXcopy);

   /* GC for drawing letters for host name */
   gc_values.font = font_struct->fid;
   gc_values.foreground = color_pool[FG];
   gc_values.background = color_pool[WHITE];
   color_letter_gc = XCreateGC(display, window, (XtGCMask) GCFont |
                                 GCForeground | GCBackground, &gc_values);
   XSetFunction(display, color_letter_gc, GXcopy);

   /* GC for drawing the default background */
   gc_values.foreground = color_pool[DEFAULT_BG];
   default_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, default_bg_gc, GXcopy);

   /* GC for drawing the normal selection background */
   gc_values.foreground = color_pool[BLACK];
   normal_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, normal_bg_gc, GXcopy);

   /* GC for drawing the locked selection background */
   gc_values.foreground = color_pool[LOCKED_INVERSE];
   locked_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, locked_bg_gc, GXcopy);

   /* GC for drawing the label background */
   gc_values.foreground = color_pool[LABEL_BG];
   label_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                           &gc_values);
   XSetFunction(display, label_bg_gc, GXcopy);

   /* GC for drawing the button background */
   gc_values.foreground = color_pool[BUTTON_BACKGROUND];
   button_bg_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, button_bg_gc, GXcopy);

   /* GC for drawing the background for "bytes on input" bar */
   gc_values.foreground = color_pool[TR_BAR];
   tr_bar_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                            &gc_values);
   XSetFunction(display, tr_bar_gc, GXcopy);

   /* GC for drawing the background for queue bar and leds */
   gc_values.foreground = color_pool[TR_BAR];
   color_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                        &gc_values);
   XSetFunction(display, color_gc, GXcopy);

   /* GC for drawing the black lines */
   gc_values.foreground = color_pool[BLACK];
   black_line_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, black_line_gc, GXcopy);

   /* GC for drawing the white lines */
   gc_values.foreground = color_pool[WHITE];
   white_line_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                             &gc_values);
   XSetFunction(display, white_line_gc, GXcopy);

   /* GC for drawing led's */
   gc_values.foreground = color_pool[TR_BAR];
   led_gc = XCreateGC(display, window, (XtGCMask) GCForeground,
                      &gc_values);
   XSetFunction(display, led_gc, GXcopy);

   /* Flush buffers so all GC's are known */
   XFlush(display);

   return;
}
