/*
 *  dir_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dir_info - displays information on a single AFD
 **
 ** SYNOPSIS
 **   dir_info [--version] [-w <work dir>] [-f <font name>] -d <directory-name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <time.h>                /* strftime(), localtime()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>              /* getcwd(), gethostname()              */
#include <sys/mman.h>
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
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <errno.h>
#include "dir_info.h"
#include "version.h"
#include "permission.h"

/* Global variables */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_dir;
Widget                     dirname_text_w,
                           toplevel_w,
                           text_wl[NO_OF_DIR_ROWS],
                           text_wr[NO_OF_DIR_ROWS],
                           label_l_widget[NO_OF_DIR_ROWS],
                           label_r_widget[NO_OF_DIR_ROWS],
                           url_text_w;
int                        dir_position = -1,
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs,
                           sys_log_fd = STDERR_FILENO,
                           view_passwd;
off_t                      fra_size;
char                       dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
                           font_name[40],
                           *p_work_dir,
                           label_l[NO_OF_DIR_ROWS][23] =
                           {
                              "Alias directory name :",
                              "Stupid mode          :",
                              "Force reread         :",
                              "Old file time (hours):",
                              "Bytes received       :",
                              "Last retrieval       :"
                           },
                           label_r[NO_OF_DIR_ROWS][22] =
                           {
                              "Priority            :",
                              "Remove files        :",
                              "Report unknown files:",
                              "Delete unknown files:",
                              "Files received      :",
                              "Next check time     :"
                           };
struct fileretrieve_status *fra;
struct prev_values         prev;

