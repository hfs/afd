/*
 *  show_olog.h - Part of AFD, an automatic file distribution program.
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

#ifndef __show_olog_h
#define __show_olog_h

#include "x_common_defs.h"

/* What information should be displayed */
#define SHOW_FTP                1
#define SHOW_SMTP               2
#define SHOW_FILE               4
#ifdef _WITH_SCP1_SUPPORT
#define SHOW_SCP1               8
#endif
#ifdef _WITH_WMO_SUPPORT
#define SHOW_WMO                16
#endif
#ifdef _WITH_MAP_SUPPORT
#define SHOW_MAP                32
#endif

#define LOCAL_FILENAME          8
#define REMOTE_FILENAME         9

#define GOT_JOB_ID              -2
#define GOT_JOB_ID_DIR_ONLY     -3
#define GOT_JOB_ID_USER_ONLY    -4

#define F_KILOBYTE              1024.0
#define F_MEGABYTE              1048576.0
#define F_GIGABYTE              1073741824.0
#define F_TERABYTE              1099511627776.0

#define FTP_ID_STR              "FTP "
#define SMTP_ID_STR             "SMTP"
#define FILE_ID_STR             "FILE"
#ifdef _WITH_SCP1_SUPPORT
#define SCP1_ID_STR             "SCP1"
#endif
#ifdef _WITH_WMO_SUPPORT
#define WMO_ID_STR              "WMO "
#endif
#ifdef _WITH_MAP_SUPPORT
#define MAP_ID_STR              "MAP "
#endif
#define UNKNOWN_ID_STR          "?   "

#define SEARCH_BUTTON           1
#define STOP_BUTTON             2
#define STOP_BUTTON_PRESSED     4

/* Status definitions for resending files. */
#define FILE_PENDING            10
#define NOT_ARCHIVED            11
#define NOT_FOUND               12
#define NOT_IN_ARCHIVE          13
#define FAILED_TO_SEND          14
/* NOTE: DONE is defined in afddefs.h as 3 */

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
#define MAX_DISPLAYED_TRANSFER_TIME 6
#define MAX_OUTPUT_LINE_LENGTH  16 + MAX_HOSTNAME_LENGTH + 1 + 1 + 4 + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + MAX_DISPLAYED_TRANSFER_TIME + 1 + 2

#define FILE_SIZE_FORMAT        "Enter file size in bytes: [=<>]file size"
#define TIME_FORMAT             "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed */
#define SHOW_SHORT_FORMAT       26
#define SHOW_MEDIUM_FORMAT      40
#define SHOW_LONG_FORMAT        60
#define HEADING_LINE_SHORT      "Date   Time     File name                  Hostname Type   File size   TT   A"
#define SUM_SEP_LINE_SHORT      "============================================================================="
#define HEADING_LINE_MEDIUM     "Date   Time     File name                                Hostname Type   File size   TT   A"
#define SUM_SEP_LINE_MEDIUM     "==========================================================================================="
#define HEADING_LINE_LONG       "Date   Time     File name                                                    Hostname Type   File size   TT   A"
#define SUM_SEP_LINE_LONG       "==============================================================================================================="

/* Definitions for sending files not in FSA */
#define SET_ASCII               'A'
#define SET_BIN                 'I'
#define SET_DOS                 'D'
#define SET_LOCK_DOT            4
#define SET_LOCK_OFF            5
#define SET_LOCK_DOT_VMS        6
#define SET_LOCK_LOCKFILE       7
#define SET_LOCK_PREFIX         8
#define HOSTNAME_NO_ENTER       20
#define HOSTNAME_ENTER          21
#define USER_NO_ENTER           22
#define USER_ENTER              23
#define PASSWORD_NO_ENTER       24
#define PASSWORD_ENTER          25
#define TARGET_DIR_NO_ENTER     26
#define TARGET_DIR_ENTER        27
#define PORT_NO_ENTER           28
#define PORT_ENTER              29
#define TIMEOUT_NO_ENTER        30
#define TIMEOUT_ENTER           31
#define PREFIX_NO_ENTER         32
#define PREFIX_ENTER            33

