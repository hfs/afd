/*
 *  edit_hc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   edit_hc - edits the AFD host configuration file
 **
 ** SYNOPSIS
 **   edit_hc [--version] [-w <AFD working directory>] [-f <font name>]
 **
 ** DESCRIPTION
 **   This dialog allows the user to change the following parameters
 **   for a given hostname:
 **       - Real Hostaname/IP Number
 **       - Transfer timeout
 **       - Retry interval
 **       - Maximum errors
 **       - Successful retries
 **       - Max. parallel transfers
 **       - Transfer Blocksize
 **       - File size offset
 **       - Number of transfers that may not burst
 **       - Proxy name
 **   additionally some protocol specific options can be set:
 **       - FTP active/passive mode
 **       - set FTP idle time
 **
 **   In the list widget "Alias Hostname" the user can change the order
 **   of the host names in the afd_ctrl dialog by using drag & drop.
 **   During the drag operation the cursor will change into a bee. The
 **   hot spot of this cursor are the two feelers.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.08.1997 H.Kiehl Created
 **   28.02.1998 H.Kiehl Added host switching information.
 **   21.10.1998 H.Kiehl Added Remove button to remove hosts from the FSA.
 **   04.08.2001 H.Kiehl Added support for active or passive mode and
 **                      FTP idle time.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* malloc(), free()                     */
#include <ctype.h>               /* isdigit()                            */
#include <signal.h>              /* signal()                             */
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>            /* mmap()                               */
#endif
#include <fcntl.h>               /* O_RDWR                               */
#include <unistd.h>              /* STDERR_FILENO                        */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"
#include "permission.h"

#ifdef _EDITRES
#include <X11/Xmu/Editres.h>
#endif
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#include "edit_hc.h"
#include "afd_ctrl.h"
#include "source.h"
#include "source_mask.h"
#include "no_source.h"
#include "no_source_mask.h"

/* Global variables */
XtAppContext               app;
Display                    *display;
Widget                     active_mode_w,
                           auto_toggle_w,
                           first_label_w,
                           ftp_idle_time_w,
                           ftp_mode_w,
                           host_1_w,
                           host_2_w,
                           host_1_label_w,
                           host_2_label_w,
                           host_list_w,
                           host_switch_toggle_w,
                           idle_time_w,
                           max_errors_w,
                           mode_label_w,
                           no_source_icon_w,
                           passive_mode_w,
                           proxy_name_w,
                           real_hostname_1_w,
                           real_hostname_2_w,
                           retry_interval_w,
                           rm_button_w,
                           second_label_w,
                           source_icon_w,
                           start_drag_w,
                           statusbox_w,
                           successful_retries_label_w,
                           successful_retries_w,
                           toplevel_w,
                           transfer_timeout_w;
Atom                       compound_text;
int                        fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           host_alias_order_change = NO,
                           in_drop_site = -2,
                           no_of_dirs,
                           no_of_hosts,
                           sys_log_fd = STDERR_FILENO;
#ifndef _NO_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       *p_work_dir,
                           last_selected_host[MAX_HOSTNAME_LENGTH + 1];
struct fileretrieve_status *fra;
struct filetransfer_status *fsa;
struct host_list           *hl = NULL; /* required by change_alias_order() */
struct changed_entry       *ce;
struct parallel_transfers  pt;
struct no_of_no_bursts     nob;
struct transfer_blocksize  tb;
struct file_size_offset    fso;

/* Local global variables */
static char                font_name[40],
                           translation_table[] = "#override <Btn2Down>: start_drag()";
static XtActionsRec        action_table[] =
                           {
                              { "start_drag", (XtActionProc)start_drag }
                           };