/* Local function prototypes */
static void                init_dir_info(int *, char **),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             i;
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   str_line[MAX_INFO_STRING_LENGTH + 1],
                   tmp_str_line[MAX_INFO_STRING_LENGTH + 1],
                   yesno[4];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".dir_info.form*background : NavajoWhite2",
                      ".dir_info.form.dir_box.?.?.?.text_wl.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.?.text_wr.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.dirname_text_w.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.url_text_w.background : NavajoWhite1",
                      ".dir_info.form.buttonbox*background : PaleVioletRed2",
                      ".dir_info.form.buttonbox*foreground : Black",
                      ".dir_info.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form_w,
                   dir_box_w,
                   dir_box1_w,
                   dir_box2_w,
                   dir_text_w,
                   button_w,
                   buttonbox_w,
                   label_w,
                   rowcol1_w,
                   rowcol2_w,
                   h_separator1_w,
                   v_separator_w;
   XmFontListEntry entry;
   XmFontList      fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_dir_info(&argc, argv);

   (void)strcpy(window_title, dir_alias);
   (void)strcat(window_title, " Info");
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel_w = XtAppInitialize(&app, "AFD", NULL, 0,
                                &argc, argv, fallback_res, args, argcount);
   display = XtDisplay(toplevel_w);

   /* Create managing widget */
   form_w = XmCreateForm(toplevel_w, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /*---------------------------------------------------------------*/
   /*          Real directory name and if required URL              */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   dir_box_w = XmCreateForm(form_w, "dir_box", args, argcount);
   XtManageChild(dir_box_w);

   rowcol1_w = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                                 dir_box_w, NULL);
   dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                  rowcol1_w,
                                  XmNfractionBase, 41,
                                  NULL);
   label_w = XtVaCreateManagedWidget("Real directory name :",
                           xmLabelGadgetClass,  dir_text_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   dirname_text_w = XtVaCreateManagedWidget("dirname_text_w",
                           xmTextWidgetClass,        dir_text_w,
                           XmNfontList,              fontlist,
                           XmNcolumns,               MAX_INFO_STRING_LENGTH,
                           XmNtraversalOn,           False,
                           XmNeditable,              False,
                           XmNcursorPositionVisible, False,
                           XmNmarginHeight,          1,
                           XmNmarginWidth,           1,
                           XmNshadowThickness,       1,
                           XmNhighlightThickness,    0,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNleftAttachment,        XmATTACH_POSITION,
                           XmNleftPosition,          12,
                           NULL);
   XtManageChild(dir_text_w);
   if (prev.host_alias[0] != '\0')
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                     rowcol1_w,
                                     XmNfractionBase, 41,
                                     NULL);
      label_w = XtVaCreateManagedWidget("URL                 :",
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      url_text_w = XtVaCreateManagedWidget("url_text_w",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               MAX_INFO_STRING_LENGTH,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_POSITION,
                              XmNleftPosition,          12,
                              NULL);
      XtManageChild(dir_text_w);
      (void)sprintf(str_line, "%*s", MAX_INFO_STRING_LENGTH, prev.display_url);
      XmTextSetString(url_text_w, str_line);
   }
   XtManageChild(rowcol1_w);
   (void)sprintf(str_line, "%*s", MAX_INFO_STRING_LENGTH,
                 prev.real_dir_name);
   XmTextSetString(dirname_text_w, str_line);

   /* Create the horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       dir_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,      h_separator1_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   dir_box_w = XmCreateForm(form_w, "dir_box", args, argcount);
   XtManageChild(dir_box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   dir_box1_w = XmCreateForm(dir_box_w, "dir_box1", args, argcount);
   XtManageChild(dir_box1_w);

   rowcol1_w = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                                 dir_box1_w, NULL);
   for (i = 0; i < NO_OF_DIR_ROWS; i++)
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                     rowcol1_w,
                                     XmNfractionBase, 41,
                                     NULL);
      label_l_widget[i] = XtVaCreateManagedWidget(label_l[i],
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wl[i] = XtVaCreateManagedWidget("text_wl",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               DIR_INFO_LENGTH_L,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_POSITION,
                              XmNleftPosition,          22,
                              NULL);
      XtManageChild(dir_text_w);
   }
   XtManageChild(rowcol1_w);

   /* Fill up the text widget with some values */
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, prev.dir_alias);
   XmTextSetString(text_wl[0], str_line);
   if (prev.stupid_mode == YES)
   {
      yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
   }
   else
   {
      yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
   }
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, yesno);
   XmTextSetString(text_wl[1], str_line);
   if (prev.force_reread == YES)
   {
      yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
   }
   else
   {
      yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
   }
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, yesno);
   XmTextSetString(text_wl[2], str_line);
   (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L, prev.old_file_time / 3600);
   XmTextSetString(text_wl[3], str_line);
   (void)sprintf(str_line, "%*lu", DIR_INFO_LENGTH_L, prev.bytes_received);
   XmTextSetString(text_wl[4], str_line);
   (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                  localtime(&prev.last_retrieval));
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, tmp_str_line);
   XmTextSetString(text_wl[5], str_line);

   /* Create the horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       dir_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);

   /* Create the vertical separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       dir_box1_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(dir_box_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   dir_box2_w = XmCreateForm(dir_box_w, "dir_box2", args, argcount);
   XtManageChild(dir_box2_w);

   rowcol2_w = XtVaCreateWidget("rowcol2", xmRowColumnWidgetClass,
                                dir_box2_w, NULL);
   for (i = 0; i < NO_OF_DIR_ROWS; i++)
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                    rowcol2_w,
                                    XmNfractionBase, 41,
                                    NULL);
      label_r_widget[i] = XtVaCreateManagedWidget(label_r[i],
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wr[i] = XtVaCreateManagedWidget("text_wr",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               DIR_INFO_LENGTH_R,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_WIDGET,
                              XmNleftWidget,            label_r_widget[i],
                              NULL);
      XtManageChild(dir_text_w);
   }
   XtManageChild(rowcol2_w);

   /* Fill up the text widget with some values */
   (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_R, prev.priority - '0');
   XmTextSetString(text_wr[0], str_line);
   if (prev.remove == YES)
   {
      yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
   }
   else
   {
      yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
   }
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
   XmTextSetString(text_wr[1], str_line);
   if (prev.report_unknown_files == YES)
   {
      yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
   }
   else
   {
      yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
   }
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
   XmTextSetString(text_wr[2], str_line);
   if (prev.delete_unknown_files == YES)
   {
      yesno[0] = 'Y'; yesno[1] = 'e'; yesno[2] = 's'; yesno[3] = '\0';
   }
   else
   {
      yesno[0] = 'N'; yesno[1] = 'o'; yesno[2] = '\0';
   }
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, yesno);
   XmTextSetString(text_wr[3], str_line);
   (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.files_received);
   XmTextSetString(text_wr[4], str_line);
   if (prev.time_option == YES)
   {
      (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     localtime(&prev.next_check_time));
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No time entry.");
   }
   XmTextSetString(text_wr[5], str_line);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator1_w);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);

   button_w = XtVaCreateManagedWidget("Close",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNfontList,         fontlist,
                                      XmNtopAttachment,    XmATTACH_POSITION,
                                      XmNtopPosition,      2,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition,   19,
                                      XmNleftAttachment,   XmATTACH_POSITION,
                                      XmNleftPosition,     1,
                                      XmNrightAttachment,  XmATTACH_POSITION,
                                      XmNrightPosition,    20,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);
   XtManageChild(form_w);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef _EDITRES
   XtAddEventHandler(toplevel_w, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel_w);
   wait_visible(toplevel_w);

   /* Call update_info() after UPDATE_INTERVAL ms */
   interval_id_dir = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      form_w);

   /* We want the keyboard focus on the Done button */
   XmProcessTraversal(button_w, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_dir_info() ++++++++++++++++++++++++++*/
static void
init_dir_info(int *argc, char *argv[])
{
   int                 count = 0,
                       dnb_fd,
                       i;
   char                dir_name_file[MAX_PATH_LENGTH],
                       *perm_buffer,
                       *ptr;
   struct stat         stat_buf;
   struct dir_name_buf *dnb;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }
   if (get_arg(argc, argv, "-d", dir_alias,
               MAX_DIR_ALIAS_LENGTH + 1) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   while (count--)
   {
      argc++;
   }

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') && ((perm_buffer[3] == '\0') ||
             (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
         {
            view_passwd = YES;
         }
         else
         {
            /*
             * First of all check if the user may use this program
             * at all.
             */
            if (posi(perm_buffer, DIR_INFO_PERM) == NULL)
            {
               (void)fprintf(stderr, "%s\n",
                             PERMISSION_DENIED_STR);
               free(perm_buffer);
               exit(INCORRECT);
            }

            /* May he see the password? */
            if (posi(perm_buffer, VIEW_PASSWD_PERM) == NULL)
            {
               /* The user may NOT view the password. */
               view_passwd = NO;
            }
            else
            {
               view_passwd = YES;
            }
         }
         free(perm_buffer);
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         view_passwd  = NO;                            
         break;

      default :
         (void)fprintf(stderr,
                       "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* Attach to the FRA */
   if (fra_attach() < 0)
   {
      (void)fprintf(stderr, "Failed to attach to FRA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if (strcmp(fra[i].dir_alias, dir_alias) == 0)
      {
         dir_position = i;
         break;
      }
   }
   if (dir_position < 0)
   {
      (void)fprintf(stderr,
                    "WARNING : Could not find directory %s in FRA. (%s %d)\n",
                    dir_alias, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Map to directory name buffer. */
   (void)sprintf(dir_name_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((dnb_fd = open(dir_name_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (fstat(dnb_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
      (void)close(dnb_fd);
      exit(INCORRECT);
   }
   if (stat_buf.st_size > 0)
   {
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         exit(INCORRECT);
      }
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   else
   {
      (void)fprintf(stderr, "Job ID database file is empty. (%s %d)\n",
                    __FILE__, __LINE__);
      (void)close(dnb_fd);
      exit(INCORRECT);
   }

   /* Initialize values in FRA structure */
   prev.dir_pos              = fra[dir_position].dir_pos;
   (void)strcpy(prev.real_dir_name, dnb[prev.dir_pos].dir_name);
   (void)strcpy(prev.host_alias, fra[dir_position].host_alias);
   (void)strcpy(prev.dir_alias, fra[dir_position].dir_alias);
   (void)strcpy(prev.url, fra[dir_position].url);
   (void)strcpy(prev.display_url, prev.url);
   if (view_passwd != YES)
   {
      remove_passwd(prev.display_url);
   }
   prev.priority             = fra[dir_position].priority;
   prev.stupid_mode          = fra[dir_position].stupid_mode;
   prev.remove               = fra[dir_position].remove;
   prev.force_reread         = fra[dir_position].force_reread;
   prev.report_unknown_files = fra[dir_position].report_unknown_files;
   prev.old_file_time        = fra[dir_position].old_file_time;
   prev.delete_unknown_files = fra[dir_position].delete_unknown_files;
   prev.bytes_received       = fra[dir_position].bytes_received;
   prev.files_received       = fra[dir_position].files_received;
   prev.last_retrieval       = fra[dir_position].last_retrieval;
   prev.next_check_time      = fra[dir_position].next_check_time;
   prev.time_option          = fra[dir_position].time_option;

   if (close(dnb_fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
   }
   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr, "Failed to munmap() from %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -d <dir-alias>\n", progname);
   (void)fprintf(stderr, "           --version\n");
   (void)fprintf(stderr, "           -f <font name>]\n");
   (void)fprintf(stderr, "           -w <working directory>]\n");
   return;
}