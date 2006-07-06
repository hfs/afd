/*
 *  show_dlog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#define SHOW_AGE_OUTPUT            1
#define SHOW_AGE_INPUT             2
#ifdef WITH_DUP_CHECK
# define SHOW_DUP_INPUT            4
# define SHOW_DUP_OUTPUT           8
#endif
#define SHOW_USER_DEL              16
#define SHOW_EXEC_FAILED_DEL       32
#define SHOW_OTHER_DEL             64

#define AGE_OUTPUT_ID_STR          "AGE(O)"
#define AGE_OUTPUT_ID_LENGTH       (sizeof(AGE_OUTPUT_ID_STR) - 1)
#define AGE_INPUT_ID_STR           "AGE(I)"
#define AGE_INPUT_ID_LENGTH        (sizeof(AGE_INPUT_ID_STR) - 1)
#ifdef WITH_DUP_CHECK
# define DUP_INPUT_ID_STR          "DUP(I)"
# define DUP_INPUT_ID_LENGTH       (sizeof(DUP_INPUT_ID_STR) - 1)
# define DUP_OUTPUT_ID_STR         "DUP(O)"
# define DUP_OUTPUT_ID_LENGTH      (sizeof(DUP_OUTPUT_ID_STR) - 1)
#endif
#define USER_DEL_ID_STR            "USER"
#define USER_DEL_ID_LENGTH         (sizeof(USER_DEL_ID_STR) - 1)
#define EXEC_FAILED_DEL_ID_STR     "EXEC"
#define EXEC_FAILED_DEL_ID_LENGTH  (sizeof(EXEC_FAILED_DEL_ID_STR) - 1)
#define OTHER_OUTPUT_DEL_ID_STR    "OTHER(O)"
#define OTHER_OUTPUT_DEL_ID_LENGTH (sizeof(OTHER_OUTPUT_DEL_ID_STR) - 1)
#define OTHER_INPUT_DEL_ID_STR     "OTHER(I)"
#define OTHER_INPUT_DEL_ID_LENGTH  (sizeof(OTHER_INPUT_DEL_ID_STR) - 1)
#define UNKNOWN_ID_STR             "UNKNOWN"
#define UNKNOWN_ID_LENGTH          (sizeof(UNKNOWN_ID_STR) - 1)
#define DEL_UNKNOWN_FILE_ID_STR    "DUF"
#define DEL_UNKNOWN_FILE_ID_LENGTH (sizeof(DEL_UNKNOWN_FILE_ID_STR) - 1)
#define MAX_REASON_LENGTH          (sizeof(OTHER_OUTPUT_DEL_ID_STR) - 1)

#define OTHER_DEL_ID_STR           "OTHER"

/* Position in toggle_label widget table. */
#define AGE_OUTPUT_POS            0
#define AGE_INPUT_POS             1
#ifdef WITH_DUP_CHECK
# define DUP_INPUT_POS            2
# define DUP_OUTPUT_POS           3
# define USER_DEL_POS             4
# define EXEC_FAILED_DEL_POS      5
# define OTHER_DEL_POS            6
#else
# define USER_DEL_POS             2
# define EXEC_FAILED_DEL_POS      3
# define OTHER_DEL_POS            4
#endif

#define GOT_JOB_ID                -2
#define GOT_JOB_ID_DIR_ONLY       -3

#define F_KILOBYTE                1024.0
#define F_MEGABYTE                1048576.0
#define F_GIGABYTE                1073741824.0
#define F_TERABYTE                1099511627776.0

#define SEARCH_BUTTON             1
#define STOP_BUTTON               2
#define STOP_BUTTON_PRESSED       4

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define START_TIME_NO_ENTER       1
#define START_TIME                2
#define END_TIME_NO_ENTER         3
#define END_TIME                  4
#define FILE_NAME_NO_ENTER        5
#define FILE_NAME                 6
#define DIRECTORY_NAME_NO_ENTER   7
#define DIRECTORY_NAME            8
#define FILE_LENGTH_NO_ENTER      9
#define FILE_LENGTH               10
#define RECIPIENT_NAME_NO_ENTER   11
#define RECIPIENT_NAME            12

#define NO_OF_VISIBLE_LINES       20
#define MAXARGS                   20

