/*
 *  x_common_defs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __x_common_defs_h
#define __x_common_defs_h

#include <Xm/Xm.h>

#define MAXARGS                         20
#define MAX_INTENSITY                   65535
#define STARTING_REDRAW_TIME            250
#define MIN_REDRAW_TIME                 650
#define MAX_REDRAW_TIME                 4500
#define REDRAW_STEP_TIME                600
#define DEFAULT_FRAME_SPACE             5
#define SPACE_ABOVE_LINE                1
#define SPACE_BELOW_LINE                2
#define BUTTON_SPACING                  3
#define LED_SPACING                     1
#define PROC_LED_SPACING                4
#define KILOBYTE                        1024
#define MEGABYTE                        1048576
#define GIGABYTE                        1073741824
#define DEFAULT_FILENAME_DISPLAY_LENGTH 25
#define DEFAULT_NO_OF_HISTORY_LOGS      0

/* Definitions for the printer interface. */
#define SELECTION_TOGGLE                1
#define ALL_TOGGLE                      2
#define PRINTER_TOGGLE                  3
#define FILE_TOGGLE                     4
#define CONTROL_D                       '\004'

/* Definitions for all X programs of the AFD */
#define MAX_MESSAGE_LENGTH              80

#define PRINTER_INFO_LENGTH             40

/* Definitions for the different message dialogs. */
#define INFO_DIALOG                     1
#define CONFIG_DIALOG                   2
#define WARN_DIALOG                     3
#define ERROR_DIALOG                    4
#define FATAL_DIALOG                    5
#define ABORT_DIALOG                    6
#define QUESTION_DIALOG                 7

/* Definitions for the Setup pulldown */
#define FONT_W                          0
#define ROWS_W                          1
#define STYLE_W                         2
#define SAVE_W                          3

/* Definitions for the Help pulldown */
#define ABOUT_W                         0
#define HYPER_W                         1
#define VERSION_W                       2

/* Definitions for the Font pulldown */
#define FONT_0_W                        0
#define FONT_1_W                        1
#define FONT_2_W                        2
#define FONT_3_W                        3
#define FONT_4_W                        4
#define FONT_5_W                        5
#define FONT_6_W                        6
#define FONT_7_W                        7
#define FONT_8_W                        8
#define FONT_9_W                        9
#define FONT_10_W                       10
#define FONT_11_W                       11
#define FONT_12_W                       12
#define NO_OF_FONTS                     13

/* Definitions for the Row pulldown */
#define ROW_0_W                         0
#define ROW_1_W                         1
#define ROW_2_W                         2
#define ROW_3_W                         3
#define ROW_4_W                         4
#define ROW_5_W                         5
#define ROW_6_W                         6
#define ROW_7_W                         7
#define ROW_8_W                         8
#define ROW_9_W                         9
#define ROW_10_W                        10
#define ROW_11_W                        11
#define ROW_12_W                        12
#define ROW_13_W                        13
#define NO_OF_ROWS                      14

/* Definitions for the Line Style pulldown widgets */
#define STYLE_0_W                       0
#define STYLE_1_W                       1
#define STYLE_2_W                       2

/* Definitions of popup selections */
#define AFD_CTRL_SEL                    50
#define CONTROL_AMG_SEL                 51
#define CONTROL_FD_SEL                  52
#define REREAD_DIR_CONFIG_SEL           53
#define REREAD_HOST_CONFIG_SEL          54
#define EDIT_DC_SEL                     55
#define EDIT_HC_SEL                     56
#define STARTUP_AFD_SEL                 57
#define SHUTDOWN_AFD_SEL                58

#ifndef PI
#define PI                              3.141592654
#endif

#define STATIC                          2
#define YUP                             2

#define FONT_ID                         "Default font:"
#define ROW_ID                          "Number of rows:"
#define STYLE_ID                        "Line style:"
#define FILENAME_DISPLAY_LENGTH_ID      "Filename display length:"
#define NO_OF_HISTORY_LENGTH_ID         "History log length:"

#define NO_INFO_AVAILABLE               "No information available for this host."

/* line style */
#define BARS_ONLY                       0
#define CHARACTERS_ONLY                 1
#define CHARACTERS_AND_BARS             2

/* Definitions for program afd_load */
#define SHOW_FILE_LOAD                  "Files"
#define SHOW_KBYTE_LOAD                 "KBytes"
#define SHOW_CONNECTION_LOAD            "Connections"
#define SHOW_TRANSFER_LOAD              "Active-Transfers"
#define FILE_LOAD_W                     0
#define KBYTE_LOAD_W                    1
#define CONNECTION_LOAD_W               2
#define TRANSFER_LOAD_W                 3

