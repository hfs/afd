/*
 *  update_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   update_info - updates any information that changes for module
 **                 afd_info
 **
 ** SYNOPSIS
 **   void update_info(Widget w)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.11.1996 H.Kiehl Created
 **   06.10.1997 H.Kiehl Take real hostname when displaying the IP number.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <time.h>             /* strftime(), gmtime()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#ifdef _WITH_XPM
#include <X11/xpm.h>
#endif
#include "afd_ctrl.h"
#include "afd_info.h"

/* external global variables */
extern int                        host_position,
                                  fsa_id;
#ifndef _NO_MMAP
extern off_t                      fsa_size;
#endif
extern char                       host_name[MAX_HOSTNAME_LENGTH + 1],
                                  host_alias_1[40],
                                  host_alias_2[40],
                                  label_l[NO_OF_FSA_ROWS][40],
                                  label_r[NO_OF_FSA_ROWS][40],
                                  protocol_label_str[],
                                  *p_work_dir;
extern Display                    *display;
extern XtIntervalId               interval_id_host;
extern XtAppContext               app;
extern Widget                     protocol_label,
                                  text_wl[NO_OF_FSA_ROWS],
                                  text_wr[NO_OF_FSA_ROWS],
                                  label_l_widget[NO_OF_FSA_ROWS],
                                  label_r_widget[NO_OF_FSA_ROWS];
extern struct filetransfer_status *fsa;
extern struct prev_values         prev;
#ifdef _WITH_XPM
extern int                        pix_stat;    /* Return status of       */
                                               /* pixmap creation.       */
extern Widget                     pll_widget,  /* Pixmap label left      */
                                  plr_widget;  /* Pixmap label right     */
extern Pixmap                     main_pixmap,
                                  secondary_pixmap;
#endif /* _WITH_XPM */


