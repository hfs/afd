/*
 *  show_ilog.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_ilog - displays the input log file from the AFD
 **
 ** SYNOPSIS
 **   show_ilog [--version]
 **                OR
 **   show_ilog [-w <AFD working directory>] [fontname] [hostname 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.05.1997 H.Kiehl Created
 **   15.02.1999 H.Kiehl Multiple recipients.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>            /* strcpy(), strcat(), strerror()         */
#include <ctype.h>             /* toupper()                              */
#include <signal.h>            /* signal()                               */
#include <stdlib.h>            /* free()                                 */
#include <sys/types.h>
#include <unistd.h>            /* gethostname()                          */
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include "afd_ctrl.h"
#include "show_ilog.h"
#include "permission.h"
#include "version.h"

/* Global variables */
Display                    *display;
XtAppContext               app;
Widget                     toplevel_w,
                           start_time_w,
                           end_time_w,
                           file_name_w,
                           directory_w,
                           file_length_w,
                           recipient_w,
                           headingbox_w,
                           listbox_w,
                           scrollbar_w,
                           statusbox_w,
                           summarybox_w,
                           special_button_w;
Window                     main_window;
XmFontList                 fontlist;
int                        char_width,
                           file_name_length,
                           fra_fd = -1,
                           fra_id,
                           no_of_dirs = 0,
                           no_of_log_files,
                           no_of_search_hosts,
                           special_button_flag,
                           sys_log_fd = STDERR_FILENO;
#ifndef _NO_MMAP
off_t                      fra_size;
#endif
Dimension                  button_height;
time_t                     start_time_val,
                           end_time_val;
size_t                     search_file_size;
char                       *p_work_dir,
                           font_name[256],
                           search_file_name[MAX_PATH_LENGTH],
                           search_directory_name[MAX_PATH_LENGTH],
                           **search_recipient,
                           **search_user;
struct item_list           *il;
struct sol_perm            perm;
struct fileretrieve_status *fra;