/* List of fonts that should be available */
#define FONT_0                          "5x7"
#define FONT_1                          "5x8"
#define FONT_2                          "6x9"
#define FONT_3                          "6x10"
#define FONT_4                          "6x12"
#define FONT_5                          "6x13"
#define FONT_6                          "7x13"
#define FONT_7                          "7x14"
#define FONT_8                          "8x13"
#define FONT_9                          "8x16"
#define FONT_10                         "9x15"
#define FONT_11                         "10x20"
#define FONT_12                         "12x24"
#define DEFAULT_FONT                    "6x13"

/* List of number of rows in a column */
#define ROW_0                           "2"
#define ROW_1                           "4"
#define ROW_2                           "6"
#define ROW_3                           "8"
#define ROW_4                           "10"
#define ROW_5                           "14"
#define ROW_6                           "16"
#define ROW_7                           "20"
#define ROW_8                           "25"
#define ROW_9                           "30"
#define ROW_10                          "40"
#define ROW_11                          "50"
#define ROW_12                          "60"
#define ROW_13                          "70"

/* All colors for status, background, foreground, etc */
#define DEFAULT_BG_COLOR                "LightBlue1"
#define DEFAULT_BG_COLOR_1              "LightSkyBlue1"
#define DEFAULT_BG_COLOR_2              "LightBlue"
#define DEFAULT_BG_COLOR_3              "PowderBue"
#define WHITE_COLOR                     "White"
#define WHITE_COLOR_1                   "snow"
#define WHITE_COLOR_2                   "GhostWhite"
#define WHITE_COLOR_3                   "WhiteSmoke"
#define NOT_WORKING_COLOR               "tomato"
#define NOT_WORKING_COLOR_1             "tomato1"
#define NOT_WORKING_COLOR_2             "OrangeRed"
#define NOT_WORKING_COLOR_3             "coral"
#define NOT_WORKING2_COLOR              "Red"
#define NOT_WORKING2_COLOR_1            "Red1"
#define NOT_WORKING2_COLOR_2            "Red2"
#define NOT_WORKING2_COLOR_3            "firebrick1"
#define STOP_TRANSFER_COLOR             "DarkOrange"
#define STOP_TRANSFER_COLOR_1           "choclate1"
#define STOP_TRANSFER_COLOR_2           "orange"
#define STOP_TRANSFER_COLOR_3           "DarkOrange1"
#define TRANSFER_ACTIVE_COLOR           "SeaGreen"
#define TRANSFER_ACTIVE_COLOR_1         "ForestGreen"
#define TRANSFER_ACTIVE_COLOR_2         "OliveDrab"
#define TRANSFER_ACTIVE_COLOR_3         "PaleGreen4"
#define PAUSE_QUEUE_COLOR               "SaddleBrown"
#define PAUSE_QUEUE_COLOR_1             "choclate4"
#define PAUSE_QUEUE_COLOR_2             "sienna4"
#define PAUSE_QUEUE_COLOR_3             "DarkOrange4"
#define AUTO_PAUSE_QUEUE_COLOR          "brown3"
#define AUTO_PAUSE_QUEUE_COLOR_1        "OrangeRed3"
#define AUTO_PAUSE_QUEUE_COLOR_2        "firebrick"
#define AUTO_PAUSE_QUEUE_COLOR_3        "IndianRed3"
#define NORMAL_STATUS_COLOR             "green3"
#define NORMAL_STATUS_COLOR_1           "LimeGreen"
#define NORMAL_STATUS_COLOR_2           "chartreuse3"
#define NORMAL_STATUS_COLOR_3           "SpringGreen3"
#define CONNECTING_COLOR                "Blue"
#define CONNECTING_COLOR_1              "MediumBlue"
#define CONNECTING_COLOR_2              "blue1"
#define CONNECTING_COLOR_3              "blue2"
#define BLACK_COLOR                     "Black"
#define BLACK_COLOR_1                   "grey1"
#define BLACK_COLOR_2                   "grey2"
#define BLACK_COLOR_3                   "grey3"
#define LOCKED_INVERSE_COLOR            "gray37"
#define LOCKED_INVERSE_COLOR_1          "seashell4"
#define LOCKED_INVERSE_COLOR_2          "ivory4"
#define LOCKED_INVERSE_COLOR_3          "gray40"
#define TR_BAR_COLOR                    "gold"
#define TR_BAR_COLOR_1                  "goldenrod"
#define TR_BAR_COLOR_2                  "gold1"
#define TR_BAR_COLOR_3                  "orange"
#define LABEL_BG_COLOR                  "NavajoWhite1"
#define LABEL_BG_COLOR_1                "NavajoWhite"
#define LABEL_BG_COLOR_2                "moccasin"
#define LABEL_BG_COLOR_3                "bisque"
#define BUTTON_BACKGROUND_COLOR         "SteelBlue1"
#define BUTTON_BACKGROUND_COLOR_1       "LightSkyBlue"
#define BUTTON_BACKGROUND_COLOR_2       "DeepSkyBlue1"
#define BUTTON_BACKGROUND_COLOR_3       "SteelBlue2"
#define EMAIL_ACTIVE_COLOR              "pink"
#define EMAIL_ACTIVE_COLOR_1            "LightPink"
#define EMAIL_ACTIVE_COLOR_2            "MistyRose1"
#define EMAIL_ACTIVE_COLOR_3            "RosyBrown1"
#define CHAR_BACKGROUND_COLOR           "lightskyblue2"
#define CHAR_BACKGROUND_COLOR_1         "SkyBlue1"
#define CHAR_BACKGROUND_COLOR_2         "LightBlue1"
#define CHAR_BACKGROUND_COLOR_3         "SkyBlue"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR    "green"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_1  "LawnGreen"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_2  "chartreuse"
#define FTP_BURST_TRANSFER_ACTIVE_COLOR_3  "green1"
#ifdef _WITH_WMO_SUPPORT
#define WMO_BURST_TRANSFER_ACTIVE_COLOR    "yellow"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_1  "LightYellow"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_2  "LightYellow2"
#define WMO_BURST_TRANSFER_ACTIVE_COLOR_3  "yellow2"
#endif


