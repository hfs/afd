/*
 *  get_send_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999, 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_send_data - 
 **
 ** SYNOPSIS
 **   void get_send_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.08.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf(), popen(), pclose()         */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* malloc(), atoi()                     */
#include <ctype.h>               /* isdigit()                            */
#include <unistd.h>              /* write(), close()                     */
#include <time.h>                /* ctime(), strftime()                  */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <X11/keysym.h>
#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "afd_ctrl.h"
#include "show_olog.h"
#include "ftpdefs.h"
#include "smtpdefs.h"

/* Global variables */
Widget           log_output,
                 send_statusbox_w;
XmTextPosition   wpr_position;
struct send_data *db = NULL;

/* External global variables */
extern Display   *display;
extern Widget    listbox_w,
                 statusbox_w,
                 summarybox_w,
                 toplevel_w;
extern char      font_name[],
                 search_file_name[],
                 search_directory_name[],
                 search_recipient[],
                 search_file_size_str[],
                 summary_str[],
                 total_summary_str[];
extern int       items_selected,
                 toggles_set,
                 file_name_length;
extern time_t    start_time_val,
                 end_time_val;

/* Local global variables */
static Widget    hostname_label_w,
                 hostname_w,
                 lock_box_w,
                 mode_box_w,
                 password_label_w,
                 password_w,
                 port_label_w,
                 port_w,
                 prefix_w,
                 sf_shell = (Widget)NULL,
                 target_dir_label_w,
                 target_dir_w,
                 timeout_label_w,
                 timeout_w,
                 user_name_label_w,
                 user_name_w;

/* Local function prototypes. */
static void      cancel_send_button(Widget, XtPointer, XtPointer),
                 debug_toggle(Widget, XtPointer, XtPointer),
                 enter_passwd(Widget, XtPointer, XtPointer),
                 lock_radio(Widget, XtPointer, XtPointer),
                 mode_radio(Widget, XtPointer, XtPointer),
                 send_save_input(Widget, XtPointer, XtPointer),
                 send_toggled(Widget, XtPointer, XtPointer),
                 transmit_button(Widget, XtPointer, XtPointer);