/* Local function prototypes */
static void                init_show_ilog(int *, char **),
                           eval_permissions(char *),
                           sig_bus(int),
                           sig_segv(int),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   *radio_label[] = { "Short", "Long" };
   static String   fallback_res[] =
                   {
                      ".show_ilog*mwmDecorations : 42",
                      ".show_ilog*mwmFunctions : 12",
                      ".show_ilog*background : NavajoWhite2",
                      ".show_ilog.mainform*background : NavajoWhite2",
                      ".show_ilog.mainform*XmText.background : NavajoWhite1",
                      ".show_ilog.mainform*listbox.background : NavajoWhite1",
                      ".show_ilog.mainform.buttonbox*background : PaleVioletRed2",
                      ".show_ilog.mainform.buttonbox*foreground : Black",
                      ".show_ilog.mainform.buttonbox*highlightColor : Black",
                      ".show_ilog.show_info*mwmDecorations : 10",
                      ".show_ilog.show_info*mwmFunctions : 4",
                      ".show_ilog.show_info*background : NavajoWhite2",
                      ".show_ilog.show_info*XmText.background : NavajoWhite1",
                      ".show_ilog.show_info.infoform.buttonbox*background : PaleVioletRed2",
                      ".show_ilog.show_info.infoform.buttonbox*foreground : Black",
                      ".show_ilog.show_info.infoform.buttonbox*highlightColor : Black",
                      ".show_ilog.Print Data*background : NavajoWhite2",
                      ".show_ilog.Print Data*XmText.background : NavajoWhite1",
                      ".show_ilog.Print Data.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_ilog.Print Data.main_form.buttonbox*foreground : Black",
                      ".show_ilog.Print Data.main_form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          block_w,
                   buttonbox_w,
                   button_w,
                   criteriabox_w,
                   currenttime_w,
                   entertime_w,
                   label_w,
                   mainform_w,
                   radio_w,
                   radiobox_w,
                   rowcol_w,
                   separator_w,
                   timebox_w;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XFontStruct     *font_struct;
   XmFontType      dummy;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_show_ilog(&argc, argv);
   (void)strcpy(window_title, "Input Log ");
   if (get_afd_name(&window_title[10]) == INCORRECT)
   {
      if (gethostname(&window_title[10], MAX_AFD_NAME_LENGTH) == 0)
      {
         window_title[0] = toupper((int)window_title[0]);
      }
   }
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel_w = XtAppInitialize(&app, "AFD", NULL, 0,
                                &argc, argv, fallback_res, args, argcount);
   display = XtDisplay(toplevel_w);

   /* Create managing widget */
   mainform_w = XmCreateForm(toplevel_w, "mainform", NULL, 0);

   /* Prepare font */
   if ((entry = XmFontListEntryLoad(XtDisplay(mainform_w), font_name,
                                    XmFONT_IS_FONT, "TAG1")) == NULL)
   {
       if ((entry = XmFontListEntryLoad(XtDisplay(mainform_w), "fixed",
                                        XmFONT_IS_FONT, "TAG1")) == NULL)
       {
          (void)fprintf(stderr,
                        "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
          exit(INCORRECT);
       }
   }
   font_struct = (XFontStruct *)XmFontListEntryGetFont(entry, &dummy);
   char_width  = font_struct->per_char->width;
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                           Time Box                                    */
/*                           --------                                    */
/* Start and end time to search ouput log file. If no time is entered it */
/* means we should search through all log files.                         */
/*-----------------------------------------------------------------------*/
   /* Create managing widget to enter the time interval. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   timebox_w = XmCreateForm(mainform_w, "timebox", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   entertime_w = XmCreateForm(timebox_w, "entertime", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol",
                           xmRowColumnWidgetClass, entertime_w,
                           XmNorientation,         XmHORIZONTAL,
                           NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget(" Start time :",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   start_time_w = XtVaCreateManagedWidget("starttime",
                           xmTextWidgetClass,   block_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNcolumns,          8,
                           XmNmaxLength,        8,
                           NULL);
   XtAddCallback(start_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)START_TIME_NO_ENTER);
   XtAddCallback(start_time_w, XmNactivateCallback, save_input,
                 (XtPointer)START_TIME);
   XtManageChild(block_w);

   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("End time :",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   end_time_w = XtVaCreateManagedWidget("endtime",
                           xmTextWidgetClass,   block_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNcolumns,          8,
                           XmNmaxLength,        8,
                           NULL);
   XtAddCallback(end_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)END_TIME_NO_ENTER);
   XtAddCallback(end_time_w, XmNactivateCallback, save_input,
                 (XtPointer)END_TIME);
   XtManageChild(block_w);
   XtManageChild(rowcol_w);
   XtManageChild(entertime_w);

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       entertime_w);
   argcount++;
   separator_w = XmCreateSeparator(timebox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   currenttime_w = XtVaCreateManagedWidget("",
                           xmLabelWidgetClass,  timebox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       separator_w,
                           NULL);
   XtManageChild(timebox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       timebox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                          Criteria Box                                 */
/*                          ------------                                 */
/* Here more search parameters can be entered, such as: file name,       */
/* length of the file, directory from which the file had its origion,    */
/* recipient of the file.                                                */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other criteria. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    49);
   argcount++;
   criteriabox_w = XmCreateForm(mainform_w, "criteriabox", args, argcount);

   label_w = XtVaCreateManagedWidget(" File name :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   24,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    8,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   file_name_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   24,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    29,
                           NULL);
   XtAddCallback(file_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)FILE_NAME_NO_ENTER);
   XtAddCallback(file_name_w, XmNactivateCallback, save_input,
                 (XtPointer)FILE_NAME);

   label_w = XtVaCreateManagedWidget(" Directory :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      25,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   48,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    8,
                           NULL);
   directory_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      25,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   48,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    29,
                           NULL);
   XtAddCallback(directory_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DIRECTORY_NAME_NO_ENTER);
   XtAddCallback(directory_w, XmNactivateCallback, save_input,
                 (XtPointer)DIRECTORY_NAME);

   label_w = XtVaCreateManagedWidget("Length :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   24,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     26,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    36,
                           NULL);
   file_length_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   24,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    48,
                           NULL);
   XtAddCallback(file_length_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)FILE_LENGTH_NO_ENTER);
   XtAddCallback(file_length_w, XmNactivateCallback, save_input,
                 (XtPointer)FILE_LENGTH);

   label_w = XtVaCreateManagedWidget("Recipient :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      25,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   48,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     26,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    36,
                           NULL);
   recipient_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      25,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   48,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    48,
                           NULL);
   XtAddCallback(recipient_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RECIPIENT_NAME_NO_ENTER);
   XtAddCallback(recipient_w, XmNactivateCallback, save_input,
                 (XtPointer)RECIPIENT_NAME);
   XtManageChild(criteriabox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       criteriabox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                             Radio Box                                 */
/*                             ---------                                 */
/* To select if the output in the list widget should be in long or short */
/* format. Default is short, since this is the fastest form.             */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,         XmPACK_COLUMN);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,      1);
   argcount++;
   radiobox_w = XmCreateRadioBox(mainform_w, "radiobox", args, argcount);
   radio_w = XtVaCreateManagedWidget(radio_label[0],
                           xmToggleButtonGadgetClass, radiobox_w,
                           XmNfontList,               fontlist,
                           XmNset,                    True,
                           NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)SHOW_SHORT_FORMAT);
   radio_w = XtVaCreateManagedWidget(radio_label[1],
                           xmToggleButtonGadgetClass, radiobox_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)SHOW_LONG_FORMAT);
   XtManageChild(radiobox_w);
   file_name_length = SHOW_SHORT_FORMAT;

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       radiobox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                           Heading Box                                 */
/*                           -----------                                 */
/* Shows a heading for the list box.                                     */
/*-----------------------------------------------------------------------*/
   headingbox_w = XtVaCreateWidget("headingbox",
                           xmTextWidgetClass,        mainform_w,
                           XmNfontList,              fontlist,
                           XmNleftAttachment,        XmATTACH_FORM,
                           XmNleftOffset,            2,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNrightOffset,           20,
                           XmNtopAttachment,         XmATTACH_WIDGET,
                           XmNtopWidget,             separator_w,
                           XmNmarginHeight,          1,
                           XmNmarginWidth,           2,
                           XmNshadowThickness,       1,
                           XmNrows,                  1,
                           XmNeditable,              False,
                           XmNcursorPositionVisible, False,
                           XmNhighlightThickness,    0,
                           XmNcolumns,               MAX_OUTPUT_LINE_LENGTH + file_name_length + 1,
                           NULL);
   XtManageChild(headingbox_w);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* The status of the output log is shown here. If eg. no files are found */
