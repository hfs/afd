/*
 *  show_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_info - displays more detailed information on a single
 **               log entry
 **
 ** SYNOPSIS
 **   void show_info(char *text)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.04.1997 H.Kiehl Created
 **   16.01.1999 H.Kiehl Add scrollbars when list gets too long.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <X11/keysym.h>
#ifdef WITH_EDITRES
#include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "x_common_defs.h"

/* External global variables */
extern Display   *display;
extern Widget    appshell;
extern char      font_name[];
extern int       max_x,
                 max_y;
extern Dimension button_height;

/* Local global variables */
static int       glyph_height,
                 glyph_width;
static Widget    infoshell = (Widget)NULL,
                 text_w;

/* Local function prototypes. */
static void      close_info_button(Widget, XtPointer, XtPointer);


/*############################## show_info() ############################*/
void
show_info(char *text)
{
   static Window win = (Window)NULL;
   static int    max_vertical_lines;

   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((infoshell == (Widget)NULL) || (XtIsRealized(infoshell) == False) ||
       (XtIsSensitive(infoshell) != True))
   {
      Widget          form_w,
                      buttonbox_w,
                      button_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      i_fontlist;
      XmFontListEntry entry;
      XFontStruct     *font_struct;
      XmFontType      dummy;

      infoshell = XtVaCreatePopupShell("show_info", topLevelShellWidgetClass,
                                       appshell, NULL);

      /* Create managing widget */
      form_w = XmCreateForm(infoshell, "infoform", NULL, 0);

      /* Prepare font */
      if ((entry = XmFontListEntryLoad(XtDisplay(form_w), font_name, XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         if ((entry = XmFontListEntryLoad(XtDisplay(form_w), "fixed", XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      font_struct = (XFontStruct *)XmFontListEntryGetFont(entry, &dummy);
      glyph_height = font_struct->ascent + font_struct->descent;
      glyph_width  = font_struct->per_char->width;
      i_fontlist = XmFontListAppendEntry(NULL, entry);
      XmFontListEntryFree(&entry);

      /*
       * Lets determine the maximum number of lines that we can
       * display on this screen.
       */
      max_vertical_lines = (8 * (DisplayHeight(display, DefaultScreen(display)) / glyph_height)) / 10;

      /* A small text box. */
      argcount = 0;
      XtSetArg(args[argcount], XmNcursorPositionVisible, False);
      argcount++;
      XtSetArg(args[argcount], XmNhighlightThickness,    0);
      argcount++;
      XtSetArg(args[argcount], XmNeditable,              False);
      argcount++;
      XtSetArg(args[argcount], XmNeditMode,              XmMULTI_LINE_EDIT);
      argcount++;
      XtSetArg(args[argcount], XmNcolumns,               max_x + 2);
      argcount++;
      if (max_y > max_vertical_lines)
      {
         XtSetArg(args[argcount], XmNrows,                  max_vertical_lines + 2);
      }
      else
      {
         XtSetArg(args[argcount], XmNrows,                  max_y);
      }
      argcount++;
      XtSetArg(args[argcount], XmNscrollVertical,        True);
      argcount++;
      XtSetArg(args[argcount], XmNscrollHorizontal,      False);
      argcount++;
      XtSetArg(args[argcount], XmNfontList,              i_fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
      argcount++;
      text_w = XmCreateScrolledText(form_w, "info_text", args, argcount);
      XtManageChild(text_w);

      argcount = 0;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);

      /* Create close button. */
      button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             i_fontlist,
                        XmNtopAttachment,        XmATTACH_FORM,
                        XmNleftAttachment,       XmATTACH_FORM,
                        XmNrightAttachment,      XmATTACH_FORM,
                        XmNbottomAttachment,     XmATTACH_FORM,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)close_info_button, 0);
      XtManageChild(buttonbox_w);
      XtManageChild(form_w);

#ifdef WITH_EDITRES
      XtAddEventHandler(infoshell, (EventMask)0, True,
                        _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(infoshell, XtGrabNone);

   while (win == (Window)NULL)
   {
      win = XtWindow(infoshell);
   }

   /* Display the info text */
   if (max_y > max_vertical_lines)
   {
      XResizeWindow(display, win,
                    glyph_width * (max_x + 3 + 2),
                    (glyph_height * (max_vertical_lines + 1)) + button_height);
      XtVaSetValues(text_w,
                    XmNcolumns, max_x + 2,
                    XmNrows,    max_vertical_lines,
                    NULL);
   }
   else
   {
      XResizeWindow(display, win,
                    glyph_width * (max_x + 3 + 2),
                    (glyph_height * (max_y + 1)) + button_height);
      XtVaSetValues(text_w,
                    XmNcolumns, max_x,
                    XmNrows,    max_y,
                    NULL);
   }
   XmTextSetString(text_w, text);
   XSync(display, 0);
   XmUpdateDisplay(text_w);

   return;
}


/*++++++++++++++++++++++++ close_info_button() ++++++++++++++++++++++++++*/
static void
close_info_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(infoshell);

   return;
}
