/*
 *  show_log.c - Part of AFD, an automatic file distribution program.
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   show_log - displays log files from the AFD
 **
 ** SYNOPSIS
 **   show_log [--version]
 **                OR
 **   show_log [-w <AFD working directory>] 
 **            [font name]
 **            System|Transfer|Debug|Monitor|Monsystem
 **            [hostname 1 hostname 2 ... hostname n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.03.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug toggle.
 **                      Remove the font button. Font now gets selected
 **                      via afd_ctrl.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>              /* gethostname(), STDERR_FILENO         */
#include <stdlib.h>              /* getenv()                             */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#ifdef _WITH_SCROLLBAR
#include <Xm/ScrollBar.h>
#else
#include <Xm/Scale.h>
#endif
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_log.h"
#include "mondefs.h"
#include "logdefs.h"
#include "version.h"
#include "cursor1.h"
#include "cursormask1.h"
#include "cursor2.h"
#include "cursormask2.h"

/* Global variables */
Display        *display;
XtAppContext   app;
XtIntervalId   interval_id_host;
XmTextPosition wpr_position;
Cursor         cursor1,
               cursor2;
Widget         toplevel,
               log_output,
               counterbox,
               log_scroll_bar,
               selectlog,
               selectscroll;
int            max_log_number,
               current_log_number,
               line_counter,
               log_type_flag,
               toggles_set,
               no_of_hosts,
               sys_log_fd = STDERR_FILENO;
unsigned int   toggles_set_parallel_jobs,
               total_length;
char           log_dir[MAX_PATH_LENGTH],
               log_type[MAX_FILENAME_LENGTH],
               log_name[MAX_FILENAME_LENGTH],
               work_dir[MAX_PATH_LENGTH],
               **hosts = NULL,
               font_name[40],
               *toggle_label[] =
               {
                  "Info",
                  "Config",
                  "Warn",
                  "Error",
                  "Debug"
               },
               *p_work_dir;
DIR            *dp;
FILE           *p_log_file;

/* Local functions */
static void    init_log_file(int *, char **),
               create_cursors(void),
               sig_bus(int),
               sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_SCROLLBAR
   int             slider_size;
#endif
   char            window_title[100],
                   hostname[MAX_AFD_NAME_LENGTH],
                   str_line[12];
   static String   fallback_res[] =
                   {
                      ".show_log*mwmDecorations : 110",
                      ".show_log*mwmFunctions : 30",
                      ".show_log.form.log_outputSW*XmText.fontList : fixed",
                      ".show_log*background : NavajoWhite2",
                      ".show_log.form.log_outputSW.log_output.background : NavajoWhite1",
                      ".show_log.form.counterbox*background : NavajoWhite1",
                      ".show_log.form.buttonbox*background : PaleVioletRed2",
                      ".show_log.form.buttonbox*foreground : Black",
                      ".show_log.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form,
                   togglebox,
                   button,
                   buttonbox,
                   v_separator1,
                   v_separator2,
                   h_separator1,
                   h_separator2,
                   toggle,
                   scalebox,
                   scalelabel;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XmFontList      fontlist;

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_log_file(&argc, argv);
   line_counter = 0;
   wpr_position = 0;
   total_length = 0;

   (void)strcpy(window_title, log_type);
   (void)strcat(window_title, " Log ");
   if (get_afd_name(hostname) == INCORRECT)
   {
      if (gethostname(hostname, MAX_AFD_NAME_LENGTH) == 0)
      {
         hostname[0] = toupper((int)hostname[0]);
         (void)strcat(window_title, hostname);
      }
   }
   else
   {
      (void)strcat(window_title, hostname);
   }
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);

   /* Create managing widget */
   form = XmCreateForm(toplevel, "form", NULL, 0);

   /* Prepare the font */
   entry = XmFontListEntryLoad(XtDisplay(toplevel), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /* Create managing widget for the toggle buttons */
   togglebox = XtVaCreateWidget("togglebox", xmRowColumnWidgetClass, form,
                                XmNorientation,      XmHORIZONTAL,
                                XmNpacking,          XmPACK_TIGHT,
                                XmNnumColumns,       1,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNresizable,        False,
                                NULL);

   /* Create toggle button widget */
   toggle = XtVaCreateManagedWidget(toggle_label[0],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_INFO);
   if ((log_type_flag == SYSTEM_LOG_TYPE) ||
       (log_type_flag == MON_SYSTEM_LOG_TYPE))
   {
      toggle = XtVaCreateManagedWidget(toggle_label[1],
                                       xmToggleButtonGadgetClass, togglebox,
                                       XmNfontList,               fontlist,
                                       XmNset,                    True,
                                       NULL);
      XtAddCallback(toggle, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_CONFIG);
   }
   toggle = XtVaCreateManagedWidget(toggle_label[2],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_WARN);
   toggle = XtVaCreateManagedWidget(toggle_label[3],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_ERROR);
   toggle = XtVaCreateManagedWidget(toggle_label[4],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    False,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_DEBUG);
   XtManageChild(togglebox);
   toggles_set = SHOW_INFO | SHOW_CONFIG | SHOW_WARN | SHOW_ERROR | SHOW_FATAL;

   /* Create the first horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             togglebox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   /* Create the first vertical separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       togglebox);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
   XtManageChild(v_separator1);

   if ((log_type_flag == TRANSFER_LOG_TYPE) ||
       (log_type_flag == TRANS_DB_LOG_TYPE))
   {
#ifdef _TOGGLED_PROC_SELECTION
      int  i;
      char label[MAX_INT_LENGTH];

      /* Create managing widget for the second toggle buttons */
      togglebox = XtVaCreateWidget("togglebox2",
                             xmRowColumnWidgetClass, form,
                             XmNorientation,      XmHORIZONTAL,
                             XmNpacking,          XmPACK_TIGHT,
                             XmNnumColumns,       1,
                             XmNtopAttachment,    XmATTACH_FORM,
                             XmNleftAttachment,   XmATTACH_WIDGET,
                             XmNleftWidget,       v_separator1,
                             XmNresizable,        False,
                             NULL);

      (signed int)toggles_set_parallel_jobs = -1;
      for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
      {
         /* Create toggle button widget */
         (void)sprintf(label, "%d", i);
         toggle = XtVaCreateManagedWidget(label,
                             xmToggleButtonGadgetClass, togglebox,
                             XmNfontList,               fontlist,
                             XmNset,                    True,
                             NULL);
         XtAddCallback(toggle, XmNvalueChangedCallback,
                       (XtCallbackProc)toggled_jobs, (XtPointer)i);
      }
      XtManageChild(togglebox);

      /* Create the second vertical separator */
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       togglebox);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
      XtManageChild(v_separator1);
