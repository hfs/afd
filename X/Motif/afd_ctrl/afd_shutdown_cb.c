/*
 *  afd_shutdown_cb.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_shutdown_cb -
 **
 ** SYNOPSIS
 **   afd_shutdown_cb
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.03.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "bang0.symbol"
#include "bang1.symbol"

extern Display *display;


/*########################### afd_shutdown_cb() #########################*/
void
afd_shutdown_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget        dialog;
    XtAppContext  app = XtWidgetToApplicationContext(w);
    XmString      text;
    extern void   done(), destroy_it(), blink();
    Screen        *screen = XtScreen(w);
    Pixel         fg, bg;
    Arg           args[5];
    int           n, depth;
    TimeOutClientData *data = XtNew(TimeOutClientData);

    /* Create the dialog */
    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); n++;
    dialog = XmCreateMessageDialog(w, "danger", args, n);

    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));

    XtAddCallback(dialog, XmNokCallback, done, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, destroy_it, data);

    /* now that dialog has been created, it's colors are initialised */
    XtVaGetValues(dialog,
                  XmNforeground, &fg,
                  XmNbackground, &bg,
                  XmNdepth, &depth,
                  NULL);

    /* Create pixmaps that are going to be used as symbolPixmaps.
     * Use the foreground and background colors of the dialog.
     */
    data->pix1 = XCreatePixmapFromBitmapData(display, XtWindow(w),
        bang0_bits, bang0_width, bang0_height, fg, bg, depth);
    data->pix2 = XCreatePixmapFromBitmapData(display, XtWindow(w),
        bang1_bits, bang1_width, bang1_height, fg, bg, depth);
    /* complete the timeout client data */
    data->dialog = dialog;
    data->app = app;

    /* Add the timeout for blinking effect */
    data->id = XtAppAddTimeOut(app, 1000L, blink, data);

    /* display the help text and the appropriate pixmap */
    text = XmStringCreateLtoR(TEXT, XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(dialog,
                  XmNmessageString, text,
                  XmNsymbolPixmap, data->pix2,
                  NULL);
    XmStringFree(text);

    XtManageChild(dialog);
    XtPopup(XtParent(dialog), XtGrabNone);
}


/*+++++++++++++++++++++++++++++++ blink() +++++++++++++++++++++++++++++++*/
void
blink(client_data, id)
XtPointer client_data;
XtIntervalId *id;
{
    TimeOutClientData *data = (TimeOutClientData *)client_data;

    data->id = XtAppAddTimeOut(data->app, 250L, blink, data);
    XtVaSetValues(data->dialog,
                  XmNsymbolPixmap,  (data->which = !data->which) ? data->pix1 : data->pix2,
                  NULL);
}

void
done(dialog, client_data, call_data)
Widget dialog;
XtPointer client_data;
XtPointer call_data;
{
    XtDestroyWidget(dialog);
}

void
destroy_it(dialog, client_data, call_data)
Widget dialog;
XtPointer client_data;
XtPointer call_data;
{
    TimeOutClientData *data = (TimeOutClientData *)client_data;
    Pixmap symbol;

    XtRemoveTimeOut(data->id);
    XFreePixmap(display, data->pix1);
    XFreePixmap(display, data->pix2);
    XtFree(data);
}