/*########################### get_send_data() ###########################*/
void
get_send_data(void)
{
   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((sf_shell == (Widget)NULL) || (XtIsRealized(sf_shell) == False) ||
       (XtIsSensitive(sf_shell) != True))
   {
      Widget          buttonbox_w,
                      button_w,
                      criteriabox_w,
                      main_form_w,
                      optionbox_w,
                      option_menu_w,
                      pane_w,
                      radio_w,
                      recipientbox_w,
                      separator_w,
                      separator1_w;
      Boolean         set_button;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmString        label;
      XmFontList      p_fontlist;
      XmFontListEntry entry;

      if (db == NULL)
      {
         if ((db = malloc(sizeof(struct send_data))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         memset(db, 0, sizeof(struct send_data));

         /* Now set some default values. */
         db->protocol = FTP;
         db->lock = SET_LOCK_DOT;
         db->transfer_mode = SET_BIN;
         db->timeout = DEFAULT_TRANSFER_TIMEOUT;
         if (db->protocol == FTP)
         {
            db->port = DEFAULT_FTP_PORT;
         }
         else if (db->protocol == SMTP)
              {
                 db->port = DEFAULT_SMTP_PORT;
              }
#ifdef _WITH_WMO_SUPPORT
         else if (db->protocol == WMO)
              {
                 db->port = -1;
              }
#endif /* _WITH_WMO_SUPPORT */
      }

      sf_shell = XtVaCreatePopupShell("Send Files", topLevelShellWidgetClass,
                                       toplevel_w, NULL);

      /* Create managing widget */
      main_form_w = XmCreateForm(sf_shell, "main_form", NULL, 0);

      /* Prepare font */
      if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), font_name,
                                       XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), "fixed",
                                          XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr,
                          "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      p_fontlist = XmFontListAppendEntry(NULL, entry);
      XmFontListEntryFree(&entry);

      /*---------------------------------------------------------------*/
      /*                         Button Box                            */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNfractionBase,     21);
      argcount++;
      buttonbox_w = XmCreateForm(main_form_w, "buttonbox", args, argcount);

      /* Create Send Button. */
      button_w = XtVaCreateManagedWidget("Send",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             p_fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         1,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        10,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       20,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)transmit_button, 0);

      /* Create Cancel Button. */
      button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             p_fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         11,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        20,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       20,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)cancel_send_button, 0);
      XtManageChild(buttonbox_w);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
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
      separator_w = XmCreateSeparator(main_form_w, "separator",
                                      args, argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                         Status Box                            */
      /*---------------------------------------------------------------*/
      send_statusbox_w = XtVaCreateManagedWidget(" ",
                        xmLabelWidgetClass,  main_form_w,
                        XmNfontList,         p_fontlist,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     separator_w,
                        NULL);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     send_statusbox_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      separator1_w = XmCreateSeparator(main_form_w, "separator",
                                       args, argcount);
      XtManageChild(separator1_w);

      /*---------------------------------------------------------------*/
      /*                        Criteria Box                           */
      /*---------------------------------------------------------------*/
      criteriabox_w = XtVaCreateWidget("criteriabox",
                        xmFormWidgetClass,  main_form_w,
                        XmNtopAttachment,   XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /*---------------------------------------------------------------*/
      /*                       Recipient Box                           */
      /*---------------------------------------------------------------*/
      recipientbox_w = XtVaCreateManagedWidget("recipientbox",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /* Distribution type (FTP, SMTP, LOC, etc) */
      /* Create a pulldown pane and attach it to the option menu */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList,         p_fontlist);
      argcount++;
      pane_w = XmCreatePulldownMenu(recipientbox_w, "pane", args, argcount);

      label = XmStringCreateLocalized("Scheme :");
      argcount = 0;
      XtSetArg(args[argcount], XmNsubMenuId,      pane_w);
      argcount++;
      XtSetArg(args[argcount], XmNlabelString,    label);
      argcount++;
      XtSetArg(args[argcount], XmNfontList,       p_fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNtopOffset,      -2);
      argcount++;
      option_menu_w = XmCreateOptionMenu(recipientbox_w, "proc_selection",
                                         args, argcount);
      XtManageChild(option_menu_w);
      XmStringFree(label);

      /* Add all possible buttons. */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, p_fontlist);
      argcount++;
      button_w = XtCreateManagedWidget("FTP",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, send_toggled,
                    (XtPointer)FTP);
      button_w = XtCreateManagedWidget("FILE",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, send_toggled,
                    (XtPointer)LOC);
      button_w = XtCreateManagedWidget("MAILTO",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, send_toggled,
                    (XtPointer)SMTP);
#ifdef _WITH_WMO_SUPPORT
      button_w = XtCreateManagedWidget("WMO",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, send_toggled,
                    (XtPointer)WMO);
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
      button_w = XtCreateManagedWidget("MAP",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, send_toggled,
                    (XtPointer)MAP);
#endif /* _WITH_MAP_SUPPORT */

      /* User */
      user_name_label_w = XtVaCreateManagedWidget("User :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       option_menu_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      user_name_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          10,
                        XmNmaxLength,        MAX_USER_NAME_LENGTH,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       user_name_label_w,
                        NULL);
      XtAddCallback(user_name_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)USER_NO_ENTER);
      XtAddCallback(user_name_w, XmNactivateCallback, send_save_input,
                    (XtPointer)USER_ENTER);
      if ((db->protocol != FTP) && (db->protocol != SMTP))
      {
         XtSetSensitive(user_name_label_w, False);
         XtSetSensitive(user_name_w, False);
      }

      /* Password */
      password_label_w = XtVaCreateManagedWidget("Password :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       user_name_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      password_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          8,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       password_label_w,
                        NULL);
      if (db->protocol != FTP)
      {
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
      }
      XtAddCallback(password_w, XmNmodifyVerifyCallback, enter_passwd,
                    (XtPointer)PASSWORD_NO_ENTER);
      XtAddCallback(password_w, XmNactivateCallback, enter_passwd,
                    (XtPointer)PASSWORD_ENTER);

      /* Hostname */
      hostname_label_w = XtVaCreateManagedWidget("Hostname :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       password_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      hostname_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          12,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       hostname_label_w,
                        NULL);
      XtAddCallback(hostname_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)HOSTNAME_NO_ENTER);
      XtAddCallback(hostname_w, XmNactivateCallback, send_save_input,
                    (XtPointer)HOSTNAME_ENTER);
      if (db->protocol == LOC)
      {
         XtSetSensitive(hostname_label_w, False);
         XtSetSensitive(hostname_w, False);
      }
      XtManageChild(recipientbox_w);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,       recipientbox_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
      argcount++;
      separator_w = XmCreateSeparator(criteriabox_w, "separator",
                                      args, argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                        1st Option Box                         */
      /*---------------------------------------------------------------*/
      optionbox_w = XtVaCreateManagedWidget("optionbox1",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       separator_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /* Directory */
      target_dir_label_w = XtVaCreateManagedWidget("Directory :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      target_dir_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          28,
                        XmNmaxLength,        MAX_PATH_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       target_dir_label_w,
                        NULL);
      XtAddCallback(target_dir_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)TARGET_DIR_NO_ENTER);
      XtAddCallback(target_dir_w, XmNactivateCallback, send_save_input,
                    (XtPointer)TARGET_DIR_ENTER);
      if ((db->protocol != FTP) && (db->protocol != LOC))
      {
         XtSetSensitive(target_dir_label_w, False);
         XtSetSensitive(target_dir_w, False);
      }

      /* Transfer timeout. */
      timeout_label_w = XtVaCreateManagedWidget("Timeout :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       target_dir_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      timeout_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       timeout_label_w,
                        XmNcolumns,          MAX_TIMEOUT_DIGITS,
                        XmNmaxLength,        MAX_TIMEOUT_DIGITS,
                        NULL);
      XtAddCallback(timeout_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)TIMEOUT_NO_ENTER);
      XtAddCallback(timeout_w, XmNactivateCallback, send_save_input,
                    (XtPointer)TIMEOUT_ENTER);