#else
      int      i;
      char     proc_name[MAX_INT_LENGTH];
      Widget   box_w,
               button_w,
               option_menu_w,
               pane_w;
      XmString label;

      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       v_separator1);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      box_w = XmCreateForm(form, "button_box", args, argcount);

      /* Create a pulldown pane and attach it to the option menu */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList,         fontlist);
      argcount++;
      pane_w = XmCreatePulldownMenu(box_w, "pane", args, argcount);

      label = XmStringCreateLocalized("Proc");
      argcount = 0;
      XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
      argcount++;
      XtSetArg(args[argcount], XmNlabelString,      label);
      argcount++;
      XtSetArg(args[argcount], XmNfontList,         fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomOffset,     -2);
      argcount++;
      option_menu_w = XmCreateOptionMenu(box_w, "proc_selection",
                                         args, argcount);
      XtManageChild(option_menu_w);
      XmStringFree(label);

      /* Add all possible buttons. */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      button_w = XtCreateManagedWidget("all",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, toggled_jobs,
                    (XtPointer)0);
      for (i = 1; i < (MAX_NO_PARALLEL_JOBS + 1); i++)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNfontList, fontlist);
         argcount++;
         (void)sprintf(proc_name, "%d", i - 1);
         button_w = XtCreateManagedWidget(proc_name,
                                          xmPushButtonWidgetClass,
                                          pane_w, args, argcount);

         /* Add callback handler */
         XtAddCallback(button_w, XmNactivateCallback, toggled_jobs,
                       (XtPointer)i);
      }
      toggles_set_parallel_jobs = 0; /* Default to 'all' */
      XtManageChild(box_w);

      /* Create the second vertical separator */
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       box_w);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
      XtManageChild(v_separator1);
