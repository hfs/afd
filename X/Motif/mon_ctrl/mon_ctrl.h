/*
 *  mon_ctrl.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __mon_ctrl_h
#define __mon_ctrl_h

#include "x_common_defs.h"
#include "mondefs.h"

/* Definitions for the menu bar items. */
#define MON_W                          0

/* Definitions for Monitor pulldown. */
#define MON_SYS_LOG_W                  0
#define MON_LOG_W                      1
#define MON_RETRY_W                    2
#define MON_DISABLE_W                  3
#define MON_TEST_W                     4
#define MON_INFO_W                     5
#define MON_EXIT_W                     6

/* Definitions for View pulldown. */
#define MON_AFD_CTRL_W                 0
#define MON_SYSTEM_W                   1
#define MON_RECEIVE_W                  2
#define MON_TRANS_W                    3
#define MON_INPUT_W                    4
#define MON_OUTPUT_W                   5
#define MON_DELETE_W                   6
#define MON_VIEW_LOAD_W                7

/* Definitions of popup selections. */
#define MON_SYS_LOG_SEL                50
#define MON_LOG_SEL                    51
#define MON_INFO_SEL                   52
#define MON_RETRY_SEL                  53
#define MON_DISABLE_SEL                54

/* Definitions for the Setup pulldown. */
#define HISTORY_W                      4

/* History length definitions. */
#define HIS_0                          "0"
#define HIS_1                          "6"
#define HIS_2                          "12"
#define HIS_3                          "18"
#define HIS_4                          "24"
#define NO_OF_HISTORY_LOGS             5

/* History types. */
#define RECEIVE_HISTORY                0
#define SYSTEM_HISTORY                 1
#define TRANSFER_HISTORY               2

/* Character types. */
#define FILES_TO_BE_SEND               0
#define FILE_SIZE_TO_BE_SEND           1
#define AVERAGE_TRANSFER_RATE          2
#define AVERAGE_CONNECTION_RATE        3
#define TOTAL_ERROR_COUNTER            4
#define JOBS_IN_QUEUE                  5
#define ACTIVE_TRANSFERS               6
#define ERROR_HOSTS                    7

/* Bar types. */
#define MON_TR_BAR_NO                  0
#define ACTIVE_TRANSFERS_BAR_NO        1
#define HOST_ERROR_BAR_NO              2

struct mon_line 
       {
          char           afd_alias[MAX_AFDNAME_LENGTH + 1];
          char           afd_display_str[MAX_AFDNAME_LENGTH + 1];
          char           str_fc[5];
          char           str_fs[5];
          char           str_tr[5];
          char           str_fr[3];
          char           str_ec[3];
          char           str_jq[5];
          char           str_at[4];
          char           str_hec[3];
          char           sys_log_fifo[LOG_FIFO_SIZE + 1];
          char           log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];
          char           amg;
          char           fd;
          signed char    blink_flag;
          signed char    blink;
          char           archive_watch;
          int            jobs_in_queue;
          int            no_of_transfers;
          int            host_error_counter;
          int            no_of_hosts;
          int            max_connections;
          unsigned int   sys_log_ec;
          unsigned int   fc;
          unsigned int   fs;
          unsigned int   tr;
          unsigned int   fr;
          unsigned int   ec;
          time_t         last_data_time;
          char           connect_status;
          float          scale[2];
          double         average_tr;         /* Average transfer rate.   */
          double         max_average_tr;     /* Max transfer rate        */
          unsigned int   bar_length[3];
          unsigned short green_color_offset;
          unsigned short blue_color_offset;
          unsigned char  inverse;
          unsigned char  expose_flag;
       };
      
/* Structure that holds the permissions */
struct mon_control_perm
       {
          char        **mon_ctrl_list;
          char        **info_list;
          char        **retry_list;
          char        **disable_list;
          char        **afd_ctrl_list;
          char        **show_slog_list;
          char        **show_rlog_list;
          char        **show_tlog_list;
          char        **show_mm_log_list;
          char        **show_ilog_list;
          char        **show_olog_list;
          char        **show_elog_list;
          char        **afd_load_list;
          char        **edit_hc_list;
          signed char amg_ctrl;              /* Start/Stop the AMG       */
          signed char fd_ctrl;               /* Start/Stop the FD        */
          signed char rr_dc;                 /* Reread DIR_CONFIG        */
          signed char rr_hc;                 /* Reread HOST_CONFIG       */
          signed char startup_afd;           /* Startup AFD              */
          signed char shutdown_afd;          /* Shutdown AFD             */
          signed char info;                  /* Info about AFD           */
          signed char retry;                 /* Retry connecting to AFD  */
          signed char disable;               /* Enable/Disable AFD       */
          signed char show_ms_log;           /* Show Monitor System Log  */
          signed char show_mm_log;           /* Show Monitor Log         */
          signed char afd_ctrl;              /* Show AFD Control Dialog  */
          signed char show_slog;             /* Show System Log          */
          signed char show_rlog;             /* Show Receive Log         */
          signed char show_tlog;             /* Show Transfer Log        */
          signed char show_ilog;             /* Show Input Log           */
          signed char show_olog;             /* Show Output Log          */
          signed char show_elog;             /* Show Delete Log          */
          signed char afd_load;              /* Show load of AFD         */
          signed char edit_hc;               /* Edit HOST_CONFIG         */
          signed char dir_ctrl;              /* dir_ctrl dialog          */
       };

/* Function Prototypes */
signed char mon_window_size(int *, int *),
            resize_mon_window(void);
void        check_afd_status(Widget),
            draw_afd_identifier(int, int, int),
            draw_mon_bar(int, signed char, char, int, int),
            draw_mon_blank_line(int),
            draw_mon_chars(int, char, int, int),
            draw_mon_label_line(void),
            draw_mon_line_status(int, signed char),
            draw_mon_proc_led(int, signed char, int, int),
            draw_remote_log_status(int, int, int, int),
            draw_remote_history(int, int, int, int),
            mon_expose_handler_label(Widget, XtPointer,
                                     XmDrawingAreaCallbackStruct *),
            mon_expose_handler_line(Widget, XtPointer,
                                    XmDrawingAreaCallbackStruct *),
            mon_focus(Widget, XtPointer, XEvent *),
            mon_input(Widget, XtPointer, XEvent *),
            popup_mon_menu_cb(Widget, XtPointer, XEvent *),
            save_mon_setup_cb(Widget, XtPointer, XtPointer),
            mon_popup_cb(Widget, XtPointer, XtPointer),
            change_mon_font_cb(Widget, XtPointer, XtPointer),
            change_mon_rows_cb(Widget, XtPointer, XtPointer),
            change_mon_style_cb(Widget, XtPointer, XtPointer),
            change_mon_history_cb(Widget, XtPointer, XtPointer),
            setup_mon_window(char *),
            start_remote_prog(Widget, XtPointer, XtPointer);

#endif /* __mon_ctrl_h */
