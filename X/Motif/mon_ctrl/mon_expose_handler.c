/*
 *  mon_expose_handler.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 1998 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mon_expose_handler - handles any expose event for label and
 **                        line window
 **
 ** SYNOPSIS
 **   void mon_expose_handler_label(Widget                      w,
 **                                 XtPointer                   client_data,
 **                                 XmDrawingAreaCallbackStruct *call_data)
 **   void mon_expose_handler(Widget                      w,
 **                           XtPointer                   client_data,
 **                           XmDrawingAreaCallbackStruct *call_data)
 **
 ** DESCRIPTION
 **   When an expose event occurs, only those parts of the window
 **   will be redrawn that where covered. For the label window
 **   the whole line will always be redrawn, also if only part of
 **   it was covered. In the line window we will only redraw those
 **   those lines that were covered.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.10.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* printf()                              */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include "afd_ctrl.h"
#include "mon_ctrl.h"
#include "permission.h"

extern Display                 *display;
extern XtAppContext            app;
extern XtIntervalId            interval_id_afd;
extern Window                  label_window,
                               line_window;
extern Widget                  appshell,
                               mw[];
extern int                     magic_value,
                               no_input,
                               no_of_afds,
                               no_of_jobs_selected,
                               window_width,
                               window_height,
                               line_length,
                               line_height,
                               no_of_columns,
                               no_of_rows,
                               redraw_time_line;
extern unsigned int            glyph_height;
extern struct mon_line         *connect_data;
extern struct mon_control_perm mcp;


/*###################### mon_expose_handler_label() #####################*/
void
mon_expose_handler_label(Widget                      w,
                         XtPointer                   client_data,
                         XmDrawingAreaCallbackStruct *call_data)
{
   XEvent *p_event = call_data->event;

   if (p_event->xexpose.count == 0)
   {
      draw_label_line();
      XFlush(display);
   }

   return;
}


/*###################### mon_expose_handler_line() ######################*/
void
mon_expose_handler_line(Widget                      w,
                        XtPointer                   client_data,
                        XmDrawingAreaCallbackStruct *call_data)
{
   XEvent     *p_event = call_data->event;
   int        i,
              top_line,
              bottom_line,
              left_column,
              top_row,
              bottom_row,
              right_column;
   static int ft_exposure_line = 0; /* first time exposure line */

   /* Get the channel no's, which have to be redrawn */
   left_column = p_event->xexpose.x / line_length;
   top_row = p_event->xexpose.y / line_height;
   bottom_row = (p_event->xexpose.y  + p_event->xexpose.height) /
                 line_height;
   if (bottom_row >= no_of_rows)
   {
      bottom_row--;
   }
   right_column = (p_event->xexpose.x  + p_event->xexpose.width) /
                   line_length;
   if (right_column >= no_of_columns)
   {
      right_column--;
   }

#ifdef _DEBUG
   (void)printf("xexpose.x   = %d    xexpose.width= %d   line_length = %d\n",
                 p_event->xexpose.x, p_event->xexpose.width, line_length);
   (void)printf("left_column = %d    right_column = %d\n",
                 left_column, right_column);
   (void)printf("top_row     = %d    bottom_row   = %d\n",
                 top_row, bottom_row);
#endif

   /* Note which lines have to be redrawn, but do not    */
   /* redraw them here. First collect all expose events. */
   do
   {
      top_line = (right_column * no_of_rows) + top_row;
      bottom_line = (right_column * no_of_rows) + bottom_row;
      while (bottom_line >= no_of_afds)
      {
         bottom_line--;
      }

#ifdef _DEBUG
      (void)printf("top_line     = %d    bottom_line   = %d\n",
                   top_line, bottom_line);
      (void)printf("=====> no_of_columns = %d   no_of_rows = %d <=====\n",
                   no_of_columns, no_of_rows);
#endif

      for (i = top_line; i <= bottom_line; i++)
      {
         connect_data[i].expose_flag = YES;
      }
      right_column--;
   } while (left_column <= right_column);

   /* Now see if there are still expose events. If so, */
   /* do NOT redraw.                                   */
   if (p_event->xexpose.count == 0)
   {
      for (i = 0; i < no_of_afds; i++)
      {
         if (connect_data[i].expose_flag == YES)
         {
            draw_line_status(i, 1);
            connect_data[i].expose_flag = NO;
         }
      }

      XFlush(display);

      /*
       * To ensure that widgets are realized before calling XtAppAddTimeOut()
       * we wait for the widget to get its first expose event. This should
       * take care of the nasty BadDrawable error on slow connections.
       */
      if (ft_exposure_line == 0)
      {
         int       bs_attribute;
         Dimension height;
         Screen    *c_screen = ScreenOfDisplay(display, DefaultScreen(display));

         interval_id_afd = XtAppAddTimeOut(app, redraw_time_line,
                                           (XtTimerCallbackProc)check_afd_status,
                                           w);
         ft_exposure_line = 1;

         if ((bs_attribute = DoesBackingStore(c_screen)) != NotUseful)
         {
            XSetWindowAttributes attr;

            attr.backing_store = bs_attribute;
            attr.save_under = DoesSaveUnders(c_screen);
            XChangeWindowAttributes(display, line_window,
                                    CWBackingStore | CWSaveUnder, &attr);
            XChangeWindowAttributes(display, label_window,
                                    CWBackingStore, &attr);
            if (no_input == False)
            {
               XChangeWindowAttributes(display, XtWindow(mw[MON_W]),
                                       CWBackingStore, &attr);

               if ((mcp.show_slog != NO_PERMISSION) ||
                   (mcp.show_rlog != NO_PERMISSION) ||
                   (mcp.show_tlog != NO_PERMISSION) ||
                   (mcp.show_ilog != NO_PERMISSION) ||
                   (mcp.show_olog != NO_PERMISSION) ||
                   (mcp.show_elog != NO_PERMISSION) ||
                   (mcp.show_queue != NO_PERMISSION) ||
                   (mcp.afd_load != NO_PERMISSION))
               {
                  XChangeWindowAttributes(display, XtWindow(mw[LOG_W]),
                                          CWBackingStore, &attr);
               }

               if ((mcp.amg_ctrl != NO_PERMISSION) ||
                   (mcp.fd_ctrl != NO_PERMISSION) ||
                   (mcp.rr_dc != NO_PERMISSION) ||
                   (mcp.rr_hc != NO_PERMISSION) ||
                   (mcp.edit_hc != NO_PERMISSION) ||
                   (mcp.dir_ctrl != NO_PERMISSION) ||
                   (mcp.startup_afd != NO_PERMISSION) ||
                   (mcp.shutdown_afd != NO_PERMISSION))
               {
                  XChangeWindowAttributes(display, XtWindow(mw[CONTROL_W]),
                                          CWBackingStore, &attr);
               }

               XChangeWindowAttributes(display, XtWindow(mw[CONFIG_W]),
                                       CWBackingStore, &attr);
#ifdef _WITH_HELP_PULLDOWN
               XChangeWindowAttributes(display, XtWindow(mw[HELP_W]),
                                       CWBackingStore, &attr);
#endif
            } /* if (no_input == False) */
         }

         /*
          * Calculate the magic unkown height factor we need to add to the
          * height of the widget when it is being resized.
          */
         XtVaGetValues(appshell, XmNheight, &height, NULL);
         magic_value = height - (window_height + line_height + glyph_height);
      }
   }

   return;
}
