/*
 *  get_send_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

/* External global variables */
extern Display    *display;
extern Widget     listbox_w,
                  summarybox_w,
                  toplevel_w;
extern char       font_name[],
                  search_file_name[],
                  search_directory_name[],
                  search_recipient[],
                  search_file_size_str[],
                  summary_str[],
                  total_summary_str[];
extern int        items_selected,
                  toggles_set,
                  file_name_length;
extern time_t     start_time_val,
                  end_time_val;

/* Local global variables */
static Widget     sf_shell = (Widget)NULL;

/* Local function prototypes. */
static void       cancel_send_button(Widget, XtPointer, XtPointer);


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
      Widget          box_w,
                      buttonbox_w,
                      button_w,
                      criteriabox_w,
                      label_w,
                      log_output,
                      main_form_w,
                      optionbox_w,
                      option_menu_w,
                      pane_w,
                      password_w,
                      recipientbox_w,
                      recipient_name_w,
                      separator_w,
                      statusbox_w,
                      timeout_w,
                      toggle_w,
                      user_name_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmString        label;
      XmFontList      p_fontlist;
      XmFontListEntry entry;

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
      statusbox_w = XtVaCreateManagedWidget(" ",
                        xmLabelWidgetClass,  main_form_w,
                        XmNfontList,         p_fontlist,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     separator_w,
                        NULL);

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

      label = XmStringCreateLocalized("Scheme");
      argcount = 0;
      XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
      argcount++;
      XtSetArg(args[argcount], XmNlabelString,      label);
      argcount++;
      XtSetArg(args[argcount], XmNfontList,         p_fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNtopOffset,        -2);
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
      XtAddCallback(button_w, XmNactivateCallback, toggled,
                    (XtPointer)SET_FTP);
#ifdef _TODO_
      button_w = XtCreateManagedWidget("FILE",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, toggled,
                    (XtPointer)SET_FILE);
      button_w = XtCreateManagedWidget("MAILTO",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, toggled,
                    (XtPointer)SET_SMTP);
#ifdef _WITH_WMO_SUPPORT
      button_w = XtCreateManagedWidget("WMO",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, toggled,
                    (XtPointer)SET_WMO);
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
      button_w = XtCreateManagedWidget("MAP",
                                       xmPushButtonWidgetClass,
                                       pane_w, args, argcount);
      XtAddCallback(button_w, XmNactivateCallback, toggled,
                    (XtPointer)SET_MAP);