#ifdef _WITH_MAP_SUPPORT
      if ((db->protocol == LOC) || (db->protocol == MAP))
#else
      if (db->protocol == LOC)
#endif
      {
         XtSetSensitive(timeout_label_w, False);
         XtSetSensitive(timeout_w, False);
      }

      /* Port */
      port_label_w = XtVaCreateManagedWidget("Port :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       timeout_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      port_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          MAX_PORT_DIGITS,
                        XmNmaxLength,        MAX_PORT_DIGITS,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       port_label_w,
                        NULL);
      XtAddCallback(port_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)PORT_NO_ENTER);
      XtAddCallback(port_w, XmNactivateCallback, send_save_input,
                    (XtPointer)PORT_ENTER);
      if (db->protocol == LOC)
      {
         XtSetSensitive(port_label_w, False);
         XtSetSensitive(port_w, False);
      }

      /*---------------------------------------------------------------*/
      /*                      Debug toggle box                         */
      /*---------------------------------------------------------------*/
      buttonbox_w = XtVaCreateWidget("debug_togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,      XmHORIZONTAL,
                        XmNpacking,          XmPACK_TIGHT,
                        XmNnumColumns,       1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNresizable,        False,
                        NULL);
      button_w = XtVaCreateManagedWidget("Debug ",
                        xmToggleButtonGadgetClass, buttonbox_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    False,
                        NULL);                           
      XtAddCallback(button_w, XmNvalueChangedCallback,
                    (XtCallbackProc)debug_toggle, NULL);
      db->debug = NO;
      XtManageChild(buttonbox_w);

      XtManageChild(optionbox_w);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,       optionbox_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
      argcount++;
      separator_w = XmCreateSeparator(criteriabox_w, "separator",
                                      args, argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                       2nd  Option Box                         */
      /*---------------------------------------------------------------*/
      optionbox_w = XtVaCreateManagedWidget("optionbox2",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       separator_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /* Transfer type (ASCII, BINARY, DOS, etc) */
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNorientation,    XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNpacking,        XmPACK_TIGHT);
      argcount++;
      XtSetArg(args[argcount], XmNnumColumns,     1);
      argcount++;
      mode_box_w = XmCreateRadioBox(optionbox_w, "radiobox", args, argcount);
      if (db->transfer_mode == SET_ASCII)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("ASCII",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)mode_radio, (XtPointer)SET_ASCII);
      if (db->transfer_mode == SET_BIN)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("BIN",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)mode_radio, (XtPointer)SET_BIN);
      if (db->transfer_mode == SET_DOS)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("DOS",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)mode_radio, (XtPointer)SET_DOS);
      XtManageChild(mode_box_w);
      if (db->protocol != FTP)
      {
         XtSetSensitive(mode_box_w, False);
      }

      /*---------------------------------------------------------------*/
      /*                      Vertical Separator                       */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       mode_box_w);
      argcount++;
      separator_w = XmCreateSeparator(optionbox_w, "separator",
                                      args, argcount);
      XtManageChild(separator_w);

      /* Lock type (DOT, OFF, DOT_VMS, LOCKFILE and prefix */
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,     separator_w);
      argcount++;
      XtSetArg(args[argcount], XmNorientation,    XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNpacking,        XmPACK_TIGHT);
      argcount++;
      XtSetArg(args[argcount], XmNnumColumns,     1);
      argcount++;
      lock_box_w = XmCreateRadioBox(optionbox_w, "radiobox", args, argcount);
      if (db->lock == SET_LOCK_DOT)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("DOT",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_DOT);
      if (db->lock == SET_LOCK_OFF)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("OFF",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_OFF);
      if (db->lock == SET_LOCK_DOT_VMS)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("DOT_VMS",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_DOT_VMS);
      if (db->lock == SET_LOCK_LOCKFILE)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("LOCKFILE",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_LOCKFILE);
      if (db->lock == SET_LOCK_PREFIX)
      {
         set_button = True;
      }
      else
      {
         set_button = False;
      }
      radio_w = XtVaCreateManagedWidget("Prefix",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    set_button,
                        NULL);
      XtAddCallback(radio_w, XmNdisarmCallback,
                    (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_PREFIX);
      XtManageChild(lock_box_w);
      if ((db->protocol != FTP) && (db->protocol != LOC))
      {
         XtSetSensitive(lock_box_w, False);
      }

      /* Text box to enter the prefix. */
      prefix_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          8,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       lock_box_w,
                        NULL);
      XtAddCallback(prefix_w, XmNlosingFocusCallback, send_save_input,
                    (XtPointer)PREFIX_NO_ENTER);
      XtAddCallback(prefix_w, XmNactivateCallback, send_save_input,
                    (XtPointer)PREFIX_ENTER);
      XtSetSensitive(prefix_w, set_button);

      XtManageChild(optionbox_w);
      XtManageChild(criteriabox_w);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,       optionbox_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
      argcount++;
      separator_w = XmCreateSeparator(criteriabox_w, "separator",
                                      args, argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                         Output Box                            */
      /*---------------------------------------------------------------*/
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
      XtSetArg(args[argcount], XmNfontList,               p_fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,              separator_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,           separator1_w);
      argcount++;
      log_output = XmCreateScrolledText(main_form_w, "log_output",
                                        args, argcount);
      XtManageChild(log_output);
      XtManageChild(main_form_w);

#ifdef _EDITRES
      XtAddEventHandler(sf_shell, (EventMask)0, True,
                        _XEditResCheckMessages, NULL);
#endif
      if (db->port > 0)
      {
         char str_line[MAX_PORT_DIGITS + 1];

         (void)sprintf(str_line, "%*d", MAX_PORT_DIGITS, db->port);
         XmTextSetString(port_w, str_line);
      }
      if (db->timeout > 0)
      {
         char str_line[MAX_TIMEOUT_DIGITS + 1];

         (void)sprintf(str_line, "%*ld", MAX_TIMEOUT_DIGITS, db->timeout);
         XmTextSetString(timeout_w, str_line);
      }
   }
   else
   {
      XmTextSetString(log_output, NULL);  /* Clears all old entries */
   }
   wpr_position = 0;
   XmTextSetInsertionPosition(log_output, 0);
   XtSetSensitive(toplevel_w, False);
   XtPopup(sf_shell, XtGrabNone);

   return;
}