/* it will be shown here.                                                */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 31);
   argcount++;
   buttonbox_w = XmCreateForm(mainform_w, "buttonbox", args, argcount);
   special_button_w = XtVaCreateManagedWidget("Search",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         1,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        10,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(special_button_w, XmNactivateCallback,
                 (XtCallbackProc)search_button, 0);
   button_w = XtVaCreateManagedWidget("Print",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         11,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        20,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)print_button, 0);
   button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         21,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        30,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* The status of the output log is shown here. If eg. no files are found */
/* it will be shown here.                                                */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                        xmLabelWidgetClass,  mainform_w,
                        XmNfontList,         fontlist,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     separator_w,
                        NULL);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     statusbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                           Summary Box                                 */
/*                           -----------                                 */
/* Summary of what has been selected. If none is slected in listbox a    */
/* summary of all items is made.                                         */
/*-----------------------------------------------------------------------*/
   summarybox_w = XtVaCreateWidget("summarybox",
                        xmTextWidgetClass,        mainform_w,
                        XmNfontList,              fontlist,
                        XmNleftAttachment,        XmATTACH_FORM,
                        XmNleftOffset,            3,
                        XmNrightAttachment,       XmATTACH_FORM,
                        XmNrightOffset,           20,
                        XmNbottomAttachment,      XmATTACH_WIDGET,
                        XmNbottomWidget,          separator_w,
                        XmNmarginHeight,          1,
                        XmNmarginWidth,           1,
                        XmNshadowThickness,       1,
                        XmNrows,                  1,
                        XmNeditable,              False,
                        XmNcursorPositionVisible, False,
                        XmNhighlightThickness,    0,
                        NULL);
   XtManageChild(summarybox_w);

/*-----------------------------------------------------------------------*/
/*                             List Box                                  */
/*                             --------                                  */
/* This scrolled list widget shows the contents of the output log,       */
/* either in short or long form. Default is short.                       */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              headingbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           summarybox_w);
   argcount++;
   XtSetArg(args[argcount], XmNvisibleItemCount,       NO_OF_VISIBLE_LINES);
   argcount++;
   XtSetArg(args[argcount], XmNselectionPolicy,        XmEXTENDED_SELECT);
   argcount++;
   XtSetArg(args[argcount], XmNscrollBarDisplayPolicy, XmSTATIC);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   listbox_w = XmCreateScrolledList(mainform_w, "listbox", args, argcount);
   XtManageChild(listbox_w);
   XtAddEventHandler(listbox_w, ButtonPressMask, False,
                     (XtEventHandler)info_click, (XtPointer)NULL);
   XtAddCallback(listbox_w, XmNextendedSelectionCallback,
                 (XtCallbackProc)item_selection, 0);
   XtManageChild(mainform_w);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef _EDITRES
   XtAddEventHandler(toplevel_w, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Start clock */
   update_time((XtPointer)currenttime_w, (XtIntervalId)NULL);

   /* Realize all widgets */
   XtRealizeWidget(toplevel_w);

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(toplevel_w, WARN_DIALOG,
                 "Failed to set signal handler's for afd_ctrl : %s",
                 strerror(errno));
   }

   /* We want the keyboard focus on the start time */
   XmProcessTraversal(start_time_w, XmTRAVERSE_CURRENT);