#endif
   }

   /* Create line counter box */
   counterbox = XtVaCreateWidget("counterbox",
                        xmTextWidgetClass,        form,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNrightAttachment,       XmATTACH_FORM,
                        XmNtopOffset,             6,
                        XmNrightOffset,           5,
                        XmNfontList,              fontlist,
                        XmNrows,                  1,
                        XmNcolumns,               MAX_LINE_COUNTER_DIGITS,
                        XmNeditable,              False,
                        XmNcursorPositionVisible, False,
                        XmNmarginHeight,          1,
                        XmNmarginWidth,           1,
                        XmNshadowThickness,       1,
                        XmNhighlightThickness,    0,
                        NULL);
   XtManageChild(counterbox);

   /* Create the second vertical separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNrightWidget,      counterbox);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      2);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator2 = XmCreateSeparator(form, "v_separator2", args, argcount);
   XtManageChild(v_separator2);

   /* Create scale widget for selecting the log file number */
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNrightWidget,      v_separator2);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      2);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   scalebox = XmCreateForm(form, "scalebox", args, argcount);
#ifdef _WITH_SCROLLBAR
   scalelabel = XtVaCreateManagedWidget("Log file:",
                        xmLabelGadgetClass, scalebox,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       2,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   str_line[0] = '0';
   str_line[1] = '\0';
   selectlog = XtVaCreateWidget(str_line,
                        xmLabelGadgetClass,       scalebox,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNtopOffset,             1,
                        XmNrightAttachment,       XmATTACH_FORM,
                        XmNrightOffset,           2,
                        XmNleftAttachment,        XmATTACH_WIDGET,
                        XmNleftWidget,            scalelabel,
                        XmNfontList,              fontlist,
                        XmNalignment,             XmALIGNMENT_END,
                        NULL);
   XtManageChild(selectlog);
   if ((slider_size = ((max_log_number + 1) / 10)) < 1)
   {
      slider_size = 1;
   }
   selectscroll = XtVaCreateManagedWidget("selectscroll",
                        xmScrollBarWidgetClass, scalebox,
                        XmNmaximum,         max_log_number + slider_size,
                        XmNminimum,         0,
                        XmNsliderSize,      slider_size,
                        XmNvalue,           0,
                        XmNincrement,       1,
                        XmNfontList,        fontlist,
                        XmNheight,          10,
                        XmNtopOffset,       1,
                        XmNorientation,     XmHORIZONTAL,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       selectlog,
                        XmNleftAttachment,  XmATTACH_WIDGET,
                        XmNleftWidget,      scalelabel,
                        NULL);
   XtAddCallback(selectscroll, XmNvalueChangedCallback, slider_moved,
                 (XtPointer)&current_log_number);
   XtAddCallback(selectscroll, XmNdragCallback, slider_moved,
                 (XtPointer)&current_log_number);
#else
   scalelabel = XtVaCreateManagedWidget("Log file:",
                        xmLabelGadgetClass,  scalebox,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       2,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   selectscroll = XtVaCreateManagedWidget("selectscroll",
                        xmScaleWidgetClass,    scalebox,
                        XmNmaximum,            max_log_number,
                        XmNminimum,            0,
                        XmNvalue,              0,
                        XmNshowValue,          True,
                        XmNorientation,        XmHORIZONTAL,
                        XmNfontList,           fontlist,
                        XmNhighlightThickness, 0,
                        XmNscaleHeight,        10,
                        XmNtopAttachment,      XmATTACH_FORM,
                        XmNtopOffset,          3,
                        XmNbottomAttachment,   XmATTACH_FORM,
                        XmNbottomOffset,       4,
                        XmNrightAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,     XmATTACH_WIDGET,
                        XmNleftWidget,         scalelabel,
                        XmNleftOffset,         2,
                        NULL);
#endif /* _WITH_SCROLLBAR */
   XtManageChild(scalebox);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 21);
   argcount++;
   buttonbox = XmCreateForm(form, "buttonbox", args, argcount);
   button = XtVaCreateManagedWidget("Update",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     1,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    10,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)update_button, 0);
   button = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     11,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    20,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox);

   /* Create the second horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator2 = XmCreateSeparator(form, "h_separator2", args, argcount);
   XtManageChild(h_separator2);

   /* Create log_text as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   9);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       True);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2);
   argcount++;
   log_output = XmCreateScrolledText(form, "log_output", args, argcount);
   XtManageChild(log_output);
   XtManageChild(form);

#ifdef _EDITRES
   XtAddEventHandler(toplevel, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel);
   wait_visible(toplevel);


   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(toplevel, WARN_DIALOG,
                 "Failed to set signal handler's for afd_ctrl : %s",
                 strerror(errno));
   }

   /* Create pixmaps for cursor */
   create_cursors();

   /*
    * Get widget id of the scroll bar in the scrolled text widget.
    */
   XtVaGetValues(XtParent(log_output), XmNverticalScrollBar,
                 &log_scroll_bar, NULL);

   init_text();

   /* Call check_log() after LOG_START_TIMEOUT ms */
   interval_id_host = XtAppAddTimeOut(app, LOG_START_TIMEOUT,
                                      (XtTimerCallbackProc)check_log,
                                      log_output);

   /* Show line_counter if necessary */
   if (line_counter == 0)
   {
      (void)sprintf(str_line, "%*d",
                    MAX_LINE_COUNTER_DIGITS, line_counter);
      XmTextSetString(counterbox, str_line);
   }

   /* We want the keyboard focus on the log output */
   XmProcessTraversal(log_output, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_log_file() +++++++++++++++++++++++++++*/
static void
init_log_file(int *argc, char *argv[])
{
   char log_file[MAX_PATH_LENGTH];

   /* Get display pointer */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open Display.\n");
      exit(INCORRECT);
   }

   /* First get working directory for the AFD */
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   if (*argc < 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [nummeric font name] System|Transfer|Debug|Monitor|Monsystem [hostname 1..n]\n",
                    argv[0]);
      exit(INCORRECT);
   }
   if ((*argc > 1) && (isdigit(argv[1][0]) != 0))
   {
      (void)strcpy(font_name, argv[1]);
      (*argc)--; argv++;
   }
   else
   {
      (void)strcpy(font_name, "fixed");
   }
   (void)strcpy(log_type, argv[1]);
   (*argc)--; argv++;

   /* Collect all hostnames */
   no_of_hosts = *argc - 1;
   if (no_of_hosts > 0)
   {
      int i = 0;

      RT_ARRAY(hosts, no_of_hosts, (MAX_AFDNAME_LENGTH + 1), char);
      while (*argc > 1)
      {
         (void)strcpy(hosts[i], argv[1]);
         (*argc)--; argv++;
         i++;
      }
   }

   /* Initialise log directory */
   (void)strcpy(log_dir, work_dir);
   (void)strcat(log_dir, LOG_DIR);
   if ((dp = opendir(log_dir)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not opendir() %s : %s (%s %d)\n",
                    log_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (strcmp(log_type, SYSTEM_STR) == 0)
   {
      (void)strcpy(log_name, SYSTEM_LOG_NAME);
      max_log_number = (MAX_SYSTEM_LOG_FILES - 1);
      log_type_flag = SYSTEM_LOG_TYPE;
   }
   else if (strcmp(log_type, TRANSFER_STR) == 0)
        {
           (void)strcpy(log_name, TRANSFER_LOG_NAME);
           max_log_number = (MAX_TRANSFER_LOG_FILES - 1);
           log_type_flag = TRANSFER_LOG_TYPE;
        }
   else if (strcmp(log_type, TRANS_DB_STR) == 0)
        {
           (void)strcpy(log_name, TRANS_DB_LOG_NAME);
           max_log_number = (MAX_TRANS_DB_LOG_FILES - 1);
           log_type_flag = TRANS_DB_LOG_TYPE;
        }
   else if (strcmp(log_type, MON_SYSTEM_STR) == 0)
        {
           (void)strcpy(log_name, MON_SYS_LOG_NAME);
           max_log_number = (MAX_MON_SYS_LOG_FILES - 1);
           log_type_flag = MON_SYSTEM_LOG_TYPE;
        }
   else if (strcmp(log_type, MONITOR_STR) == 0)
        {
           (void)strcpy(log_name, MON_LOG_NAME);
           max_log_number = (MAX_MON_LOG_FILES - 1);
           log_type_flag = MONITOR_LOG_TYPE;
        }
        else
        {
            (void)fprintf(stderr, "ERROR   : Unknown log type %s (%s %d)\n",
                          log_type, __FILE__, __LINE__);
            exit(INCORRECT);
        }

   (void)sprintf(log_file, "%s/%s0", log_dir, log_name);
   current_log_number = 0;

   if ((p_log_file = fopen(log_file, "r")) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not fopen() %s : %s (%s %d)\n",
                    log_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}


/*+++++++++++++++++++++++++++ create_cursors() ++++++++++++++++++++++++++*/
static void
create_cursors(void)
{
   XColor fg,
          bg;
   Pixmap cursorsrc,
          cursormask;

   cursorsrc = XCreateBitmapFromData(display,
                                     XtWindow(toplevel),
                                     (char *)cursor1_bits,
                                     cursor1_width, cursor1_height);
   cursormask = XCreateBitmapFromData(display,
                                      XtWindow(toplevel),
                                      (char *)cursormask1_bits,
                                      cursormask1_width, cursormask1_height);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "black", &fg);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "white", &bg);
   cursor1 = XCreatePixmapCursor(display, cursorsrc, cursormask, &fg, &bg,
                                 cursor1_x_hot, cursor1_y_hot);
   XFreePixmap(display, cursorsrc); XFreePixmap(display, cursormask);

   cursorsrc = XCreateBitmapFromData(display,
                                     XtWindow(toplevel),
                                     (char *)cursor2_bits,
                                     cursor2_width, cursor2_height);
   cursormask = XCreateBitmapFromData(display,
                                      XtWindow(toplevel),
                                      (char *)cursormask2_bits,
                                      cursormask2_width, cursormask2_height);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "black", &fg);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "white", &bg);
   cursor2 = XCreatePixmapCursor(display, cursorsrc, cursormask, &fg, &bg,
                                 cursor2_x_hot, cursor2_y_hot);
   XFreePixmap(display, cursorsrc); XFreePixmap(display, cursormask);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);                   
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{                 
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}
