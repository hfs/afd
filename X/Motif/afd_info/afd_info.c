/*
 *  afd_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_info - displays information on a single host
 **
 ** SYNOPSIS
 **   afd_info [--version] [-w <work dir>] [-f <font name>] -h host-name
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.11.1996 H.Kiehl Created
 **   06.10.1997 H.Kiehl Take real hostname when displaying the IP number.
 **   01.06.1998 H.Kiehl Show real host names and protocols.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <time.h>                /* strftime(), gmtime()                 */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>              /* getcwd(), gethostname()              */
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
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <errno.h>
#include "active_passive.h"
#include "afd_info.h"
#include "version.h"

/* Global variables */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_host;
Widget                     protocol_label,
                           toplevel,
                           text_wl[NO_OF_FSA_ROWS],
                           text_wr[NO_OF_FSA_ROWS],
                           label_l_widget[NO_OF_FSA_ROWS],
                           label_r_widget[NO_OF_FSA_ROWS],
                           info_w,
                           pll_widget,  /* Pixmap label left  */
                           plr_widget;  /* Pixmap label right */
Pixmap                     active_pixmap,
                           passive_pixmap;
Colormap                   default_cmap;
int                        sys_log_fd = STDERR_FILENO,
                           no_of_hosts,
                           fsa_id,
                           fsa_fd = -1,
                           host_position;
unsigned long              color_pool[COLOR_POOL_SIZE];
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       info_file[MAX_PATH_LENGTH],
                           host_name[MAX_HOSTNAME_LENGTH + 1],
                           font_name[40],
                           host_alias_1[40],
                           host_alias_2[40],
                           *info_data = NULL,
                           protocol_label_str[60],
                           *p_work_dir,
                           label_l[NO_OF_FSA_ROWS][40] =
                           {
                              "",
                              "Real host name 1:",
                              "Files transfered:",
                              "Last connection :",
                              "Total errors    :"
                           },
                           label_r[NO_OF_FSA_ROWS][40] =
                           {
                              "",
                              "Real host name 2     :",
                              "Bytes transfered     :",
                              "No. of connections   :",
                              "Retry interval (min) :"
                           };
struct filetransfer_status *fsa;
struct prev_values         prev;