#ifdef _WITH_FANCY_TRAVERSE
   /*
    * Only now my we activate the losing focus callback. If we
    * do it earlier, the start time will always be filled with
    * the current time. This is NOT what we want.
    */
   XtAddCallback(start_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)START_TIME);
#endif

   /* Get widget ID of the scrollbar. */
   XtVaGetValues(XtParent(listbox_w), XmNverticalScrollBar,
                 &scrollbar_w, NULL);
   XtAddCallback(scrollbar_w, XmNdragCallback,
                 (XtCallbackProc)scrollbar_moved, 0);
   XtVaGetValues(buttonbox_w, XmNheight, &button_height, NULL);

   /* Write heading. */
   if (file_name_length == SHOW_SHORT_FORMAT)
   {
      XmTextSetString(headingbox_w, HEADING_LINE_SHORT);
   }
   else
   {
      XmTextSetString(headingbox_w, HEADING_LINE_LONG);
   }

   if (no_of_search_hosts > 0)
   {
      char *str;

      if ((str = malloc(MAX_RECIPIENT_LENGTH * no_of_search_hosts)) != NULL)
      {
         int length,
             i;

         length = sprintf(str, "%s", search_recipient[0]);
         for (i = 1; i < no_of_search_hosts; i++)
         {
            length += sprintf(&str[length], ", %s", search_recipient[i]);
         }
         XtVaSetValues(recipient_w, XmNvalue, str, NULL);
         free(str);
      }
   }

   /* Get Window for resizing the main window. */
   main_window = XtWindow(toplevel_w);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_show_ilog() ++++++++++++++++++++++++++*/
static void
init_show_ilog(int *argc, char *argv[])
{
   char *perm_buffer;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (get_arg(argc, argv, "-f", font_name, 256) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE     : (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      eval_permissions(perm_buffer);
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      perm.view_passwd  = NO;
                      perm.list_limit   = NO_LIMIT;
                      break;

      default       : (void)fprintf(stderr,
                                    "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   /* Collect all hostnames */
   no_of_search_hosts = *argc - 1;
   if (no_of_search_hosts > 0)
   {
      int i = 0;

      RT_ARRAY(search_recipient, no_of_search_hosts,
               (MAX_RECIPIENT_LENGTH + 1), char);
      RT_ARRAY(search_user, no_of_search_hosts,
               (MAX_RECIPIENT_LENGTH + 1), char);
      while (*argc > 1)
      {
         (void)strcpy(search_recipient[i], argv[1]);
         if (strlen(search_recipient[i]) == MAX_HOSTNAME_LENGTH)
         {
            (void)strcat(search_recipient[i], "*");
         }
         (*argc)--; argv++;
         i++;
      }
      for (i = 0; i < no_of_search_hosts; i++)
      {
         search_user[i][0] = '\0';
      }
   }

   start_time_val = -1;
   end_time_val = -1;
   search_file_size = -1;
   search_file_name[0] = '\0';
   search_directory_name[0] = '\0';
   special_button_flag = SEARCH_BUTTON;
   no_of_log_files = 0;

   return;
}


/*---------------------------------- usage() ----------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s [-w <working directory>] [-f <font name>] [host name 1..n]\n",
                 progname);
   return;
}


/*-------------------------- eval_permissions() -------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   char *ptr;

   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l'))
   {
      perm.list_limit   = NO_LIMIT;
      perm.view_passwd  = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, SHOW_ILOG_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         free(perm_buffer);
         exit(INCORRECT);
      }
      else
      {
         /*
          * For future use. Allow to limit for host names
          * as well.
          */
         ptr--;
      }

      /* May he see the password when using info click? */
      if ((ptr = posi(perm_buffer, VIEW_PASSWD_PERM)) == NULL)
      {
         /* The user may NOT view the password. */
         perm.view_passwd = NO;
      }

      /* Is there a limit on how many items the user may view? */
      if ((ptr = posi(perm_buffer, LIST_LIMIT)) == NULL)
      {
         /* There is no limit. */
         perm.list_limit = NO_LIMIT;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            char *p_start = ptr + 1,
                 tmp_char;

            ptr++;
            while ((*ptr != ',') && (*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
            tmp_char = *ptr;
            *ptr = '\0';
            perm.list_limit = atoi(p_start);
            *ptr = tmp_char;
         }
         else
         {
            perm.list_limit = NO_LIMIT;
         }
      }
   }

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
