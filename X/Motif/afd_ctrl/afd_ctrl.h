/*
 *  afd_ctrl.h - Part of AFD, an automatic file distribution program.
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

#ifndef __afd_ctrl_h
#define __afd_ctrl_h

#include <time.h>                       /* clock_t                       */
#include "x_common_defs.h"

#define MAX_STRING_LENGTH               20
#define DEFAULT_NO_OF_ROWS              40
#define TV_STARTING_REDRAW_TIME         100
#define MIN_TV_REDRAW_TIME              200
#define MAX_TV_REDRAW_TIME              800
#define TV_REDRAW_STEP_TIME             100
#define BAR_LENGTH_MODIFIER             7

#define PRINTER_INFO_LENGTH             40

#define MAX_NO_OF_DETAILED_TRANSFERS    60

/* Definitions for the menu bar items */
#define HOST_W                          0
#define LOG_W                           1
#define CONTROL_W                       2
#define CONFIG_W                        3
#define HELP_W                          4

/* Definitions for Host pulldown */
#define QUEUE_W                         0
#define TRANSFER_W                      1
#define DISABLE_W                       2
#define SWITCH_W                        3
#define RETRY_W                         4
#define DEBUG_W                         5
#define TEST_W                          6
#define INFO_W                          7
#define VIEW_DC_W                       8
#define EXIT_W                          9

/* Definitions for View pulldown */
#define SYSTEM_W                        0
#define TRANS_W                         1
#define TRANS_DEBUG_W                   2
#define INPUT_W                         3
#define OUTPUT_W                        4
#define DELETE_W                        5
#define VIEW_LOAD_W                     6
#define VIEW_JOB_W                      7

/* Definitions for Control pulldown */
#define AMG_CTRL_W                      0
#define FD_CTRL_W                       1
#define RR_DC_W                         2
#define RR_HC_W                         3
#define EDIT_HC_W                       4
#define STARTUP_AFD_W                   5
#define SHUTDOWN_AFD_W                  6

/* Definitions of popup selections */
#define QUEUE_SEL                       0
#define TRANS_SEL                       1
#define RETRY_SEL                       2
#define DEBUG_SEL                       3
#define INFO_SEL                        4
#define S_LOG_SEL                       5
#define T_LOG_SEL                       6
#define D_LOG_SEL                       7
#define I_LOG_SEL                       8
#define O_LOG_SEL                       9
#define EXIT_SEL                        10
#define DISABLE_SEL                     11
#define DELETE_SEL                      12
#define VIEW_JOB_SEL                    13
#define R_LOG_SEL                       14
#define SWITCH_SEL                      15
#define VIEW_FILE_LOAD_SEL              16
#define VIEW_KBYTE_LOAD_SEL             17
#define VIEW_CONNECTION_LOAD_SEL        18
#define VIEW_TRANSFER_LOAD_SEL          19
#define VIEW_DC_SEL                     20
#define PING_SEL                        21
#define TRACEROUTE_SEL                  22
/* NOTE: x_common_defs.h defines from 50 onwards. */

/* Definitions for testing connections */
#define SHOW_PING_TEST                  "Ping"
#define SHOW_TRACEROUTE_TEST            "Traceroute"
#define PING_W                          0
#define TRACEROUTE_W                    1

/* Bar types */
#define ERROR_BAR_NO                    0
#define TR_BAR_NO                       1
#define CURRENT_FILE_SIZE_BAR_NO        0
#define NO_OF_FILES_DONE_BAR_NO         1
#define FILE_SIZE_DONE_BAR_NO           2

/* Character types for the tv window. */
#define FILE_SIZE_IN_USE                0
#define FILE_SIZE_IN_USE_DONE           1
#define NUMBER_OF_FILES                 2
#define NUMBER_OF_FILES_DONE            3
#define FILE_SIZE                       4
#define FILE_SIZE_DONE                  5