#define MAX_TIMEOUT_DIGITS      4       /* Maximum number of digits for  */
                                        /* the timeout field.            */
#define MAX_PORT_DIGITS         5       /* Maximum number of digits for  */
                                        /* the port field.               */

/* Structure that holds offset (to job ID) to each item in list. */
struct item_list
       {
          FILE *fp;
          int  no_of_items;
          int  *line_offset;   /* Array that contains the offset to the */
                               /* file name of that item.               */
          int  *offset;        /* Array that contains the offset to the */
                               /* job ID of that item.                  */
          char *archived;      /* Was this file archived?               */
       };

/* Structure to hold all data for a single job ID. */
struct info_data
       {
          unsigned int       job_no;
          int                no_of_files;
          time_t             date_send;
          char               **files;
#ifdef _WITH_DYNAMIC_MEMORY
          int                no_of_loptions;
          char               **loptions;
          int                no_of_soptions;
          char               *soptions;
#else
          int                no_of_loptions;
          int                no_of_soptions;
          char               *soptions;
          char               loptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
#endif
          char               recipient[MAX_RECIPIENT_LENGTH];
          char               user[MAX_RECIPIENT_LENGTH];
          char               mail_destination[MAX_RECIPIENT_LENGTH];
          char               local_file_name[MAX_FILENAME_LENGTH];
          char               remote_file_name[MAX_FILENAME_LENGTH];
          char               dir[MAX_PATH_LENGTH];
          char               archive_dir[MAX_PATH_LENGTH];
          char               file_size[MAX_INT_LENGTH + MAX_INT_LENGTH];
          char               trans_time[MAX_INT_LENGTH + MAX_INT_LENGTH];
          char               priority;
          struct dir_options d_o;
       };

/* To optimize the resending of files the following structure is needed. */
struct resend_list
       {
          char         status;   /* DONE - file has been resend.   */
          unsigned int job_id;
          int          file_no;
          int          pos;
       };

/* Structure holding all data of for sending files. */
struct send_data
       {
          char   hostname[MAX_FILENAME_LENGTH];
          char   smtp_server[MAX_FILENAME_LENGTH];
          char   user[MAX_USER_NAME_LENGTH + 1];
          char   target_dir[MAX_PATH_LENGTH];
          char   prefix[MAX_FILENAME_LENGTH];
          char   subject[MAX_PATH_LENGTH];
          char   lock;                    /* DOT, OFF, etc.              */
          char   mode_flag;               /* FTP passive or active mode. */
          char   transfer_mode;
          int    protocol;
          int    port;
          int    debug;
          time_t timeout;
          char   *password;
       };

/* Permission structure for show_olog */
struct sol_perm
       {
          int  resend_limit;
          int  send_limit;
          int  list_limit;
          char view_passwd;
       };

/* Various macro definitions. */
#define GET_TIME()                            \
        {                                     \
           register int i = 0;                \
                                              \
           while ((*ptr != ' ') && (i < MAX_INT_LENGTH)) \
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
extern void calculate_summary(char *, time_t, time_t, unsigned int,
                              double, double),
            close_button(Widget, XtPointer, XtPointer),
            file_name_toggle(Widget, XtPointer, XtPointer),
            format_info(char *),
            get_info(int),
            get_send_data(int, int *),
            get_data(void),
            info_click(Widget, XtPointer, XEvent *),
            item_selection(Widget, XtPointer, XtPointer),
            print_button(Widget, XtPointer, XtPointer),
            radio_button(Widget, XtPointer, XtPointer),
            resend_button(Widget, XtPointer, XtPointer),
            resend_files(int, int *),
            save_input(Widget, XtPointer, XtPointer),
            send_button(Widget, XtPointer, XtPointer),
            send_files(int, int *),
            send_files_ftp(int, int *, int *, int *, int *,
                           struct resend_list *, int *, int *),
            send_files_smtp(int, int *, int *, int *, int *,
                            struct resend_list *, int *, int *),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            toggled(Widget, XtPointer, XtPointer),
            trans_log(char *, char *, int , char *, ...);
extern int  get_sum_data(int, time_t *, double *, double *);

#endif /* __show_olog_h */