/*++++++++++++++++++++++++ cancel_send_button() +++++++++++++++++++++++++*/
static void
cancel_send_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtSetSensitive(toplevel_w, True);
   XtPopdown(sf_shell);

   return;
}


/*++++++++++++++++++++++++++++ lock_radio() +++++++++++++++++++++++++++++*/
static void
lock_radio(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (db->lock != (int)client_data)
   {
      if ((int)client_data == SET_LOCK_PREFIX)
      {
         XtSetSensitive(prefix_w, True);
      }
      else if (db->lock == SET_LOCK_PREFIX)
           {
              XtSetSensitive(prefix_w, False);
           }
   }
   db->lock = (int)client_data;

   return;
}


/*+++++++++++++++++++++++++++ debug_toggle() ++++++++++++++++++++++++++++*/
static void
debug_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (db->debug == NO)
   {
      db->debug = YES;
   }
   else
   {
      db->debug = NO;
   }

   return;
}


/*++++++++++++++++++++++++++++ mode_radio() +++++++++++++++++++++++++++++*/
static void
mode_radio(Widget w, XtPointer client_data, XtPointer call_data)
{
   db->transfer_mode = (int)client_data;

   return;
}


/*+++++++++++++++++++++++++++ send_toggled() ++++++++++++++++++++++++++++*/
static void
send_toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   switch ((int)client_data)
   {
      case FTP :
         XtSetSensitive(user_name_label_w, True);
         XtSetSensitive(user_name_w, True);
         XtSetSensitive(password_label_w, True);
         XtSetSensitive(password_w, True);
         XtSetSensitive(hostname_label_w, True);
         XtSetSensitive(hostname_w, True);
         XtSetSensitive(target_dir_label_w, True);
         XtSetSensitive(target_dir_w, True);
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);
         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);
         XtSetSensitive(mode_box_w, True);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);
         if ((db->port == 0) || (db->protocol != FTP))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_FTP_PORT;
            (void)sprintf(str_line, "%*d", MAX_PORT_DIGITS, db->port);
            XmTextSetString(port_w, str_line);
         }
         break;
      case SMTP :
         XtSetSensitive(user_name_label_w, True);
         XtSetSensitive(user_name_w, True);
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
         XtSetSensitive(hostname_label_w, True);
         XtSetSensitive(hostname_w, True);
         XtSetSensitive(target_dir_label_w, False);
         XtSetSensitive(target_dir_w, False);
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);
         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, False);
         XtSetSensitive(prefix_w, False);
         if ((db->port == 0) || (db->protocol != SMTP))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_SMTP_PORT;
            (void)sprintf(str_line, "%*d", MAX_PORT_DIGITS, db->port);
            XmTextSetString(port_w, str_line);
         }
         break;
      case LOC :
         XtSetSensitive(user_name_label_w, False);
         XtSetSensitive(user_name_w, False);
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
         XtSetSensitive(hostname_label_w, False);
         XtSetSensitive(hostname_w, False);
         XtSetSensitive(target_dir_label_w, True);
         XtSetSensitive(target_dir_w, True);
         XtSetSensitive(port_label_w, False);
         XtSetSensitive(port_w, False);
         XtSetSensitive(timeout_label_w, False);
         XtSetSensitive(timeout_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);
         break;