/* LED indicators */
#define AMG_LED                         0
#define FD_LED                          1
#define AW_LED                          2
#define AFDD_LED                        3

/* Status LED's */
#define LED_ONE                         1
#define LED_TWO                         2

/* Log indicators (bit mapped) */
#define SYS_LOG_INDICATOR               0
#define TRANS_LOG_INDICATOR             1

/* Structure definitions */
struct line 
       {
          char           hostname[MAX_HOSTNAME_LENGTH + 1];
          char           host_display_str[MAX_HOSTNAME_LENGTH + 1];
          int            no_of_files[MAX_NO_PARALLEL_JOBS];
          char           connect_status[MAX_NO_PARALLEL_JOBS];
          char           detailed_selection[MAX_NO_PARALLEL_JOBS];
          unsigned int   bytes_send[MAX_NO_PARALLEL_JOBS];
                                             /* No. of bytes send so far.*/
          char           debug;              /* Is debugging enabled     */
                                             /* or disabled?             */
          char           host_toggle;
          char           host_toggle_display;
          unsigned char  stat_color_no;
          unsigned char  special_flag;
          clock_t        start_time;
          char           status_led[2];
          int            total_file_counter; /* The overall number of    */
                                             /* files still to be send.  */
          char           str_tfc[5];         /* String holding this      */
                                             /* number.                  */
          unsigned int   total_file_size;    /* The overall number of    */
                                             /* bytes still to be send.  */
          char           str_tfs[5];         /* String holding this      */
                                             /* number.                  */
          int            bytes_per_sec;      /* Actual transfer rate.    */
          char           str_tr[5];          /* String holding this      */
                                             /* number.                  */
          double         average_tr;         /* Average transfer rate.   */
          double         max_average_tr;     /* Max transfer rate        */
                                             /* (dynamic).               */
          int            error_counter;      /* Number of errors so far. */
          char           str_ec[3];          /* String holding this      */
                                             /* number.                  */
          int            max_errors;
          int            allowed_transfers;
          float          scale;
          unsigned int   bar_length[2];
          unsigned short green_color_offset;
          unsigned short red_color_offset;
          unsigned char  inverse;
          unsigned char  expose_flag;
       };
      
/* Structure that holds the permissions */
struct afd_control_perm
       {
          char        **afd_ctrl_list;
          char        **ctrl_transfer_list;
          char        **ctrl_queue_list;
          char        **switch_host_list;
          char        **disable_list;
          char        **info_list;
          char        **debug_list;
          char        **retry_list;
          char        **show_slog_list;
          char        **show_tlog_list;
          char        **show_dlog_list;
          char        **show_ilog_list;
          char        **show_olog_list;
          char        **show_rlog_list;
          char        **afd_load_list;
          char        **view_jobs_list;
          char        **edit_hc_list;
          char        **view_dc_list;
          signed char amg_ctrl;              /* Start/Stop the AMG       */
          signed char fd_ctrl;               /* Start/Stop the FD        */
          signed char rr_dc;                 /* Reread DIR_CONFIG        */
          signed char rr_hc;                 /* Reread HOST_CONFIG       */
          signed char startup_afd;           /* Startup AFD              */
          signed char shutdown_afd;          /* Shutdown AFD             */
          signed char ctrl_transfer;         /* Start/Stop transfer      */
          signed char ctrl_queue;            /* Start/Stop queue         */
          signed char switch_host;           /* Switch host              */
          signed char disable;               /* Disable host             */
          signed char info;                  /* Info about host          */
          signed char debug;                 /* Enable/disable debugging */
          signed char retry;                 /* Retry sending file       */
          signed char show_slog;             /* Show System Log          */
          signed char show_tlog;             /* Show Transfer Log        */
          signed char show_dlog;             /* Show Debug Log           */
          signed char show_ilog;             /* Show Input Log           */
          signed char show_olog;             /* Show Output Log          */
          signed char show_rlog;             /* Show Delete Log          */
          signed char afd_load;              /* Show load of AFD         */
          signed char view_jobs;             /* View detailed transfer   */
          signed char edit_hc;               /* Edit HOST_CONFIG         */
          signed char view_dc;               /* View DIR_CONFIG entries  */
       };