/* Local functions. */
static void                create_option_menu_fso(Widget, Widget, XmFontList),
                           create_option_menu_nob(Widget, Widget, XmFontList),
                           create_option_menu_pt(Widget, Widget, XmFontList),
                           create_option_menu_tb(Widget, Widget, XmFontList),
                           init_edit_hc(int *, char **, char *),
                           init_widget_data(void),
                           sig_bus(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      ".edit_hc*mwmDecorations : 10",
                      ".edit_hc*mwmFunctions : 4",
                      ".edit_hc*background : NavajoWhite2",
                      ".edit_hc.form_w.host_list_box_w.host_list_wSW*background : NavajoWhite1",
                      ".edit_hc.form_w*XmText.background : NavajoWhite1",
                      ".edit_hc.form_w.button_box*background : PaleVioletRed2",
                      ".edit_hc.form_w.button_box.Remove.XmDialogShell*background : NavajoWhite2",
                      ".edit_hc.form_w.button_box*foreground : Black",
                      ".edit_hc.form_w.button_box*highlightColor : Black",
                      NULL
                   };
   Widget          box_w,
                   button_w,
                   form_w,
                   label_w,
                   h_separator_bottom_w,
                   h_separator_top_w,
                   v_separator_w;
   XtTranslations  translations;
   Atom            targets[1];
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XmFontList      fontlist;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Initialise global values */
   init_edit_hc(&argc, argv, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel_w = XtAppInitialize(&app, "AFD", NULL, 0,
                                &argc, argv, fallback_res, args, argcount);

   compound_text = XmInternAtom(display, "COMPOUND_TEXT", False);

   /* Create managing widget */
   form_w = XmCreateForm(toplevel_w, "form_w", NULL, 0);

   /* Prepare the font */
   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* Contains two buttons, one to activate the changes and the other to    */
/* close this window.                                                    */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     31);
   argcount++;
   box_w = XmCreateForm(form_w, "button_box", args, argcount);

   button_w = XtVaCreateManagedWidget("Update",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    10,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)submite_button, NULL);
   rm_button_w = XtVaCreateManagedWidget("Remove",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     11,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    20,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(rm_button_w, XmNactivateCallback,
                 (XtCallbackProc)remove_button, NULL);
   button_w = XtVaCreateManagedWidget("Close",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     21,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    30,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, NULL);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* Here any feedback from the program will be shown.                     */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                           xmLabelWidgetClass,       form_w,
                           XmNfontList,              fontlist,
                           XmNleftAttachment,        XmATTACH_FORM,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNbottomAttachment,      XmATTACH_WIDGET,
                           XmNbottomWidget,          h_separator_bottom_w,
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
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                             Host List Box                             */
/*                             -------------                            */
/* Lists all hosts that are stored in the FSA. They are listed in there  */
/* short form, ie MAX_HOSTNAME_LENGTH as displayed by afd_ctrl.          */
/* ----------------------------------------------------------------------*/
   /* Create managing widget for host list box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   box_w = XmCreateForm(form_w, "host_list_box_w", args, argcount);
   label_w = XtVaCreateManagedWidget("Alias Hostname:",
                             xmLabelGadgetClass,  box_w,
                             XmNfontList,         fontlist,
                             XmNtopAttachment,    XmATTACH_FORM,
                             XmNtopOffset,        SIDE_OFFSET,
                             XmNleftAttachment,   XmATTACH_FORM,
                             XmNleftOffset,       SIDE_OFFSET,
                             XmNrightAttachment,  XmATTACH_FORM,
                             XmNrightOffset,      SIDE_OFFSET,
                             XmNalignment,        XmALIGNMENT_BEGINNING,
                             NULL);

   /* Add actions */
   XtAppAddActions(app, action_table, XtNumber(action_table));
   translations = XtParseTranslationTable(translation_table);

   /* Create the host list widget */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              label_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNvisibleItemCount,       10);
   argcount++;
   XtSetArg(args[argcount], XmNselectionPolicy,        XmEXTENDED_SELECT);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtranslations,           translations);
   argcount++;
   host_list_w = XmCreateScrolledList(box_w, "host_list_w", args, argcount);
   XtManageChild(host_list_w);
   XtManageChild(box_w);
   XtAddCallback(host_list_w, XmNextendedSelectionCallback, selected, NULL);

   /* Set up host_list_w as a drop site. */
   targets[0] = XmInternAtom(display, "COMPOUND_TEXT", False);
   argcount = 0;
   XtSetArg(args[argcount], XmNimportTargets,      targets);
   argcount++;
   XtSetArg(args[argcount], XmNnumImportTargets,   1);
   argcount++;
   XtSetArg(args[argcount], XmNdropSiteOperations, XmDROP_MOVE);
   argcount++;
   XtSetArg(args[argcount], XmNdropProc,           accept_drop);
   argcount++;
   XmDropSiteRegister(box_w, args, argcount);

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(form_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

/*-----------------------------------------------------------------------*/
/*                           Host Switch Box                             */
/*                           ---------------                             */
/* Allows user to set host or auto host switching.                       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "host_switch_box_w", args, argcount);

   host_switch_toggle_w = XtVaCreateManagedWidget("Host switching",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(host_switch_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)HOST_SWITCHING);

   host_1_label_w = XtVaCreateManagedWidget("Host 1:",
                       xmLabelGadgetClass, box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_switch_toggle_w,
                       XmNleftOffset,       2 * SIDE_OFFSET,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_1_ID);
   host_2_label_w = XtVaCreateManagedWidget("Host 2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_2_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_2_ID);
   auto_toggle_w = XtVaCreateManagedWidget("Auto",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             host_2_w,
                       XmNleftOffset,             2 * SIDE_OFFSET,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(auto_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)AUTO_SWITCHING);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);


/*-----------------------------------------------------------------------*/
/*                           Real Hostname Box                           */
/*                           -----------------                           */
/* One text widget in which the user can enter the true host name or     */
/* IP-Address of the remote host. Another text widget is there for the   */
/* user to enter a proxy name.                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    62);
   argcount++;
   box_w = XmCreateForm(form_w, "real_hostname_box_w", args, argcount);

   XtVaCreateManagedWidget("Real Hostname/IP Number:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNrightAttachment,  XmATTACH_POSITION,
                       XmNrightPosition,    40,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   30,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   first_label_w = XtVaCreateManagedWidget("Host 1:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       NULL);
   real_hostname_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       first_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_1);

   second_label_w = XtVaCreateManagedWidget("Host 2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       real_hostname_1_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       NULL);
   real_hostname_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      31,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       second_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNrightAttachment,  XmATTACH_FORM,
                       XmNrightOffset,      SIDE_OFFSET,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_2);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                         Text Input Box                                */
/*                         --------------                                */
/* Here more control parameters can be entered, such as: maximum number  */
/* of errors, transfer timeout, retry interval and successful retries.   */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    62);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   label_w = XtVaCreateManagedWidget("Transfer timeout:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   transfer_timeout_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(transfer_timeout_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)TRANSFER_TIMEOUT);

   label_w = XtVaCreateManagedWidget("Retry interval    :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     31,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   retry_interval_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(retry_interval_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(retry_interval_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(retry_interval_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RETRY_INTERVAL);

   label_w = XtVaCreateManagedWidget("Maximum errors  :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   61,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   max_errors_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   61,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(max_errors_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(max_errors_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(max_errors_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)MAXIMUM_ERRORS);

   successful_retries_label_w = XtVaCreateManagedWidget("Successful retries:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     31,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   61,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   successful_retries_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       successful_retries_label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   61,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(successful_retries_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(successful_retries_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(successful_retries_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SUCCESSFUL_RETRIES);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                             Option Box                                */
/*                             ----------                                */
/* Here more control parameters can be selected, such as: maximum number */
/* of parallel transfers, transfer blocksize and file size offset.       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     81);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   /* Parallel transfers */
   label_w = XtVaCreateManagedWidget("Max. parallel transfers     :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   20,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_pt(box_w, label_w, fontlist);

   /* Transfer blocksize */
   label_w = XtVaCreateManagedWidget("Transfer Blocksize          :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      21,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   40,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_tb(box_w, label_w, fontlist);

   /* File size offset */
   label_w = XtVaCreateManagedWidget("File size offset for append :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      41,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   60,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_fso(box_w, label_w, fontlist);

   /* File size offset */
   label_w = XtVaCreateManagedWidget("Number of no bursts         :",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      61,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   80,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     NULL);
   create_option_menu_nob(box_w, label_w, fontlist);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                       Protocol Specific Options                       */
/*                       -------------------------                       */
/* Select FTP active or passive mode and set FTP idle time for remote    */
/* FTP-server.                                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for the protocol specific option box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific_box_w", args, argcount);

   mode_label_w = XtVaCreateManagedWidget("FTP Mode :",
                                xmLabelGadgetClass,  box_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNleftOffset,       5,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       mode_label_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   ftp_mode_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   active_mode_w = XtVaCreateManagedWidget("Active",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(active_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button,
                 (XtPointer)FTP_ACTIVE_MODE_SEL);
   passive_mode_w = XtVaCreateManagedWidget("Passive",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(passive_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button,
                 (XtPointer)FTP_PASSIVE_MODE_SEL);
   XtManageChild(ftp_mode_w);

   ftp_idle_time_w = XtVaCreateWidget("ftp_idle_time_togglebox",
                                xmRowColumnWidgetClass, box_w,
                                XmNorientation,      XmHORIZONTAL,
                                XmNpacking,          XmPACK_TIGHT,
                                XmNnumColumns,       1,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_WIDGET,
                                XmNleftWidget,       ftp_mode_w,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNresizable,        False,
                                NULL);
   idle_time_w = XtVaCreateManagedWidget("Set idle time ",
                                xmToggleButtonGadgetClass, ftp_idle_time_w,
                                XmNfontList,               fontlist,
                                XmNset,                    False,
                                NULL);
   XtAddCallback(idle_time_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, NULL);
   XtManageChild(ftp_idle_time_w);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                            Proxy Name Box                             */
/*                            --------------                             */
/* One text widget in which the user can enter the proxy name.           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for the proxy name box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   box_w = XmCreateForm(form_w, "proxy_name_box_w", args, argcount);

   label_w = XtVaCreateManagedWidget("Proxy Name:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNleftOffset,       5,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   proxy_name_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNrightOffset,      5,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(proxy_name_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(proxy_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)PROXY_NAME);
   XtManageChild(box_w);
   XtManageChild(form_w);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef _EDITRES
   XtAddEventHandler(toplevel_w, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel_w);
   wait_visible(toplevel_w);

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(toplevel_w, WARN_DIALOG,
                 "Failed to set signal handler's for afd_ctrl : %s",
                 strerror(errno));
   }

   /* Initialize widgets with data */
   init_widget_data();

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_edit_hc() +++++++++++++++++++++++++++*/
static void
init_edit_hc(int *argc, char *argv[], char *window_title)
{
   char        *perm_buffer,
               *p_user,
               hostname[MAX_AFD_NAME_LENGTH],
               sys_log_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Now lets see if user may use this program */
   switch(get_permissions(&perm_buffer))
   {
      case NONE     : (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      if ((perm_buffer[0] == 'a') &&
                          (perm_buffer[1] == 'l') &&
                          (perm_buffer[2] == 'l') &&
                          ((perm_buffer[3] == '\0') ||
                           (perm_buffer[3] == ' ') ||
                           (perm_buffer[3] == '\t')))
                      {
                         free(perm_buffer);
                         break;
                      }
                      else if (posi(perm_buffer, EDIT_HC_PERM) == NULL)
                           {
                              (void)fprintf(stderr,
                                            "%s\n", PERMISSION_DENIED_STR);
                              exit(INCORRECT);
                           }
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      break;

      default       : (void)fprintf(stderr,
                                    "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   /* Check that no one else is using this dialog. */
   if ((p_user = lock_proc(EDIT_HC_LOCK_ID, NO)) != NULL)
   {
      (void)fprintf(stderr,
                    "Only one user may use this dialog. Currently %s is using it.\n",
                    p_user);
      exit(INCORRECT);
   }

   /* Get the font if supplied. */
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, "fixed");
   }

   /* Create and open sys_log fifo. */
   (void)strcpy(sys_log_fifo, p_work_dir);
   (void)strcat(sys_log_fifo, FIFO_DIR);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Allocate memory to store all changes */
   if ((ce = (struct changed_entry *)malloc(no_of_hosts * sizeof(struct changed_entry))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Get display pointer */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "Host Config ");
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

   return;
}


/*++++++++++++++++++++++++ create_option_menu_pt() +++++++++++++++++++++++*/
static void
create_option_menu_pt(Widget parent, Widget label_w, XmFontList fontlist)
{
   int      i;
   char     button_name[MAX_INT_LENGTH];
   Widget   pane_w;
   Arg      args[MAXARGS];
   Cardinal argcount;


   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      1);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   20);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   pt.option_menu_w = XmCreateOptionMenu(parent, "parallel_transfer",
                                         args, argcount);
   XtManageChild(pt.option_menu_w);

   /* Add all possible buttons. */
   for (i = 1; i <= MAX_NO_PARALLEL_JOBS; i++)
   {
      (void)sprintf(button_name, "%d", i);
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      pt.value[i - 1] = i;
      pt.button_w[i - 1] = XtCreateManagedWidget(button_name,
                                             xmPushButtonWidgetClass,
                                             pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(pt.button_w[i - 1], XmNactivateCallback, pt_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*++++++++++++++++++++++++ create_option_menu_tb() +++++++++++++++++++++++*/
static void
create_option_menu_tb(Widget parent, Widget label_w, XmFontList fontlist)
{
   int      i;
   char     *blocksize_name[MAX_TB_BUTTONS] =
            {
               "256 B",
               "512 B",
               "1 KB",
               "2 KB",
               "4 KB",
               "8 KB",
               "16 KB",
               "64 KB"
            };
   Widget   pane_w;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      21);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   40);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   tb.option_menu_w = XmCreateOptionMenu(parent, "transfer_blocksize",
                                         args, argcount);
   XtManageChild(tb.option_menu_w);

   /* Add all possible buttons. */
   for (i = 0; i < MAX_TB_BUTTONS; i++)
   {
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      tb.button_w[i] = XtCreateManagedWidget(blocksize_name[i],
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(tb.button_w[i], XmNactivateCallback, tb_option_changed,
                    (XtPointer)i);
   }

   tb.value[0] = 256; tb.value[1] = 512; tb.value[2] = 1024;
   tb.value[3] = 2048; tb.value[4] = 4096; tb.value[5] = 8192;
   tb.value[6] = 16384; tb.value[7] = 65536;

   return;
}


/*+++++++++++++++++++++++ create_option_menu_fso() +++++++++++++++++++++++*/
static void
create_option_menu_fso(Widget parent, Widget label_w, XmFontList fontlist)
{
   int      i;
   char     button_name[MAX_INT_LENGTH];
   Widget   pane_w;
   Arg      args[MAXARGS];
   Cardinal argcount;

   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId, pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      41);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   fso.option_menu_w = XmCreateOptionMenu(parent, "file_size_offset",
                                          args, argcount);
   XtManageChild(fso.option_menu_w);

   /* Add all possible buttons. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   fso.value[0] = -1;
   fso.button_w[0] = XtCreateManagedWidget("None",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[0], XmNactivateCallback, fso_option_changed,
                 (XtPointer)0);
   fso.value[1] = AUTO_SIZE_DETECT;
   fso.button_w[1] = XtCreateManagedWidget("Auto",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[1], XmNactivateCallback, fso_option_changed,
                 (XtPointer)1);
   for (i = 2; i < MAX_FSO_BUTTONS; i++)
   {
      (void)sprintf(button_name, "%d", i);
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      fso.value[i] = i;
      fso.button_w[i] = XtCreateManagedWidget(button_name,
                                              xmPushButtonWidgetClass,
                                              pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(fso.button_w[i], XmNactivateCallback, fso_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*+++++++++++++++++++++++ create_option_menu_nob() +++++++++++++++++++++++*/
static void
create_option_menu_nob(Widget parent, Widget label_w, XmFontList fontlist)
{
   int      i;
   char     button_name[MAX_INT_LENGTH];
   Widget   pane_w;
   Arg      args[MAXARGS];
   Cardinal argcount;


   /* Create a pulldown pane and attach it to the option menu */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      61);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   80);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       label_w);
   argcount++;
   nob.option_menu_w = XmCreateOptionMenu(parent, "no_of_no_burst",
                                          args, argcount);
   XtManageChild(nob.option_menu_w);

   /* Add all possible buttons. */
   for (i = 0; i <= MAX_NO_PARALLEL_JOBS; i++)
   {
      (void)sprintf(button_name, "%d", i);
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      nob.value[i] = i;
      nob.button_w[i] = XtCreateManagedWidget(button_name,
                                              xmPushButtonWidgetClass,
                                              pane_w, args, argcount);

      /* Add callback handler */
      XtAddCallback(nob.button_w[i], XmNactivateCallback, nob_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*+++++++++++++++++++++++++++ init_widget_data() ++++++++++++++++++++++++*/
static void
init_widget_data(void)
{
   int      i;
   XmString item;
   Arg      args[MAXARGS];
   Cardinal argcount;
   Pixmap   icon,
            iconmask;
   Display  *sdisplay = XtDisplay(host_list_w);
   Window   win = XtWindow(host_list_w);

   for (i = 0; i < no_of_hosts; i++)
   {
      item = XmStringCreateLocalized(fsa[i].host_alias);
      XmListAddItemUnselected(host_list_w, item, i + 1);
      XmStringFree(item);

      /* Initialize array holding all changed entries. */
      ce[i].value_changed = 0;
      ce[i].real_hostname[0][0] = -1;
      ce[i].real_hostname[1][0] = -1;
      ce[i].proxy_name[0] = -1;
      ce[i].transfer_timeout = -1L;
      ce[i].retry_interval = -1;
      ce[i].max_errors = -1;
      ce[i].max_successful_retries = -1;
      ce[i].allowed_transfers = -1;
      ce[i].block_size = -1;
      ce[i].file_size_offset = -3;
      if (fsa[i].host_toggle_str[0] == '\0')
      {
         ce[i].host_toggle[0][0] = '1';
         ce[i].host_toggle[1][0] = '2';
         ce[i].host_switch_toggle = OFF;
         ce[i].auto_toggle = OFF;
      }
      else
      {
         ce[i].host_toggle[0][0] = fsa[i].host_toggle_str[HOST_ONE];
         ce[i].host_toggle[1][0] = fsa[i].host_toggle_str[HOST_TWO];
         ce[i].host_switch_toggle = ON;
         if (fsa[i].auto_toggle == ON)
         {
            ce[i].auto_toggle = ON;
         }
         else
         {
            ce[i].auto_toggle = OFF;
         }
      }
   }

   /*
    * Create source cursor for drag & drop
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_bits,
                                    source_width,
                                    source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_mask_bits,
                                    source_mask_width,
                                    source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   source_icon_w = XmCreateDragIcon(host_list_w, "source_icon",
                                    args, argcount);

   /*
    * Create invalid source cursor for drag & drop
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_bits,
                                    no_source_width,
                                    no_source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_mask_bits,
                                    no_source_mask_width,
                                    no_source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  no_source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, no_source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   no_source_icon_w = XmCreateDragIcon(host_list_w, "no_source_icon",
                      args, argcount);

   /* Select the first host */
   if (no_of_hosts > 0)
   {
      XmListSelectPos(host_list_w, 1, True);
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
