/*
 *  update_info.c - Part of AFD, an automatic file distribution program.
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
 **   update_info - updates any information that changes for module
 **                 mon_info
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
 **   21.02.1999 H.Kiehl Created
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
#include "afd_ctrl.h"
#include "mon_info.h"
#include "mondefs.h"

/* external global variables */
extern int                    afd_position,
                              msa_id;
extern off_t                  msa_size;
extern char                   label_l[NO_OF_MSA_ROWS][21],
                              label_r[NO_OF_MSA_ROWS][17],
                              *p_work_dir;
extern Display                *display;
extern XtIntervalId           interval_id_host;
extern XtAppContext           app;
extern Widget                 text_wl[NO_OF_MSA_ROWS],
                              text_wr[NO_OF_MSA_ROWS],
                              label_l_widget[NO_OF_MSA_ROWS],
                              label_r_widget[NO_OF_MSA_ROWS];
extern struct mon_status_area *msa;
extern struct prev_values     prev;


/*############################ update_info() ############################*/
void
update_info(w)
Widget w;
{
   static int  interval = 0;
   signed char flush = NO;
   char        str_line[MAX_INFO_STRING_LENGTH],
               tmp_str_line[MAX_INFO_STRING_LENGTH];

   /* Check if MSA changed */
   (void)check_msa();

   if (strcmp(prev.real_hostname, msa[afd_position].hostname) != 0)
   {
      (void)strcpy(prev.real_hostname, msa[afd_position].hostname);
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.real_hostname);
      XmTextSetString(text_wl[0], str_line);
      flush = YES;
   }

   if (prev.port != msa[afd_position].port)
   {
      prev.port = msa[afd_position].port;
      (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.port);
      XmTextSetString(text_wl[1], str_line);
      flush = YES;
   }

   if (strcmp(prev.r_work_dir, msa[afd_position].r_work_dir) != 0)
   {
      (void)strcpy(prev.r_work_dir, msa[afd_position].r_work_dir);
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, prev.r_work_dir);
      XmTextSetString(text_wr[1], str_line);
      flush = YES;
   }

   if (prev.last_data_time != msa[afd_position].last_data_time)
   {
      prev.last_data_time = msa[afd_position].last_data_time;
      (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                     gmtime(&prev.last_data_time));
      (void)sprintf(str_line, "%*s", MON_INFO_LENGTH, tmp_str_line);
      XmTextSetString(text_wl[2], str_line);
      flush = YES;
   }

   if (prev.poll_interval != msa[afd_position].poll_interval)
   {
      prev.poll_interval = msa[afd_position].poll_interval;
      (void)sprintf(str_line, "%*d", MON_INFO_LENGTH, prev.poll_interval);
      XmTextSetString(text_wr[2], str_line);
      flush = YES;
   }

   if (prev.max_connections != msa[afd_position].max_connections)
   {
      prev.max_connections = msa[afd_position].max_connections;
      (void)sprintf(str_line, "%*u", MON_INFO_LENGTH, prev.max_connections);
      XmTextSetString(text_wl[3], str_line);
      flush = YES;
   }

   if (prev.no_of_hosts != msa[afd_position].no_of_hosts)
   {
      prev.no_of_hosts = msa[afd_position].no_of_hosts;
      (void)sprintf(str_line, "%*u", MON_INFO_LENGTH, prev.no_of_hosts);
      XmTextSetString(text_wr[3], str_line);
      flush = YES;
   }

   if (interval++ == FILE_UPDATE_INTERVAL)
   {
      interval = 0;

      /* Check if the information file for this AFD has changed */
      flush = check_info_file();
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