struct job_data
       {
          char          hostname[MAX_HOSTNAME_LENGTH + 1];
          char          host_display_str[MAX_HOSTNAME_LENGTH + 1];
          char          priority[2];
          char          file_name_in_use[MAX_FILENAME_LENGTH + 1];
          char          str_fs_use[5];        /* String file_size_in_use.*/
          char          str_fs_use_done[5];
          char          str_fc[4];            /* String no_of_files.     */
          char          str_fc_done[4];       /* String fc done.         */
          char          str_fs[5];            /* String file_size.       */
          char          str_fs_done[5];       /* String fs done.         */
          char          connect_status;
          unsigned char expose_flag;
          unsigned char stat_color_no;
          unsigned char special_flag;
          off_t         file_size_in_use;
          off_t         file_size_in_use_done;
          int           no_of_files;          /* Number of all files.    */
          int           no_of_files_done;
          off_t         file_size;            /* Size of all files.      */
          off_t         file_size_done;
          float         scale[3];
          unsigned int  bar_length[3];
          int           job_no;
          int           fsa_no;
          int           rotate;
       };

/* Function Prototypes */
extern void        calc_but_coord(void),
                   change_font_cb(Widget, XtPointer, XtPointer),
                   change_rows_cb(Widget, XtPointer, XtPointer),
                   change_style_cb(Widget, XtPointer, XtPointer),
                   check_host_status(Widget),
                   check_status(Widget),
                   check_tv_status(Widget),
                   control_cb(Widget, XtPointer, XtPointer),
                   create_tv_window(void),
                   expose_handler_button(Widget, XtPointer, XmDrawingAreaCallbackStruct *),
                   expose_handler_label(Widget, XtPointer, XmDrawingAreaCallbackStruct *),
                   expose_handler_line(Widget, XtPointer, XmDrawingAreaCallbackStruct *),
                   expose_handler_tv_line(Widget, XtPointer, XmDrawingAreaCallbackStruct *),
                   draw_file_name(int, int, int),
                   draw_label_line(void),
                   draw_line_status(int, signed char),
                   draw_button_line(void),
                   draw_blank_line(int),
                   draw_proc_stat(int, int, int, int),
                   draw_detailed_line(int),
                   draw_detailed_selection(int, int),
                   draw_dest_identifier(int, int, int),
                   draw_debug_led(int, int, int),
                   draw_led(int, int, int, int),
                   draw_bar(int, signed char, char, int, int),
                   draw_chars(int, char, int, int),
                   draw_proc_led(int, signed char),
                   draw_log_status(int, int),
                   draw_queue_counter(int),
                   draw_rotating_dash(int, int, int),
                   draw_tv_bar(int, signed char, char, int, int),
                   draw_tv_blank_line(int),
                   draw_tv_chars(int, char, int, int),
                   draw_tv_dest_identifier(int, int, int),
                   draw_tv_job_number(int, int, int),
                   draw_tv_priority(int, int, int),
                   draw_tv_label_line(void),
                   focus(Widget, XtPointer, XEvent *),
                   init_gcs(void),
                   init_jd_structure(struct job_data *, int, int),
                   input(Widget, XtPointer, XEvent *),
                   popup_cb(Widget, XtPointer, XtPointer),
                   popup_menu_cb(Widget, XtPointer, XEvent *),
                   save_setup_cb(Widget, XtPointer, XtPointer),
                   setup_tv_window(void),
                   setup_window(char *),
                   tv_input(Widget, XtPointer, XEvent *),
                   tv_locate_xy(int, int *, int *);
extern signed char resize_tv_window(void),
                   resize_window(void),
                   tv_window_size(Dimension *, Dimension *),
                   window_size(int *, int *);

#endif /* __afd_ctrl_h */
