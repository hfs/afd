/*
 *  show_dlog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __show_dlog_h
#define __show_dlog_h

#include "x_common_defs.h"

/* What information should be displayed */
#define SHOW_AGE_OUTPUT         1
#define SHOW_AGE_INPUT          2
#define SHOW_USER_DEL           4
#define SHOW_OTHER_DEL          8

#define AGE_OUTPUT_ID_STR       "AGE(O)"
#define AGE_INPUT_ID_STR        "AGE(I)"
#define USER_DEL_ID_STR         "USER"
#define OTHER_DEL_ID_STR        "OTHER"
#define UNKNOWN_ID_STR          "UNKNOWN"
#define AGE_OUTPUT_ID_LENGTH    6
#define AGE_INPUT_ID_LENGTH     6
#define USER_DEL_ID_LENGTH      4
#define OTHER_DEL_ID_LENGTH     5
#define UNKNOWN_ID_LENGTH       7
#define MAX_REASON_LENGTH       7

#define SELECTION_TOGGLE        1
#define ALL_TOGGLE              2
#define PRINTER_TOGGLE          3
#define FILE_TOGGLE             4

#define EQUAL_SIGN              1
#define LESS_THEN_SIGN          2
#define GREATER_THEN_SIGN       3

#define GOT_JOB_ID              -2
#define GOT_JOB_ID_DIR_ONLY     -3

#define F_KILOBYTE              1024.0
#define F_MEGABYTE              1048576.0
#define F_GIGABYTE              1073741824.0
#define F_TERABYTE              1099511627776.0

#define CONTROL_D               "\004"

#define SEARCH_BUTTON           1
#define STOP_BUTTON             2
#define STOP_BUTTON_PRESSED     4

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define START_TIME_NO_ENTER     1
#define START_TIME              2
#define END_TIME_NO_ENTER       3
#define END_TIME                4
#define FILE_NAME_NO_ENTER      5
#define FILE_NAME               6
#define DIRECTORY_NAME_NO_ENTER 7
#define DIRECTORY_NAME          8
#define FILE_LENGTH_NO_ENTER    9
#define FILE_LENGTH             10
#define RECIPIENT_NAME_NO_ENTER 11
#define RECIPIENT_NAME          12

#define NO_OF_VISIBLE_LINES     18
#define MAXARGS                 20

#define LINES_BUFFERED          1000
#define MAX_DISPLAYED_FILE_SIZE 10
#define MAX_PROC_USER_LENGTH    15
#define MAX_OUTPUT_LINE_LENGTH  16 + MAX_HOSTNAME_LENGTH + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + MAX_REASON_LENGTH + 1 + MAX_PROC_USER_LENGTH

#define FILE_SIZE_FORMAT        "Enter file size in bytes: [=<>]file size"
#define TIME_FORMAT             "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed */
#define SHOW_SHORT_FORMAT       26
#define SHOW_MEDIUM_FORMAT      40
#define SHOW_LONG_FORMAT        60
#define HEADING_LINE_SHORT      "Date   Time     File name                  File size   Hostname Reason  Process/User   "
#define SUM_SEP_LINE_SHORT      "======================================================================================="
#define HEADING_LINE_MEDIUM     "Date   Time     File name                                File size   Hostname Reason  Process/User   "
#define SUM_SEP_LINE_MEDIUM     "====================================================================================================="
#define HEADING_LINE_LONG       "Date   Time     File name                                                    File size   Hostname Reason  Process/User   "
#define SUM_SEP_LINE_LONG       "========================================================================================================================="

/* Structure that holds offset (to job ID) to each item in list. */
struct item_list
       {
          FILE *fp;
          int  no_of_items;
          int  *line_offset;   /* Array that contains the offset to the */
                               /* file name of that item.               */
          int  *offset;        /* Array that contains the offset to the */
                               /* ID of that item.                      */
          char *input_id;      /* If YES the ID is the directory ID     */
                               /* otherwise it is the job ID.           */
       };

/* Structure to hold the data for a single entry in the AMG history file. */
struct db_entry
       {
          int          no_of_files;
          int          no_of_loptions;
          int          no_of_soptions;
          char         *soptions;
          char         files[MAX_NO_FILES][MAX_FILENAME_LENGTH];
          char         loptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
          char         recipient[MAX_RECIPIENT_LENGTH];
          char         priority;
       };

/* Structure to hold all data for a single dir ID. */
struct info_data
       {
          unsigned int    job_no;
          int             count;       /* Counts number of dbe entries. */
          char            input_id;
          char            dir[MAX_PATH_LENGTH];
          char            file_name[MAX_FILENAME_LENGTH];
          char            proc_user[MAX_PROC_USER_LENGTH + 1];
          char            extra_reason[MAX_PATH_LENGTH];
          struct db_entry *dbe;
       };

/* Permission structure for show_dlog */
struct sol_perm
       {
          int  list_limit;
          char view_passwd;
       };

/* Various macro definitions. */
#define GET_TIME()                            \
        {                                     \
           register int i = 0;                \
                                              \
           while (*ptr != ' ')                \
           {                                  \
              time_buf[i++] = *(ptr++);       \
           }                                  \
           time_buf[i] = '\0';                \
           time_val = (time_t)atol(time_buf); \
        }
#define SHOW_MESSAGE()                                                        \
        {                                                                     \
           XmString xstr;                                                     \
                                                                              \
           xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG); \
           XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);            \
           XmStringFree(xstr);                                                \
        }
#define CHECK_INTERRUPT()                                                  \
        {                                                                  \
           XEvent event;                                                   \
                                                                           \
           XFlush(display);                                                \
           XmUpdateDisplay(toplevel_w);                                    \
                                                                           \
           while (XCheckMaskEvent(display, ButtonPressMask |               \
                  ButtonReleaseMask | ButtonMotionMask | KeyPressMask,     \
                  &event))                                                 \
           {                                                               \
              if ((event.xany.window == XtWindow(special_button_w)) ||     \
                  (event.xany.window == XtWindow(scrollbar_w)) ||          \
                  (event.xany.window == XtWindow(listbox_w)))              \
              {                                                            \
                 XtDispatchEvent(&event);                                  \
              }                                                            \
              else                                                         \
              {                                                            \
                 if (event.type != MotionNotify)                           \
                 {                                                         \
                    XBell(display, 50);                                    \
                 }                                                         \
              }                                                            \
           }                                                               \
        }

/* Function prototypes */
extern void calculate_summary(char *, time_t, time_t, unsigned int, double),
            toggled(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            close_button(Widget, XtPointer, XtPointer),
            format_input_info(char *),
            format_output_info(char *),
            get_info(int, char),
            get_data(void),
            info_click(Widget, XtPointer, XEvent *),
            item_selection(Widget, XtPointer, XtPointer),
            print_button(Widget, XtPointer, XtPointer),
            print_data(void),
            save_input(Widget, XtPointer, XtPointer),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer);
int         get_sum_data(int, time_t *, double *);

#endif /* __show_dlog_h */
