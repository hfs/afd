/*
 *  log_callbacks.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   log_callbacks - all callback functions for module show_log
 **
 ** SYNOPSIS
 **   log_callbacks
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
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ScrollBar.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_log.h"
#include "logdefs.h"

extern Display        *display;
extern Widget         log_output,
                      counterbox,
                      selectlog,
                      selectscroll;
extern XmTextPosition wpr_position;
extern int            log_type_flag,
                      toggles_set,
                      line_counter,
                      current_log_number;
extern unsigned int   toggles_set_parallel_jobs,
                      total_length;
extern ino_t          current_inode_no;
extern char           **hosts,
                      log_dir[],
                      log_type[],
                      log_name[];
extern DIR            *dp;
extern FILE           *p_log_file;


/*############################## toggled() ##############################*/
void
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (int)client_data;

   return;
}


/*########################### toggled_jobs() ############################*/
void
toggled_jobs(Widget w, XtPointer client_data, XtPointer call_data)
{
#ifdef _TOGGLED_PROC_SELECTION
   unsigned int flag = 1 << (int)client_data;

   toggles_set_parallel_jobs ^= flag;
#else
   toggles_set_parallel_jobs = (int)client_data;
#endif

   return;
}


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (hosts != NULL)
   {
      FREE_RT_ARRAY(hosts);
   }
   if (p_log_file != NULL)
   {
      (void)fclose(p_log_file);
   }

   exit(0);
}


/*########################### update_button() ###########################*/
void
update_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char log_file[MAX_PATH_LENGTH],
        str_line[MAX_LINE_COUNTER_DIGITS];

   rewinddir(dp);
   if (p_log_file != NULL)
   {
      (void)fclose(p_log_file);
   }

   /* Get slider position so we can determine the current log number */
   XtVaGetValues(selectscroll,
                 XmNvalue, &current_log_number,
                 NULL);

   /* Reread log file */
   (void)sprintf(log_file, "%s/%s%d", log_dir, log_name, current_log_number);
   if ((p_log_file = fopen(log_file, "r")) == NULL)
   {
      char *ptr,
           error_line[80];

      if (errno != ENOENT)
      {
         (void)xrec(w, FATAL_DIALOG, "Could not fopen() %s : %s (%s %d)",
                    log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }

      ptr = log_file + strlen(log_file);

      /* Cut off the path */
      while ((*ptr != '/') && (ptr != log_file))
      {
         ptr--;
      }
      if (*ptr == '/')
      {
         *ptr = '\0';
         (void)sprintf(error_line,
                       "\n\n\n\n\t\tSorry, %s is not available!\n",
                       ptr + 1);
         *ptr = '/';
      }
      else
      {
         (void)sprintf(error_line,
                       "\n\n\n\n\t\tSorry, %s is not available!\n",
                       ptr);
      }

      XmTextSetInsertionPosition(log_output, 0);
      XmTextSetString(log_output, NULL);  /* Clears all old entries */
      XmTextSetString(log_output, error_line);
      XFlush(display);

      return;
   }
   if ((log_type_flag != TRANSFER_LOG_TYPE) &&
       (log_type_flag != RECEIVE_LOG_TYPE) &&
       (current_log_number == 0))
   {
      struct stat stat_buf;

      if (fstat(fileno(p_log_file), &stat_buf) == -1)
      {
         (void)fprintf(stderr, "ERROR   : Could not fstat() %s : %s (%s %d)\n",
                       log_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      current_inode_no = stat_buf.st_ino;
   }
   line_counter = 0;
   wpr_position = 0;
   total_length = 0;
   XmTextSetInsertionPosition(log_output, wpr_position);
   XmTextSetString(log_output, NULL);  /* Clears all old entries */
   init_text();

   /* Show line_counter if it is zero */
   if (line_counter == 0)
   {
      (void)sprintf(str_line, "%*d",
                    MAX_LINE_COUNTER_DIGITS, line_counter);
      XmTextSetString(counterbox, str_line);
   }

   return;
}


#ifdef _WITH_SCROLLBAR
/*############################## slider_moved() #########################*/
void
slider_moved(Widget    w,
             XtPointer client_data,
             XtPointer call_data)
{
   char                      str_line[MAX_INT_LENGTH];
   XmString                  text;
   XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *)call_data;

   (void)sprintf(str_line, "%d", cbs->value);
   text = XmStringCreateLocalized(str_line);
   XtVaSetValues(selectlog,
                 XmNlabelString, text,
                 NULL);
   XmStringFree(text);

   return;
}
#endif