/* Position of each color in global array */
/*****************/
/* see afddefs.h */
/*****************/

/* The following definitions are for show_ilog and show_olog only */
#define CHECK_TIME_INTERVAL             10

/* Structure definitions */
struct apps_list
       {
          char  progname[MAX_PROCNAME_LENGTH + 1];
          pid_t pid;
          int   position;           /* Position in FSA or MSA. */
          int   app_reply_fd[2];
       };

struct coord
       {
          int x;
          int y;
       };

struct dir_options
       {
          int  no_of_dir_options;
          char aoptions[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
          char dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char url[MAX_PATH_LENGTH];
       };

#define CREATE_LFC_STRING(str, value)                       \
        {                                                   \
           (str)[5] = '\0';                                 \
           if ((value) < 10)                                \
           {                                                \
              (str)[0] = ' ';                               \
              (str)[1] = ' ';                               \
              (str)[2] = ' ';                               \
              (str)[3] = ' ';                               \
              (str)[4] = (value) + '0';                     \
           }                                                \
           else if ((value) < 100)                          \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ' ';                          \
                   (str)[2] = ' ';                          \
                   (str)[3] = ((value) / 10) + '0';         \
                   (str)[4] = ((value) % 10) + '0';         \
                }                                           \
           else if ((value) < 1000)                         \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ' ';                          \
                   (str)[2] = ((value) / 100) + '0';        \
                   (str)[3] = (((value) / 10) % 10) + '0';  \
                   (str)[4] = ((value) % 10) + '0';         \
                }                                           \
           else if ((value) < 10000)                        \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ((value) / 1000) + '0';       \
                   (str)[2] = (((value) / 100) % 10) + '0'; \
                   (str)[3] = (((value) / 10) % 10) + '0';  \
                   (str)[4] = ((value) % 10) + '0';         \
                }                                           \
                else                                        \
                {                                           \
                   (str)[0] = (((value) / 10000) % 10) + '0';\
                   (str)[1] = (((value) / 1000) % 10) + '0';\
                   (str)[2] = (((value) / 100) % 10) + '0'; \
                   (str)[3] = (((value) / 10) % 10) + '0';  \
                   (str)[4] = ((value) % 10) + '0';         \
                }                                           \
        }