#endif /* _WITH_MAP_SUPPORT */
#endif /* _TODO_ */

      /* Recipient */
      label_w = XtVaCreateManagedWidget("Recipient :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       option_menu_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      recipient_name_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          12,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       label_w,
                        NULL);
      XtAddCallback(recipient_name_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)RECIPIENT_NO_ENTER);
      XtAddCallback(recipient_name_w, XmNactivateCallback, save_input,
                    (XtPointer)RECIPIENT_ENTER);

      /* User */
      label_w = XtVaCreateManagedWidget("User :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       recipient_name_w,
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
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       label_w,
                        NULL);
      XtAddCallback(user_name_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)USER_NO_ENTER);
      XtAddCallback(user_name_w, XmNactivateCallback, save_input,
                    (XtPointer)USER_ENTER);

      /* Password */
      label_w = XtVaCreateManagedWidget("Password :",
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
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       label_w,
                        NULL);
      XtAddCallback(password_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)PASSWORD_NO_ENTER);
      XtAddCallback(password_w, XmNactivateCallback, save_input,
                    (XtPointer)PASSWORD_ENTER);
      XtManageChild(recipientbox_w);

      /*---------------------------------------------------------------*/
      /*                        1st Option Box                         */
      /*---------------------------------------------------------------*/
      optionbox_w = XtVaCreateManagedWidget("optionbox1",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       recipientbox_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /* Directory */
      label_w = XtVaCreateManagedWidget("Directory :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      user_name_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          8,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       label_w,
                        NULL);
      XtAddCallback(user_name_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)TARGET_DIR_NO_ENTER);
      XtAddCallback(user_name_w, XmNactivateCallback, save_input,
                    (XtPointer)TARGET_DIR_ENTER);

      /* Port */
      label_w = XtVaCreateManagedWidget("Port :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       user_name_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      password_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          4,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       label_w,
                        NULL);
      XtAddCallback(password_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)PORT_NO_ENTER);
      XtAddCallback(password_w, XmNactivateCallback, save_input,
                    (XtPointer)PORT_ENTER);

      /* Transfer type (ASCII, BINARY, DOS, etc) */
      box_w = XtVaCreateWidget("togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,         XmHORIZONTAL,
                        XmNpacking,             XmPACK_TIGHT,
                        XmNnumColumns,          1,
                        XmNtopAttachment,       XmATTACH_FORM,
                        XmNleftAttachment,      XmATTACH_WIDGET,
                        XmNleftWidget,          password_w,
                        XmNresizable,           False,
                        NULL);
      toggle_w = XtVaCreateManagedWidget("ASCII",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_ASCII);
      toggle_w = XtVaCreateManagedWidget("BIN",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_BIN);
      toggle_w = XtVaCreateManagedWidget("DOS",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_DOS);
      XtManageChild(box_w);

      label_w = XtVaCreateManagedWidget("Timeout :",
                        xmLabelGadgetClass, optionbox_w,
                        XmNfontList,        p_fontlist,
                        XmNtopAttachment,   XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_WIDGET,
                        XmNleftWidget,      box_w,
                        XmNalignment,       XmALIGNMENT_END,
                        NULL);
      timeout_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,  optionbox_w,
                        XmNfontList,        p_fontlist,
                        XmNmarginHeight,    1,
                        XmNmarginWidth,     1,
                        XmNshadowThickness, 1,
                        XmNtopAttachment,   XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_WIDGET,
                        XmNleftWidget,      label_w,
                        XmNcolumns,         4,
                        XmNmaxLength,       4,
                        NULL);
      XtAddCallback(timeout_w, XmNlosingFocusCallback, save_input,
                    (XtPointer)TIMEOUT_NO_ENTER);
      XtAddCallback(timeout_w, XmNactivateCallback, save_input,
                    (XtPointer)TIMEOUT_ENTER);
      XtManageChild(optionbox_w);

      /*---------------------------------------------------------------*/
      /*                       2nd  Option Box                         */
      /*---------------------------------------------------------------*/
      optionbox_w = XtVaCreateManagedWidget("optionbox2",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       optionbox_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

      /* Lock type (DOT, OFF, DOT_VMS, LOCKFILE and prefix */
      box_w = XtVaCreateWidget("togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,         XmHORIZONTAL,
                        XmNpacking,             XmPACK_TIGHT,
                        XmNnumColumns,          1,
                        XmNtopAttachment,       XmATTACH_FORM,
                        XmNleftAttachment,      XmATTACH_FORM,
                        XmNresizable,           False,
                        NULL);
      toggle_w = XtVaCreateManagedWidget("DOT",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_LOCK_DOT);
      toggle_w = XtVaCreateManagedWidget("OFF",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_LOCK_OFF);
      toggle_w = XtVaCreateManagedWidget("DOT_VMS",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_LOCK_DOT_VMS);
      toggle_w = XtVaCreateManagedWidget("LOCKFILE",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_LOCK_LOCKFILE);
      toggle_w = XtVaCreateManagedWidget("Prefix",
                        xmToggleButtonGadgetClass, box_w,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SET_LOCK_PREFIX);
      XtManageChild(box_w);

      XtManageChild(optionbox_w);
      XtManageChild(criteriabox_w);

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
      XtSetArg(args[argcount], XmNtopWidget,              criteriabox_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,           statusbox_w);
      argcount++;
      log_output = XmCreateScrolledText(main_form_w, "log_output",
                                        args, argcount);
      XtManageChild(log_output);
      XtManageChild(main_form_w);

#ifdef _EDITRES
      XtAddEventHandler(sf_shell, (EventMask)0, True,
                        _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(sf_shell, XtGrabNone);

   return;
}


/*++++++++++++++++++++++++ cancel_send_button() +++++++++++++++++++++++++*/
static void
cancel_send_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(sf_shell);

   return;
}