/*############################ update_info() ############################*/
void
update_info(w)
Widget w;
{
   static int  interval = 0;
   signed char flush = NO;
   char        *ptr,
               str_line[MAX_INFO_STRING_LENGTH],
               tmp_str_line[MAX_INFO_STRING_LENGTH];

   /* Check if FSA changed */
   (void)check_fsa();

   if (prev.protocol != fsa[host_position].protocol)
   {
      XmString text;
      size_t   length;

      prev.protocol = fsa[host_position].protocol;
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
      text = XmStringCreateLocalized(protocol_label_str);
      XtVaSetValues(protocol_label, XmNlabelString, text, NULL);
      XmStringFree(text);
   }

   if (strcmp(prev.real_hostname[0], fsa[host_position].real_hostname[0]) != 0)
   {
      (void)strcpy(prev.real_hostname[0], fsa[host_position].real_hostname[0]);
      (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, prev.real_hostname[0]);
      XmTextSetString(text_wl[1], str_line);
      flush = YES;
   }

   if (strcmp(prev.real_hostname[1], fsa[host_position].real_hostname[1]) != 0)
   {
      (void)strcpy(prev.real_hostname[1], fsa[host_position].real_hostname[1]);
      (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, prev.real_hostname[1]);
      XmTextSetString(text_wr[1], str_line);
      flush = YES;
   }

   if (prev.retry_interval != fsa[host_position].retry_interval)
   {
      prev.retry_interval = fsa[host_position].retry_interval;
      (void)sprintf(str_line, "%*d",
                    AFD_INFO_LENGTH, prev.retry_interval / 60);
      XmTextSetString(text_wr[4], str_line);
      flush = YES;
   }

   if (prev.files_send != fsa[host_position].file_counter_done)
   {
      prev.files_send = fsa[host_position].file_counter_done;
      (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.files_send);
      XmTextSetString(text_wl[2], str_line);
      flush = YES;
   }

   if (prev.bytes_send != fsa[host_position].bytes_send)
   {
      prev.bytes_send = fsa[host_position].bytes_send;
      (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.bytes_send);
      XmTextSetString(text_wr[2], str_line);
      flush = YES;
   }

   if (prev.total_errors != fsa[host_position].total_errors)
   {
      prev.total_errors = fsa[host_position].total_errors;
      (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.total_errors);
      XmTextSetString(text_wl[4], str_line);
      flush = YES;
   }

   if (prev.no_of_connections != fsa[host_position].connections)
   {
      prev.no_of_connections = fsa[host_position].connections;
      (void)sprintf(str_line, "%*u", AFD_INFO_LENGTH, prev.no_of_connections);
      XmTextSetString(text_wr[3], str_line);
      flush = YES;
   }

   if (prev.last_connection != fsa[host_position].last_connection)
   {
      prev.last_connection = fsa[host_position].last_connection;
      (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     gmtime(&prev.last_connection));
      (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
      XmTextSetString(text_wl[3], str_line);
      flush = YES;
   }

   if (prev.toggle_pos != fsa[host_position].toggle_pos)
   {
      prev.toggle_pos = fsa[host_position].toggle_pos;

      if (prev.toggle_pos == 0)
      {
         /*
          * There is NO secondary host
          */

         /* Display the first host name */
         (void)strcpy(host_alias_1, host_name);
#ifdef _WITH_XPM
         if (pix_stat == XpmSuccess)
         {
            (void)sprintf(label_l[0], "%*c%-*s :",
                          3, ' ', (FSA_INFO_TEXT_WIDTH_L - 3), host_alias_1);
            XtVaSetValues(pll_widget,
                          XmNtopAttachment,    XmATTACH_POSITION,
                          XmNtopPosition,      1,
                          XmNbottomAttachment, XmATTACH_POSITION,
                          XmNbottomPosition,   40,
                          XmNleftAttachment,   XmATTACH_POSITION,
                          XmNleftPosition,     1,
                          XmNlabelType,        XmPIXMAP,
                          XmNlabelPixmap,      main_pixmap,
                          NULL);
         }
         else
         {
#endif /* _WITH_XPM */
            (void)sprintf(label_l[0], "%-*s :",
                          FSA_INFO_TEXT_WIDTH_L, host_alias_1);
#ifdef _WITH_XPM
         }
#endif
         XtVaSetValues(label_l_widget[0],
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelString,      label_l[0],
                       NULL);

         /* Get IP for the first host */
         get_ip_no(fsa[host_position].real_hostname[0], tmp_str_line);
         (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
         XmTextSetString(text_wl[0], str_line);

         /* Display the second host name */
         (void)strcpy(host_alias_2, NO_SECODARY_HOST);
         (void)sprintf(label_r[0], "%-*s :",
                       FSA_INFO_TEXT_WIDTH_R, host_alias_2);
         XmTextSetString(text_wr[0], NULL);
      }
      else
      {
         /*
          * There IS a secondary host
          */

         /* Display the first host name */
         (void)strcpy(host_alias_1, host_name);
         ptr = host_alias_1 + strlen(host_alias_1);
         *ptr = fsa[host_position].host_toggle_str[1];
         *(ptr + 1) = '\0';
#ifdef _WITH_XPM
         if (pix_stat == XpmSuccess)
         {
            (void)sprintf(label_l[0], "%*c%-*s :",
                          3, ' ', (FSA_INFO_TEXT_WIDTH_L - 3), host_alias_1);
            if (fsa[host_position].host_toggle == HOST_ONE)
            {
               XtVaSetValues(pll_widget,
                             XmNtopAttachment,    XmATTACH_POSITION,
                             XmNtopPosition,      1,
                             XmNbottomAttachment, XmATTACH_POSITION,
                             XmNbottomPosition,   40,
                             XmNleftAttachment,   XmATTACH_POSITION,
                             XmNleftPosition,     1,
                             XmNlabelType,        XmPIXMAP,
                             XmNlabelPixmap,      main_pixmap,
                             NULL);
            }
            else
            {
               XtVaSetValues(pll_widget,
                             XmNtopAttachment,    XmATTACH_POSITION,
                             XmNtopPosition,      1,
                             XmNbottomAttachment, XmATTACH_POSITION,
                             XmNbottomPosition,   40,
                             XmNleftAttachment,   XmATTACH_POSITION,
                             XmNleftPosition,     1,
                             XmNlabelType,        XmPIXMAP,
                             XmNlabelPixmap,      secondary_pixmap,
                             NULL);
            }
         }
         else
         {
#endif /* _WITH_XPM */
            (void)sprintf(label_l[0], "%-*s :",
                          FSA_INFO_TEXT_WIDTH_L, host_alias_1);
#ifdef _WITH_XPM
         }
#endif
         XtVaSetValues(label_l_widget[0],
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelString,      label_l[0],
                       NULL);

         /* Get IP for the first host */
         get_ip_no(fsa[host_position].real_hostname[0], tmp_str_line);
         (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
         XmTextSetString(text_wl[0], str_line);

         /* Display the second host name */
         (void)strcpy(host_alias_2, host_name);
         ptr = host_alias_2 + strlen(host_alias_2);
         *ptr = fsa[host_position].host_toggle_str[2];
         *(ptr + 1) = '\0';
#ifdef _WITH_XPM
         if (pix_stat == XpmSuccess)
         {
            (void)sprintf(label_r[0], "%*c%-*s :",
                          3, ' ', (FSA_INFO_TEXT_WIDTH_R - 1), host_alias_2);
            if (fsa[host_position].host_toggle == HOST_ONE)
            {
               XtVaSetValues(plr_widget,
                             XmNtopAttachment,    XmATTACH_POSITION,
                             XmNtopPosition,      1,
                             XmNbottomAttachment, XmATTACH_POSITION,
                             XmNbottomPosition,   40,
                             XmNleftAttachment,   XmATTACH_POSITION,
                             XmNleftPosition,     1,
                             XmNlabelType,        XmPIXMAP,
                             XmNlabelPixmap,      secondary_pixmap,
                             NULL);
            }
            else
            {
               XtVaSetValues(plr_widget,
                             XmNtopAttachment,    XmATTACH_POSITION,
                             XmNtopPosition,      1,
                             XmNbottomAttachment, XmATTACH_POSITION,
                             XmNbottomPosition,   40,
                             XmNleftAttachment,   XmATTACH_POSITION,
                             XmNleftPosition,     1,
                             XmNlabelType,        XmPIXMAP,
                             XmNlabelPixmap,      main_pixmap,
                             NULL);
            }
         }
         else
         {
#endif /* _WITH_XPM */
            (void)sprintf(label_r[0], "%-*s :",
                          FSA_INFO_TEXT_WIDTH_R, host_alias_2);
#ifdef _WITH_XPM
         }
#endif
         XtVaSetValues(label_r_widget[0],
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelString,      label_r[0],
                       NULL);

         /* Get IP for the second host */
         get_ip_no(fsa[host_position].real_hostname[1], tmp_str_line);
         (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
         XmTextSetString(text_wr[0], str_line);
      }

      flush = YES;
   }

#ifdef _WITH_XPM
   /* Check if the host have been toggled */
   if ((fsa[host_position].toggle_pos != 0) &&
       (pix_stat == XpmSuccess) &&
       (prev.host_toggle != fsa[host_position].host_toggle))
   {
      prev.host_toggle = fsa[host_position].host_toggle;
      if (fsa[host_position].host_toggle == HOST_ONE)
      {
         XtVaSetValues(pll_widget,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelType,        XmPIXMAP,
                       XmNlabelPixmap,      main_pixmap,
                       NULL);
         XtVaSetValues(plr_widget,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelType,        XmPIXMAP,
                       XmNlabelPixmap,      secondary_pixmap,
                       NULL);
      }
      else
      {
         XtVaSetValues(pll_widget,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelType,        XmPIXMAP,
                       XmNlabelPixmap,      secondary_pixmap,
                       NULL);
         XtVaSetValues(plr_widget,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   40,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     1,
                       XmNlabelType,        XmPIXMAP,
                       XmNlabelPixmap,      main_pixmap,
                       NULL);
      }

      flush = YES;
   }
#endif /* _WITH_XPM */

   if (interval++ == FILE_UPDATE_INTERVAL)
   {
      struct stat stat_buf;

      interval = 0;

      /* Check if the system host file (/etc/hosts) has been changed */
      if (stat(SYSTEM_HOST_FILE, &stat_buf) < 0)
      {
         (void)xrec(w, FATAL_DIALOG, "Failed to stat() %s : %s (%s %d)",
                    SYSTEM_HOST_FILE, strerror(errno), __FILE__, __LINE__);
         return;
      }
      
      if (prev.host_file_time != stat_buf.st_mtime)
      {
         prev.host_file_time = stat_buf.st_mtime;
         get_ip_no(fsa[host_position].real_hostname[0], tmp_str_line);
         (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
         XmTextSetString(text_wl[0], str_line);
         if (fsa[host_position].toggle_pos != 0)
         {
            get_ip_no(fsa[host_position].real_hostname[1], tmp_str_line);
            (void)sprintf(str_line, "%*s", AFD_INFO_LENGTH, tmp_str_line);
            XmTextSetString(text_wr[0], str_line);
         }

         flush = YES;
      }
      
      /* Check if the information file for this host has changed */
      if (check_info_file() == YES)
      {
         flush = YES;
      }
   }

   if (flush == YES)
   {
      XFlush(display);
   }

   /* Call update_info() after UPDATE_INTERVAL ms */
   interval_id_host = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      NULL);

   return;
}