#define CREATE_FC_STRING(str, value)                        \
        {                                                   \
           (str)[4] = '\0';                                 \
           if ((value) < 10)                                \
           {                                                \
              (str)[0] = ' ';                               \
              (str)[1] = ' ';                               \
              (str)[2] = ' ';                               \
              (str)[3] = (value) + '0';                     \
           }                                                \
           else if ((value) < 100)                          \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ' ';                          \
                   (str)[2] = ((value) / 10) + '0';         \
                   (str)[3] = ((value) % 10) + '0';         \
                }                                           \
           else if ((value) < 1000)                         \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ((value) / 100) + '0';        \
                   (str)[2] = (((value) / 10) % 10) + '0';  \
                   (str)[3] = ((value) % 10) + '0';         \
                }                                           \
                else                                        \
                {                                           \
                   (str)[0] = (((value) / 1000) % 10) + '0';\
                   (str)[1] = (((value) / 100) % 10) + '0'; \
                   (str)[2] = (((value) / 10) % 10) + '0';  \
                   (str)[3] = ((value) % 10) + '0';         \
                }                                           \
        }

#define CREATE_FR_STRING(str, value)                        \
        {                                                   \
           (str)[4] = '\0';                                 \
           if ((value) < 1.0)                               \
           {                                                \
              (str)[0] = ' ';                               \
              (str)[1] = '0';                               \
              (str)[2] = '.';                               \
              (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
           }                                                \
           else if ((value) < 10.0)                         \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = (int)(value) + '0';           \
                   (str)[2] = '.';                          \
                   (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
                }                                           \
           else if ((value) < 100.0)                        \
                {                                           \
                   (str)[0] = ((int)(value) / 10) + '0';    \
                   (str)[1] = ((int)(value) % 10) + '0';    \
                   (str)[2] = '.';                          \
                   (str)[3] = ((int)((value) * 10.0) % 10) + '0';\
                }                                           \
           else if ((value) < 1000.0)                       \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ((int)(value) / 100) + '0';   \
                   (str)[2] = ((int)((value) / 10) % 10) + '0';\
                   (str)[3] = ((int)(value) % 10) + '0';    \
                }                                           \
                else                                        \
                {                                           \
                   (str)[0] = ((int)((value) / 1000) % 10) + '0';\
                   (str)[1] = ((int)((value) / 100) % 10) + '0'; \
                   (str)[2] = (((int)(value) / 10) % 10) + '0';  \
                   (str)[3] = ((int)(value) % 10) + '0';    \
                }                                           \
        }

#define CREATE_SFC_STRING(str, value)                       \
        {                                                   \
           (str)[3] = '\0';                                 \
           if ((value) < 10)                                \
           {                                                \
              (str)[0] = ' ';                               \
              (str)[1] = ' ';                               \
              (str)[2] = (value) + '0';                     \
           }                                                \
           else if ((value) < 100)                          \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = ((value) / 10) + '0';         \
                   (str)[2] = ((value) % 10) + '0';         \
                }                                           \
                else                                        \
                {                                           \
                   (str)[0] = (((value) / 100) % 10) + '0'; \
                   (str)[1] = (((value) / 10) % 10) + '0';  \
                   (str)[2] = ((value) % 10) + '0';         \
                }                                           \
        }

#define CREATE_EC_STRING(str, value)                        \
        {                                                   \
           (str)[2] = '\0';                                 \
           if ((value) < 10)                                \
           {                                                \
              (str)[0] = ' ';                               \
              (str)[1] = (value) + '0';                     \
           }                                                \
           else if ((value) < 100)                          \
                {                                           \
                   (str)[0] = ((value) / 10) + '0';         \
                   (str)[1] = ((value) % 10) + '0';         \
                }                                           \
                else                                        \
                {                                           \
                   (str)[0] = ' ';                          \
                   (str)[1] = '>';                          \
                }                                           \
        }