#define LINES_BUFFERED            1000
#define MAX_DISPLAYED_FILE_SIZE   10
#define MAX_PROC_USER_LENGTH      18
#define MAX_OUTPUT_LINE_LENGTH    16 + MAX_HOSTNAME_LENGTH + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + MAX_REASON_LENGTH + 1 + MAX_PROC_USER_LENGTH + 1

#define FILE_SIZE_FORMAT          "Enter file size in bytes: [=<>]file size"
#define TIME_FORMAT               "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed */
#define SHOW_SHORT_FORMAT         26
#define SHOW_MEDIUM_FORMAT        40
#define SHOW_LONG_FORMAT          70
#define DATE_TIME_HEADER         "Date   Time     "
#define FILE_NAME_HEADER         "File name"
#define FILE_SIZE_HEADER         "File size   "
#define HOST_NAME_HEADER         "Hostname"
#if MAX_HOSTNAME_LENGTH < 8
# define HOST_NAME_LENGTH        8
#else
# define HOST_NAME_LENGTH        MAX_HOSTNAME_LENGTH
#endif
#define REST_HEADER              "Reason   Process/User  "

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
          char         *files;
          char         loptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
          char         recipient[MAX_RECIPIENT_LENGTH];
          char         priority;
       };

/* Structure to hold all data for a single dir ID. */
struct info_data
       {
          unsigned int       job_no;
          unsigned int       dir_id;
          int                count;   /* Counts number of dbe entries. */
          char               input_id;
          char               dir[MAX_PATH_LENGTH];
          char               dir_id_str[MAX_DIR_ALIAS_LENGTH + 1];
          char               file_name[MAX_FILENAME_LENGTH];
          char               file_size[MAX_INT_LENGTH + MAX_INT_LENGTH];
          char               proc_user[MAX_PROC_USER_LENGTH + 1];
          char               extra_reason[MAX_PATH_LENGTH];
          struct dir_options d_o;
          struct db_entry    *dbe;
       };

/* Permission structure for show_dlog */
struct sol_perm
       {
          int  list_limit;
          char view_passwd;
       };

/* Various macro definitions. */
#define SHOW_MESSAGE()                                                        \
        {                                                                     \
           XmString     xstr;                                                 \
           XExposeEvent xeev;                                                 \
           int          w, h;                                                 \
                                                                              \
           xeev.type = Expose;                                                \
           xeev.display = display;                                            \
           xeev.window = XtWindow(statusbox_w);                               \
           xeev.x = 0; xeev.y = 0;                                            \
           XtVaGetValues (statusbox_w, XmNwidth, &w, XmNheight, &h, NULL);    \
           xeev.width = w; xeev.height = h;                                   \
           xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG); \
           XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);            \
           (XtClass(statusbox_w))->core_class.expose (statusbox_w, (XEvent *)&xeev, NULL);\
           XmStringFree(xstr);                                                \
        }
#define SHOW_SUMMARY_DATA()                                                   \
        {                                                                     \
           XmString     xstr;                                                 \
           XExposeEvent xeev;                                                 \
           int          w, h;                                                 \
                                                                              \
           xeev.type = Expose;                                                \
           xeev.display = display;                                            \
           xeev.window = XtWindow(summarybox_w);                              \
           xeev.x = 0; xeev.y = 0;                                            \
           XtVaGetValues(summarybox_w, XmNwidth, &w, XmNheight, &h, NULL);    \
           xeev.width = w; xeev.height = h;                                   \
           xstr = XmStringCreateLtoR(summary_str, XmFONTLIST_DEFAULT_TAG);    \
           XtVaSetValues(summarybox_w, XmNlabelString, xstr, NULL);           \
           (XtClass(summarybox_w))->core_class.expose (summarybox_w, (XEvent *)&xeev, NULL);\
           XmStringFree(xstr);                                                \
        }
#define CHECK_INTERRUPT()                                                  \
        {                                                                  \
           XEvent event;                                                   \
                                                                           \
           XFlush(display);                                                \
           XmUpdateDisplay(appshell);                                      \
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
            save_input(Widget, XtPointer, XtPointer),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer);
int         get_sum_data(int, time_t *, double *);

#endif /* __show_dlog_h */