#ifdef _WITH_WMO_SUPPORT
      case WMO :
         XtSetSensitive(user_name_label_w, False);
         XtSetSensitive(user_name_w, False);
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
         XtSetSensitive(hostname_label_w, True);
         XtSetSensitive(hostname_w, True);
         XtSetSensitive(target_dir_label_w, False);
         XtSetSensitive(target_dir_w, False);
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);
         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, False);
         XtSetSensitive(prefix_w, False);
         if ((db->port == 0) || (db->protocol != WMO))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = 0;
            (void)sprintf(str_line, "%*s", MAX_PORT_DIGITS - 1, " ");
            XmTextSetString(port_w, str_line);
         }
         break;
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
      case MAP :
         XtSetSensitive(user_name_label_w, True);
         XtSetSensitive(user_name_w, True);
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
         XtSetSensitive(hostname_label_w, True);
         XtSetSensitive(hostname_w, True);
         XtSetSensitive(target_dir_label_w, False);
         XtSetSensitive(target_dir_w, False);
         XtSetSensitive(port_label_w, False);
         XtSetSensitive(port_w, False);
         XtSetSensitive(timeout_label_w, False);
         XtSetSensitive(timeout_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, False);
         XtSetSensitive(prefix_w, False);
         break;
#endif /* _WITH_MAP_SUPPORT */
      default : /* Should never get here! */
         (void)fprintf(stderr, "Junk programmer!\n");
         exit(INCORRECT);
   }
   db->protocol = (int)client_data;

   return;
}