#define CREATE_FS_STRING(str, value)                        \
        {                                                   \
           (str)[4] = '\0';                                 \
           if ((value) < KILOBYTE)                          \
           {                                                \
              if ((value) < 1000)                           \
              {                                             \
                 (str)[3] = 'B';                            \
                 if ((value) < 10)                          \
                 {                                          \
                    (str)[0] = ' ';                         \
                    (str)[1] = ' ';                         \
                    (str)[2] = (value) + '0';               \
                 }                                          \
                 else if ((value) < 100)                    \
                      {                                     \
                         (str)[0] = ' ';                    \
                         (str)[1] = ((value) / 10) + '0';   \
                         (str)[2] = ((value) % 10) + '0';   \
                      }                                     \
                      else                                  \
                      {                                     \
                         (str)[0] = ((value) / 100) + '0';  \
                         (str)[1] = (((value) / 10) % 10) + '0';\
                         (str)[2] = ((value) % 10) + '0';   \
                      }                                     \
              }                                             \
              else                                          \
              {                                             \
                 (str)[0] = '0';                            \
                 (str)[1] = '.';                            \
                 (str)[2] = '9';                            \
                 (str)[3] = 'K';                            \
              }                                             \
           }                                                \
           else if ((value) < MEGABYTE)                     \
                {                                           \
                   if ((value) < 1000000)                   \
                   {                                        \
                      int num = (value) / KILOBYTE;         \
                                                            \
                      (str)[3] = 'K';                       \
                      if (num < 10)                         \
                      {                                     \
                         num = ((value) * 10) / KILOBYTE;   \
                         (str)[0] = (num / 10) + '0';       \
                         (str)[1] = '.';                    \
                         (str)[2] = (num % 10) + '0';       \
                      }                                     \
                      else if (num < 100)                   \
                           {                                \
                              (str)[0] = ' ';               \
                              (str)[1] = (num / 10) + '0';  \
                              (str)[2] = (num % 10) + '0';  \
                           }                                \
                           else                             \
                           {                                \
                              (str)[0] = (num / 100) + '0'; \
                              (str)[1] = ((num / 10) % 10) + '0';\
                              (str)[2] = (num % 10) + '0';  \
                           }                                \
                   }                                        \
                   else                                     \
                   {                                        \
                      (str)[0] = '0';                       \
                      (str)[1] = '.';                       \
                      (str)[2] = '9';                       \
                      (str)[3] = 'M';                       \
                   }                                        \
                }                                           \
           else if ((value) < GIGABYTE)                     \
                {                                           \
                   if ((value) < 1000000000)                \
                   {                                        \
                      int num = (value) / MEGABYTE;         \
                                                            \
                      (str)[3] = 'M';                       \
                      if (num < 10)                         \
                      {                                     \
                         num = ((value) * 10) / MEGABYTE;   \
                         (str)[0] = (num / 10) + '0';       \
                         (str)[1] = '.';                    \
                         (str)[2] = (num % 10) + '0';       \
                      }                                     \
                      else if (num < 100)                   \
                           {                                \
                              (str)[0] = ' ';               \
                              (str)[1] = (num / 10) + '0';  \
                              (str)[2] = (num % 10) + '0';  \
                           }                                \
                           else                             \
                           {                                \
                              (str)[0] = (num / 100) + '0'; \
                              (str)[1] = ((num / 10) % 10) + '0';\
                              (str)[2] = (num % 10) + '0';  \
                           }                                \
                   }                                        \
                   else                                     \
                   {                                        \
                      (str)[0] = '0';                       \
                      (str)[1] = '.';                       \
                      (str)[2] = '9';                       \
                      (str)[3] = 'G';                       \
                   }                                        \
                }                                           \
                else                                        \
                {                                           \
                   int num = (value) / GIGABYTE;            \
                                                            \
                   (str)[3] = 'G';                          \
                   if (num < 10)                            \
                   {                                        \
                      (str)[0] = num + '0';                 \
                      (str)[1] = '.';                       \
                      num = (value) / 107374182;            \
                      (str)[2] = (num % 10) + '0';          \
                   }                                        \
                   else if (num < 100)                      \
                        {                                   \
                           (str)[0] = ' ';                  \
                           (str)[1] = (num / 10) + '0';     \
                           (str)[2] = (num % 10) + '0';     \
                        }                                   \
                        else                                \
                        {                                   \
                           (str)[0] = ((num / 100) % 10) + '0';\
                           (str)[1] = ((num / 10) % 10) + '0';\
                           (str)[2] = (num % 10) + '0';     \
                        }                                   \
                }                                           \
        }

/* Function Prototypes */
extern int  check_info_file(void),
            get_current_jid_list(void),
            prepare_printer(int *),
            prepare_file(int *),
            sfilter(char *, char *),      /* show_?log */
            store_host_names(char **, char *),
            xrec(Widget, char, char *, ...);
extern void check_nummeric(Widget, XtPointer, XtPointer),
            get_dir_options(int, struct dir_options *),
            get_ip_no(char *, char *),
            get_printer_cmd(char *, char *),
            init_color(Display *),
            locate_xy(int, int *, int *),
            make_xprocess(char *, char *, char **, int),
            print_data(void),
            print_data_button(Widget, XtPointer, XtPointer),
            read_setup(char *, int *, int *),
            remove_passwd(char *),
            reset_message(Widget),                /* show_?log */
            show_info(char *),                    /* show_?log */
            show_message(Widget, char *),         /* show_?log */
            update_time(XtPointer, XtIntervalId), /* show_?log */
            wait_visible(Widget),
            write_setup(int, int);

#endif /* __x_common_defs_h */