/* Local function prototypes */
static void                init_afd_info(int *, char **),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             i,
                   length;
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   str_line[MAX_INFO_STRING_LENGTH],
                   tmp_str_line[MAX_INFO_STRING_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".afd_info.form*background : NavajoWhite2",
                      ".afd_info.form.fsa_box.?.?.?.text_wl.background : NavajoWhite1",
                      ".afd_info.form.fsa_box.?.?.?.text_wr.background : NavajoWhite1",
                      ".afd_info.form.host_infoSW.host_info.background : NavajoWhite1",
                      ".afd_info.form.buttonbox*background : PaleVioletRed2",
                      ".afd_info.form.buttonbox*foreground : Black",
                      ".afd_info.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form,
                   fsa_box,
                   fsa_box1,
                   fsa_box2,
                   fsa_text,
                   button,
                   buttonbox,
                   rowcol1,
                   rowcol2,
                   h_separator1,
                   h_separator2,
                   v_separator;
   Pixel           default_background;
   XmFontListEntry entry;
   XmFontList      fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   struct stat     stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values */
   p_work_dir = work_dir;
   init_afd_info(&argc, argv);

   (void)strcpy(window_title, host_name);
   (void)strcat(window_title, " Info");
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   toplevel = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);
   display = XtDisplay(toplevel);

   /* Create managing widget */
   form = XmCreateForm(toplevel, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /* Prepare pixmaps */
   XtVaGetValues(form,
                 XmNbackground, &default_background,
                 XmNcolormap, &default_cmap,
                 NULL);
   init_color(display);
   ximage.width = ACTIVE_PASSIVE_WIDTH;
   ximage.height = ACTIVE_PASSIVE_HEIGHT;
   ximage.data = active_passive_bits;
   ximage.xoffset = 0;
   ximage.format = XYBitmap;
   ximage.byte_order = MSBFirst;
   ximage.bitmap_pad = 8;
   ximage.bitmap_bit_order = LSBFirst;
   ximage.bitmap_unit = 8;
   ximage.depth = 1;
   ximage.bytes_per_line = 2;
   ximage.obdata = NULL;
   if (XmInstallImage(&ximage, "active") == True)
   {
      active_pixmap = XmGetPixmap(XtScreen(toplevel),
                                         "active",
                                         color_pool[NORMAL_STATUS], /* Foreground */
                                         default_background);/* Background */
   }
   if (XmInstallImage(&ximage, "passive") == True)
   {
      passive_pixmap = XmGetPixmap(XtScreen(toplevel),
                                          "passive",
                                          color_pool[BUTTON_BACKGROUND], /* Foreground */
                                          default_background);/* Background */
   }

   /* Create host label for host name */
   if ((fsa[host_position].host_toggle_str[0] != '\0') &&
       (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
       (passive_pixmap != XmUNSPECIFIED_PIXMAP))
   {
      (void)sprintf(label_l[0], "%*c%-*s :",
                    3, ' ', (FSA_INFO_TEXT_WIDTH_L - 3), host_alias_1);
      (void)sprintf(label_r[0], "%*c%-*s :",
                    3, ' ', (FSA_INFO_TEXT_WIDTH_R - 1), host_alias_2);
   }
   else
   {
      (void)sprintf(label_l[0], "%-*s :",
                    FSA_INFO_TEXT_WIDTH_L, host_alias_1);
      (void)sprintf(label_r[0], "%-*s :",
                    FSA_INFO_TEXT_WIDTH_R + 2, host_alias_2);
   }

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   fsa_box = XmCreateForm(form, "fsa_box", args, argcount);
   XtManageChild(fsa_box);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   fsa_box1 = XmCreateForm(fsa_box, "fsa_box1", args, argcount);
   XtManageChild(fsa_box1);

   rowcol1 = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                              fsa_box1, NULL);
   for (i = 0; i < NO_OF_FSA_ROWS; i++)
   {
      fsa_text = XtVaCreateWidget("fsa_text", xmFormWidgetClass,
                                  rowcol1,
                                  XmNfractionBase, 41,
                                  NULL);
      if ((i == 0) && (fsa[host_position].host_toggle_str[0] != '\0') &&
          (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (passive_pixmap != XmUNSPECIFIED_PIXMAP))
      {
         if (fsa[host_position].host_toggle == HOST_ONE)
         {
            pll_widget = XtVaCreateManagedWidget("pixmap_label_l", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      active_pixmap,
                                    NULL);
         }
         else
         {
            pll_widget = XtVaCreateManagedWidget("pixmap_label_l", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      passive_pixmap,
                                    NULL);
         }
      }
      label_l_widget[i] = XtVaCreateManagedWidget(label_l[i], xmLabelGadgetClass, fsa_text,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wl[i] = XtVaCreateManagedWidget("text_wl", xmTextWidgetClass, fsa_text,
                                           XmNfontList,              fontlist,
                                           XmNcolumns,               AFD_INFO_LENGTH,
                                           XmNtraversalOn,           False,
                                           XmNeditable,              False,
                                           XmNcursorPositionVisible, False,
                                           XmNmarginHeight,          1,
                                           XmNmarginWidth,           1,
                                           XmNshadowThickness,       1,
                                           XmNhighlightThickness,    0,
                                           XmNrightAttachment,       XmATTACH_FORM,
                                           XmNleftAttachment,        XmATTACH_POSITION,
                                           XmNleftPosition,          20,
                                           NULL);
      XtManageChild(fsa_text);
   }
   XtManageChild(rowcol1);

   /* Fill up the text widget with some values */
   if (stat(SYSTEM_HOST_FILE, &stat_buf) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to stat() %s : %s (%s %d)\n",
                    SYSTEM_HOST_FILE, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   prev.host_file_time = stat_buf.st_mtime;
   if ((fsa[host_position].protocol & FTP_FLAG) ||
#ifdef _WITH_MAP_SUPPORT
       (fsa[host_position].protocol & MAP_FLAG) ||
#endif /* _WITH_MAP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
       (fsa[host_position].protocol & WMO_FLAG) ||
#endif /* _WITH_WMO_SUPPORT */
       (fsa[host_position].protocol & SMTP_FLAG))
   {
      get_ip_no(fsa[host_position].real_hostname[0], tmp_str_line);
   }
   else
   {
      *tmp_str_line = '\0';
   }
   (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
   XmTextSetString(text_wl[0], str_line);
   (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, prev.real_hostname[0]);
   XmTextSetString(text_wl[1], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.files_send);
   XmTextSetString(text_wl[2], str_line);
   (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                  gmtime(&prev.last_connection));
   (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
   XmTextSetString(text_wl[3], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.total_errors);
   XmTextSetString(text_wl[4], str_line);

   /* Create the first horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             fsa_box);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   /* Create the vertical separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       fsa_box1);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator = XmCreateSeparator(fsa_box, "v_separator", args, argcount);
   XtManageChild(v_separator);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator);
   argcount++;
   fsa_box2 = XmCreateForm(fsa_box, "fsa_box2", args, argcount);
   XtManageChild(fsa_box2);

   rowcol2 = XtVaCreateWidget("rowcol2", xmRowColumnWidgetClass,
                              fsa_box2, NULL);
   for (i = 0; i < NO_OF_FSA_ROWS; i++)
   {
      fsa_text = XtVaCreateWidget("fsa_text", xmFormWidgetClass,
                                  rowcol2,
                                  XmNfractionBase, 41,
                                  NULL);
      if ((i == 0) && (fsa[host_position].host_toggle_str[0] != '\0') &&
          (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (passive_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (fsa[host_position].toggle_pos != 0))
      {
         if (fsa[host_position].host_toggle == HOST_ONE)
         {
            plr_widget = XtVaCreateManagedWidget("pixmap_label_r", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      passive_pixmap,
                                    NULL);
         }
         else
         {
            plr_widget = XtVaCreateManagedWidget("pixmap_label_r", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      active_pixmap,
                                    NULL);
         }
      }
      label_r_widget[i] = XtVaCreateManagedWidget(label_r[i], xmLabelGadgetClass, fsa_text,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wr[i] = XtVaCreateManagedWidget("text_wr", xmTextWidgetClass, fsa_text,
                                           XmNfontList,              fontlist,
                                           XmNcolumns,               AFD_INFO_LENGTH,
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
      XtManageChild(fsa_text);
   }
   XtManageChild(rowcol2);

   /* Fill up the text widget with some values */
   if (prev.toggle_pos != 0)
   {
      if ((fsa[host_position].protocol & FTP_FLAG) ||
#ifdef _WITH_MAP_SUPPORT
          (fsa[host_position].protocol & MAP_FLAG) ||
#endif /* _WITH_MAP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
          (fsa[host_position].protocol & WMO_FLAG) ||
#endif /* _WITH_WMO_SUPPORT */
          (fsa[host_position].protocol & SMTP_FLAG))
      {
         get_ip_no(fsa[host_position].real_hostname[1], tmp_str_line);
      }
      else
      {
         *tmp_str_line = '\0';
      }
      (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
      XmTextSetString(text_wr[0], str_line);
   }
   (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, prev.real_hostname[1]);
   XmTextSetString(text_wr[1], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.bytes_send);
   XmTextSetString(text_wr[2], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.no_of_connections);
   XmTextSetString(text_wr[3], str_line);
   (void)sprintf(str_line, "%*d", AFD_INFO_LENGTH, prev.retry_interval / 60);
   XmTextSetString(text_wr[4], str_line);

   length = sprintf(protocol_label_str, "Protocols : ");
   if (fsa[host_position].protocol & FTP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "FTP ");
   }
   if (fsa[host_position].protocol & LOC_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "LOC ");
   }
   if (fsa[host_position].protocol & SMTP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "SMTP ");
   }
#ifdef _WITH_MAP_SUPPORT
   if (fsa[host_position].protocol & MAP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "MAP ");
   }
#endif
#ifdef _WITH_WMO_SUPPORT
   if (fsa[host_position].protocol & WMO_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "WMO ");
   }
#endif
   protocol_label = XtVaCreateManagedWidget(protocol_label_str,
                                            xmLabelGadgetClass, form,
                                            XmNfontList,        fontlist,
                                            XmNtopAttachment,   XmATTACH_WIDGET,
                                            XmNtopWidget,       h_separator1,
                                            XmNleftAttachment,  XmATTACH_FORM,
                                            XmNrightAttachment, XmATTACH_FORM,
                                            NULL);

   /* Create the second first horizontal separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             protocol_label);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox = XmCreateForm(form, "buttonbox", args, argcount);

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

   button = XtVaCreateManagedWidget("Close",
                                    xmPushButtonWidgetClass, buttonbox,
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
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox);

   /* Create log_text as a ScrolledText window */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNrows,                   10);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              3);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             3);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            3);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           3);
   argcount++;
   info_w = XmCreateScrolledText(form, "host_info", args, argcount);
   XtManageChild(info_w);
   XtManageChild(form);

   /* Free font list */
   XmFontListFree(fontlist);