/*++++++++++++++++++++++++++ send_save_input() ++++++++++++++++++++++++++*/
static void
send_save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   int  type = (int)client_data;
   char *value = XmTextGetString(w);

   switch (type)
   {
      case HOSTNAME_NO_ENTER :
         (void)strcpy(db->hostname, value);
         break;

      case HOSTNAME_ENTER :
         (void)strcpy(db->hostname, value);
         reset_message(send_statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case USER_NO_ENTER :
         (void)strcpy(db->user, value);
         break;

      case USER_ENTER :
         (void)strcpy(db->user, value);
         reset_message(send_statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case TARGET_DIR_NO_ENTER :
         (void)strcpy(db->target_dir, value);
         break;

      case TARGET_DIR_ENTER :
         (void)strcpy(db->target_dir, value);
         reset_message(send_statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case PORT_NO_ENTER :
      case PORT_ENTER :
         if (value[0] != '\0')
         {
            char *ptr = value;

            while (*ptr != '\0')
            {
               if (!isdigit(*ptr))
               {
                  show_message(send_statusbox_w, "Invalid port number!");
                  XtFree(value);
                  return;
               }
               ptr++;
            }
            if (*ptr == '\0')
            {
               db->port = atoi(value);
            }
         }
         if (type == PORT_ENTER)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         reset_message(send_statusbox_w);
         break;

      case TIMEOUT_NO_ENTER :
      case TIMEOUT_ENTER :
         if (value[0] != '\0')
         {
            char *ptr = value;

            while (*ptr != '\0')
            {
               if (!isdigit(*ptr))
               {
                  show_message(send_statusbox_w, "Invalid timeout!");
                  XtFree(value);
                  return;
               }
               ptr++;
            }
            if (*ptr == '\0')
            {
               db->timeout = atoi(value);
            }
         }
         if (type == TIMEOUT_ENTER)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         reset_message(send_statusbox_w);
         break;

      case PREFIX_NO_ENTER :
         (void)strcpy(db->prefix, value);
         break;

      case PREFIX_ENTER :
         (void)strcpy(db->prefix, value);
         reset_message(send_statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      default :
         (void)fprintf(stderr, "ERROR   : Impossible! (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
   }
   XtFree(value);

   return;
}


/*+++++++++++++++++++++++++++ enter_passwd() ++++++++++++++++++++++++++++*/
/*                            --------------                             */
/* Description: Displays the password as asterisk '*' to make it         */
/*              invisible.                                               */
/* Source     : Motif Programming Manual Volume 6A                       */
/*              by Dan Heller & Paula M. Ferguson                        */
/*              Page 502                                                 */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
enter_passwd(Widget w, XtPointer client_data, XtPointer call_data)
{
   int                        length,
                              type = (int)client_data;
   char                       *new;
   XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *)call_data;

   if (cbs->reason == XmCR_ACTIVATE)
   {
      return;
   }

   /* Backspace */
   if (cbs->startPos < cbs->currInsert)
   {
      db->password[cbs->startPos] = '\0';
      return;
   }

   /* Pasting no allowed. */
   if (cbs->text->length > 1)
   {
      cbs->doit = False;
      return;
   }

   new = XtMalloc(cbs->endPos + 2);
   if (db->password)
   {
      (void)strcpy(new, db->password);
      XtFree(db->password);
   }
   else
   {
      new[0] = '\0';
   }
   db->password = new;
   (void)strncat(db->password, cbs->text->ptr, cbs->text->length);
   db->password[cbs->endPos + cbs->text->length] = '\0';
   for (length = 0; length < cbs->text->length; length++)
   {
      cbs->text->ptr[length] = '*';
   }

   if (type == PASSWORD_ENTER)
   {
      XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
   }

   return;
}


/*########################## transmit_button() ##########################*/
static void
transmit_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   int no_selected,
       *select_list;

   reset_message(send_statusbox_w);

   /*
    * First lets check if all required parameters are available.
    */
   switch (db->protocol)
   {
      case FTP :
      case SMTP :
         if (db->user[0] == '\0')
         {
            show_message(send_statusbox_w, "No user name given!");
            XmProcessTraversal(user_name_w, XmTRAVERSE_CURRENT);
            return;
         }
         if (db->hostname[0] == '\0')
         {
            show_message(send_statusbox_w, "No hostname given!");
            XmProcessTraversal(hostname_w, XmTRAVERSE_CURRENT);
            return;
         }
         break;
      case LOC :
         break;
#ifdef _WITH_WMO_SUPPORT
      case WMO :
         if (db->hostname[0] == '\0')
         {
            show_message(send_statusbox_w, "No hostname given!");
            XmProcessTraversal(hostname_w, XmTRAVERSE_CURRENT);
            return;
         }
         if (db->port == -1)
         {
            show_message(send_statusbox_w, "No port given!");
            XmProcessTraversal(port_w, XmTRAVERSE_CURRENT);
            return;
         }
         break;
#endif
#ifdef _WITH_MAP_SUPPORT
      case MAP :
         break;
#endif
      default :
         show_message(send_statusbox_w, "No protocol selected, or unknown.");
         break;
   }

   if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
   {
      send_files(no_selected, select_list);
      XtFree((char *)select_list);

      /*
       * After resending, see if any items habe been left selected.
       * If so, create a new summary string or else insert the total
       * summary string if no items are left selected.
       */
      if (XmListGetSelectedPos(listbox_w, &select_list, &no_selected) == True)
      {
         int    i;
         time_t date,
                first_date_found,
                last_date_found;
         double current_file_size,
                current_trans_time,
                file_size = 0.0,
                trans_time = 0.0;

         first_date_found = -1;
         for (i = 0; i < no_selected; i++)
         {
            if (get_sum_data((select_list[i] - 1),
                             &date, &current_file_size,
                             &current_trans_time) == INCORRECT)
            {
               return;
            }
            if (first_date_found == -1)
            {
               first_date_found = date;
            }
            file_size += current_file_size;
            trans_time += current_trans_time;
         }
         last_date_found = date;
         XtFree((char *)select_list);
         calculate_summary(summary_str, first_date_found, last_date_found,
                           no_selected, file_size, trans_time);
      }
      else
      {
         (void)strcpy(summary_str, total_summary_str);
      }
      XmTextSetString(summarybox_w, summary_str);
   }
   else
   {
      show_message(statusbox_w, "No file selected!");
   }

   return;
}
