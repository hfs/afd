/*
 *  dir_ctrl.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __dir_ctrl_h
#define __dir_ctrl_h

#include <time.h>                       /* clock_t                       */
#include "x_common_defs.h"

/* Redraw times for dir_ctrl. */
#define STARTING_DIR_REDRAW_TIME       150
#define MIN_DIR_REDRAW_TIME            250
#define MAX_DIR_REDRAW_TIME            1000
#define DIR_REDRAW_STEP_TIME           150

/* Definitions for the menu bar items. */
#define DIR_W                          0

/* Definitions for Monitor pulldown. */
#define DIR_DISABLE_W                  0
#define DIR_VIEW_LOAD_W                1
#define DIR_EXIT_W                     2
#define NO_DIR_MENU                    3

/* Definitions for View pulldown. */
#define DIR_SYSTEM_W                   0
#define DIR_RECEIVE_W                  1
#define DIR_TRANS_W                    2
#define DIR_INPUT_W                    3
#define DIR_OUTPUT_W                   4
#define DIR_DELETE_W                   5
#define DIR_INFO_W                     6
#define NO_DIR_VIEW_MENU               7

/* Definitions of popup selections. */
#define DIR_INFO_SEL                   60
#define DIR_DISABLE_SEL                61

/* Character types. */
#define NO_OF_DIR_PROCESS              0
#define FILE_RATE                      1
#define BYTE_RATE                      2

/* Bar types. */
#define FILE_RATE_BAR_NO               0
#define BYTE_RATE_BAR_NO               1

struct dir_line 
       {
          char           dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char           dir_display_str[MAX_DIR_ALIAS_LENGTH + 1];
          char           str_np[3];
          char           str_fr[5];
          char           str_tr[5];
          int            max_process;
          int            no_of_process;
          unsigned int   bytes_per_sec;
          unsigned int   prev_bytes_per_sec;
          float          files_per_sec;
          float          prev_files_per_sec;
          unsigned long  bytes_received;
          unsigned int   files_received;
          time_t         last_retrieval;
          unsigned char  dir_status;
          double         average_tr;         /* Average byte rate.   */
          double         max_average_tr;     /* Max byte rate        */
          double         average_fr;         /* Average file rate.   */
          double         max_average_fr;     /* Max file rate        */
          unsigned int   bar_length[2];
          clock_t        start_time;
          unsigned char  inverse;
          unsigned char  expose_flag;
       };
      
/* Structure that holds the permissions */
struct dir_control_perm
       {
          char        **dir_ctrl_list;
          char        **info_list;
          char        **disable_list;
          char        **show_slog_list;
          char        **show_rlog_list;
          char        **show_tlog_list;
          char        **show_ilog_list;
          char        **show_olog_list;
          char        **show_elog_list;
          char        **afd_load_list;
          signed char info;                  /* Info about AFD           */
          signed char disable;               /* Enable/Disable AFD       */
          signed char show_slog;             /* Show System Log          */
          signed char show_rlog;             /* Show Receive Log         */
          signed char show_tlog;             /* Show Transfer Log        */
          signed char show_ilog;             /* Show Input Log           */
          signed char show_olog;             /* Show Output Log          */
          signed char show_elog;             /* Show Delete Log          */
          signed char afd_load;              /* Show load of AFD         */
       };

/* Function Prototypes */
signed char dir_window_size(int *, int *),
            resize_dir_window(void);
void        check_dir_status(Widget),
            draw_dir_identifier(int, int, int),
            draw_dir_bar(int, signed char, char, int, int),
            draw_dir_blank_line(int),
            draw_dir_chars(int, char, int, int),
            draw_dir_label_line(void),
            draw_dir_line_status(int, signed char),
            draw_dir_proc_led(int, signed char, int, int),
            draw_dir_type(int, int, int),
            dir_expose_handler_label(Widget, XtPointer,
                                     XmDrawingAreaCallbackStruct *),
            dir_expose_handler_line(Widget, XtPointer,
                                    XmDrawingAreaCallbackStruct *),
            dir_focus(Widget, XtPointer, XEvent *),
            dir_input(Widget, XtPointer, XEvent *),
            popup_dir_menu_cb(Widget, XtPointer, XEvent *),
            save_dir_setup_cb(Widget, XtPointer, XtPointer),
            dir_popup_cb(Widget, XtPointer, XtPointer),
            change_dir_font_cb(Widget, XtPointer, XtPointer),
            change_dir_rows_cb(Widget, XtPointer, XtPointer),
            change_dir_style_cb(Widget, XtPointer, XtPointer),
            setup_dir_window(char *);

#endif /* __dir_ctrl_h */