#ifdef _EDITRES
   XtAddEventHandler(toplevel, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets */
   XtRealizeWidget(toplevel);
   wait_visible(toplevel);

   /* Read and display the information file */
   check_info_file();

   /* Call update_info() after UPDATE_INTERVAL ms */
   interval_id_host = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      form);

   /* We want the keyboard focus on the Done button */
   XmProcessTraversal(button, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_afd_info() ++++++++++++++++++++++++++*/
static void
init_afd_info(int *argc, char *argv[])
{
   char *ptr;

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
   if (get_arg(argc, argv, "-h", host_name,
               MAX_HOSTNAME_LENGTH + 1) == INCORRECT)
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

   /* Attach to the FSA */
   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((host_position = get_host_position(fsa, host_name, no_of_hosts)) < 0)
   {
      (void)fprintf(stderr, "Host %s is not in FSA.\n", host_name);
      exit(INCORRECT);
   }

   if (fsa[host_position].toggle_pos == 0)
   {
      /* There is NO secondary host */
      (void)strcpy(host_alias_1, host_name);
      (void)strcpy(host_alias_2, NO_SECODARY_HOST);
   }
   else
   {
      /* There IS a secondary host */
      (void)strcpy(host_alias_1, host_name);
      ptr = host_alias_1 + strlen(host_alias_1);
      *ptr = fsa[host_position].host_toggle_str[1];
      *(ptr + 1) = '\0';
      (void)strcpy(host_alias_2, host_name);
      ptr = host_alias_2 + strlen(host_alias_2);
      *ptr = fsa[host_position].host_toggle_str[2];
      *(ptr + 1) = '\0';
   }

   /* Initialize values in FSA structure */
   (void)strcpy(prev.real_hostname[0], fsa[host_position].real_hostname[0]);
   (void)strcpy(prev.real_hostname[1], fsa[host_position].real_hostname[1]);
   prev.retry_interval = fsa[host_position].retry_interval;
   prev.files_send = fsa[host_position].file_counter_done;
   prev.bytes_send = fsa[host_position].bytes_send;
   prev.total_errors = fsa[host_position].total_errors;
   prev.no_of_connections = fsa[host_position].connections;
   prev.last_connection = fsa[host_position].last_connection;
   prev.host_toggle = fsa[host_position].host_toggle;
   prev.toggle_pos = fsa[host_position].toggle_pos;
   prev.protocol = fsa[host_position].protocol;

   /* Create name of info file and read it */
   (void)sprintf(info_file, "%s%s/%s%s", p_work_dir,
                 ETC_DIR, INFO_IDENTIFIER, host_name);

   return;
}


/*------------------------------ usage() --------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -h host-name\n", progname);
   (void)fprintf(stderr, "            --version\n");
   (void)fprintf(stderr, "            -f <font name>\n");
   (void)fprintf(stderr, "            -w <work directory>\n");
   return;
}
