/*
 *  create_tv_window.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_tv_window - create detailed transfer view window
 **
 ** SYNOPSIS
 **   void create_tv_window(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include "afd_ctrl.h"
#include "permission.h"

/* External global variables. */
extern Widget                  appshell,
                               detailed_window_w,
                               transviewshell,
                               tv_label_window_w;
extern XtIntervalId            interval_id_tv;
extern int                     filename_display_length,
                               ft_exposure_tv_line,
                               line_height,
                               no_of_jobs_selected;
extern char                    tv_window;
extern Dimension               tv_window_width,
                               tv_window_height;
extern unsigned long           color_pool[];
extern struct afd_control_perm acp;

/* Local funtion prototype definitions */
static void                    tv_destroy(Widget, XtPointer, XEvent *),
                               tv_input(Widget, XtPointer, XEvent *);


/*########################## create_tv_window() #########################*/
void
create_tv_window(void)
{
   Widget          tv_form_w;
   Arg             args[MAXARGS];
   Cardinal        argcount;

   transviewshell = XtVaCreatePopupShell("View Transfer",
                                         topLevelShellWidgetClass,
                                         appshell, NULL);

   /* Setup and determine window parameters */
   setup_tv_window();

   /* Get window size */
   (void)tv_window_size(&tv_window_width, &tv_window_height);

   /* Create managing widget */
   tv_form_w = XmCreateForm(transviewshell, "trans_view", NULL, 0);
   XtManageChild(tv_form_w);

   /* Create the tv_label_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension)line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, tv_window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   tv_label_window_w = XmCreateDrawingArea(tv_form_w, "tv_label_window_w",
                                           args, argcount);
   XtManageChild(tv_label_window_w);

   /* Create the detailed_window_w */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, tv_window_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, tv_window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, tv_label_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   detailed_window_w = XmCreateDrawingArea(tv_form_w, "detailed_window_w",
                                           args, argcount);
   XtManageChild(detailed_window_w);

   /* Add expose handler for label and detailed window */
   XtAddCallback(tv_label_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_label, (XtPointer)1);
   XtAddCallback(detailed_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_tv_line, NULL);

   XtAddEventHandler(detailed_window_w, ButtonPressMask, False,
                     (XtEventHandler)tv_input, NULL);
   XtAddCallback(tv_label_window_w, XmNdestroyCallback,
                 (XtCallbackProc)tv_destroy, NULL);

#ifdef _EDITRES
   XtAddEventHandler(transviewshell, (EventMask)0, True,
                      _XEditResCheckMessages, NULL);
#endif

   return;
}


/*############################# tv_input() ##############################*/
static void
tv_input(Widget      w,
         XtPointer   client_data,
         XEvent      *event)
{
   if ((acp.view_jobs != NO_PERMISSION) &&
       (no_of_jobs_selected > 0) &&
       (event->xkey.state & ControlMask))
   {
      if (event->xbutton.button == 1)
      {
         if (filename_display_length > 5)
         {
            int i;

            filename_display_length--;
            setup_tv_window();
            (void)resize_tv_window();

            draw_tv_label_line();
            for (i = 0; i < no_of_jobs_selected; i++)
            {
               draw_detailed_line(i);
            }
         }
      }
      else
      {
         if (filename_display_length < MAX_FILENAME_LENGTH)
         {
            int i;

            filename_display_length++;
            setup_tv_window();
            (void)resize_tv_window();

            draw_tv_label_line();
            for (i = 0; i < no_of_jobs_selected; i++)
            {
               draw_detailed_line(i);
            }
         }
      }
   }

   return;
}


/*########################### tv_destroy() ##############################*/
static void
tv_destroy(Widget      w,
           XtPointer   client_data,
           XEvent      *event)
{
   if (tv_window == ON)
   {
      XtRemoveTimeOut(interval_id_tv);
      XtDestroyWidget(transviewshell);
      transviewshell = NULL;
      ft_exposure_tv_line = 0;
      tv_window = OFF;
   }

   return;
}
