/*
 *  show_olog.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
#define SHOW_FTP                 1
#define SHOW_FILE                2
#define SHOW_HTTP                4
#define SHOW_SMTP                8
#ifdef _WITH_SCP_SUPPORT
# define SHOW_SCP                16
#endif
#ifdef _WITH_WMO_SUPPORT
# define SHOW_WMO                32
#endif
#ifdef _WITH_MAP_SUPPORT
# define SHOW_MAP                64
#endif
#ifdef WITH_SSL
# define SHOW_FTPS               128
# define SHOW_HTTPS              256
# define SHOW_SMTPS              512
#endif
#define SHOW_SFTP                1024

#define LOCAL_FILENAME           8
#define REMOTE_FILENAME          9

#define GOT_JOB_ID               -2
#define GOT_JOB_ID_DIR_ONLY      -3
#define GOT_JOB_ID_USER_ONLY     -4
#define GOT_JOB_ID_PRIORITY_ONLY -5

#define FTP_ID_STR               "FTP  "
#define FILE_ID_STR              "FILE "
#define SMTP_ID_STR              "SMTP "
#define HTTP_ID_STR              "HTTP "
#ifdef _WITH_SCP_SUPPORT
# define SCP_ID_STR              "SCP  "
#endif
#ifdef _WITH_WMO_SUPPORT
# define WMO_ID_STR              "WMO  "
#endif
#ifdef _WITH_MAP_SUPPORT
# define MAP_ID_STR              "MAP  "
#endif
#ifdef WITH_SSL
# define FTPS_ID_STR             "FTPS "
# define HTTPS_ID_STR            "HTTPS"
# define SMTPS_ID_STR            "SMTPS"
#endif
#define SFTP_ID_STR              "SFTP "
#define UNKNOWN_ID_STR           "?    "

#define SEARCH_BUTTON            1
#define STOP_BUTTON              2
#define STOP_BUTTON_PRESSED      4

/* Status definitions for resending files. */
#define FILE_PENDING             10
#define NOT_ARCHIVED             11
#define NOT_FOUND                12
#define NOT_IN_ARCHIVE           13
#define SEND_LIMIT_REACHED       14
/* NOTE: DONE is defined in afddefs.h as 3 */

/* When saving input lets define some names so we know where */
/* to store the user input.                                  */
#define START_TIME_NO_ENTER      1
#define START_TIME               2
#define END_TIME_NO_ENTER        3
#define END_TIME                 4
#define FILE_NAME_NO_ENTER       5
#define FILE_NAME                6
#define DIRECTORY_NAME_NO_ENTER  7
#define DIRECTORY_NAME           8
#define FILE_LENGTH_NO_ENTER     9
#define FILE_LENGTH              10
#define RECIPIENT_NAME_NO_ENTER  11
#define RECIPIENT_NAME           12

#define NO_OF_VISIBLE_LINES      20
#define MAXARGS                  20

#define LINES_BUFFERED           1000
#define MAX_DISPLAYED_FILE_SIZE  10
#define MAX_DISPLAYED_TRANSFER_TIME 6
#define MAX_OUTPUT_LINE_LENGTH   16 + MAX_HOSTNAME_LENGTH + 1 + 1 + 5 + 1 + MAX_DISPLAYED_FILE_SIZE + 1 + MAX_DISPLAYED_TRANSFER_TIME + 1 + 2

#define FILE_SIZE_FORMAT         "Enter file size in bytes: [=<>]file size"
#define TIME_FORMAT              "Absolut: MMDDhhmm or DDhhmm or hhmm   Relative: -DDhhmm or -hhmm or -mm"

/* Maximum length of the file name that is displayed */
#define SHOW_SHORT_FORMAT        26
#define SHOW_MEDIUM_FORMAT       40
#define SHOW_LONG_FORMAT         70
#define DATE_TIME_HEADER         "Date   Time     "
#define FILE_NAME_HEADER         "File name"
#define HOST_NAME_HEADER         "Hostname"
#if MAX_HOSTNAME_LENGTH < 8
# define HOST_NAME_LENGTH        8
#else
# define HOST_NAME_LENGTH        MAX_HOSTNAME_LENGTH
#endif
#define REST_HEADER              "Type    File size   TT   A"

#define LOG_CHECK_INTERVAL       1000L   /* Default interval in milli-    */
                                        /* seconds to check for changes  */
                                        /* in log file.                  */

/* Structure that holds offset (to job ID) to each item in list. */
struct item_list
       {
          FILE  *fp;
          int   no_of_items;
          off_t *line_offset; /* Array that contains the offset to the */
                              /* file name of that item.               */
          int   *offset;      /* Array that contains the offset to the */
                              /* job ID of that item.                  */
          char  *archived;    /* Was this file archived?               */
       };

/* Structure to hold all data for a single job ID. */
struct info_data
       {
          unsigned int       job_no;
          unsigned int       dir_id;
          int                no_of_files;
          time_t             date_send;
          char               *files;
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
          char               dir_id_str[MAX_DIR_ALIAS_LENGTH + 1];
          char               recipient[MAX_RECIPIENT_LENGTH];
          char               user[MAX_RECIPIENT_LENGTH];
          char               mail_destination[MAX_RECIPIENT_LENGTH];
          char               local_file_name[MAX_FILENAME_LENGTH];
          char               remote_file_name[MAX_FILENAME_LENGTH];
          char               dir[MAX_PATH_LENGTH];
          char               archive_dir[MAX_PATH_LENGTH];
          char               file_size[MAX_INT_LENGTH + MAX_INT_LENGTH];
          char               trans_time[MAX_INT_LENGTH + MAX_INT_LENGTH];
          char               unique_name[9 + 1 + 9 + 1 + 9 + 2];
          char               priority;
          struct dir_options d_o;
       };

/* To optimize the resending of files the following structure is needed. */
struct resend_list
       {
          unsigned int job_id;
          int          file_no;  /* File number of log file.     */
          int          pos;
          char         status;   /* DONE - file has been resend. */
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
extern void calculate_summary(char *, time_t, time_t, unsigned int,
                              double, double),
            close_button(Widget, XtPointer, XtPointer),
            continues_toggle(Widget, XtPointer, XtPointer),
            file_name_toggle(Widget, XtPointer, XtPointer),
            format_info(char *),
            get_info(int),
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
            set_sensitive(void),
            scrollbar_moved(Widget, XtPointer, XtPointer),
            search_button(Widget, XtPointer, XtPointer),
            toggled(Widget, XtPointer, XtPointer);
extern int  get_sum_data(int, time_t *, double *, double *);

#endif /* __show_olog_h */
