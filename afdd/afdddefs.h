/*
 *  afdddefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __afdddefs_h
#define __afdddefs_h

#define HUNK_MAX             4096
#define DEFAULT_AFD_PORT_NO  "4444"
#define DEFAULT_FILE_NO      0
#define STAT_INTERVAL        120   /* in seconds */
#define EVERYTHING           -1
#define AFDD_CMD_TIMEOUT     900
#define MAX_AFDD_CONNECTIONS 5
#define AFD_SHUTTING_DOWN    124

#define DEFAULT_CHECK_INTERVAL 3   /* Default interval in seconds to */
                                   /* check if certain values have   */
                                   /* changed in the FSA.            */

#define HELP_CMD             "HELP\r\n"
#define QUIT_CMD             "QUIT\r\n"
#define TRACEI_CMD           "TRACEI"
#define TRACEI_CMDL          "TRACEI\r\n"
#define TRACEO_CMD           "TRACEO"
#define TRACEO_CMDL          "TRACEO\r\n"
#define TRACEF_CMD           "TRACEF"
#define TRACEF_CMDL          "TRACEF\r\n"
#define ILOG_CMD             "ILOG"
#define ILOG_CMDL            "ILOG\r\n"
#define OLOG_CMD             "OLOG"
#define OLOG_CMDL            "OLOG\r\n"
#define SLOG_CMD             "SLOG"
#define SLOG_CMDL            "SLOG\r\n"
#define TLOG_CMD             "TLOG"
#define TLOG_CMDL            "TLOG\r\n"
#define TDLOG_CMD            "TDLOG"
#define TDLOG_CMDL           "TDLOG\r\n"
#define PROC_CMD             "PROC\r\n"
#define DISC_CMD             "DISC\r\n"
#define STAT_CMD             "STAT"
#define STAT_CMDL            "STAT\r\n"
#define START_STAT_CMD       "SSTAT"
#define START_STAT_CMDL      "SSTAT\r\n"
#define LDB_CMD              "LDB\r\n"
#define LRF_CMD              "LRF\r\n"
#define INFO_CMD             "INFO "
#define INFO_CMDL            "INFO\r\n"
#define AFDSTAT_CMD          "AFDSTAT"
#define AFDSTAT_CMDL         "AFDSTAT\r\n"

#define QUIT_SYNTAX          "214 Syntax: QUIT (terminate service)"
#define HELP_SYNTAX          "214 Syntax: HELP [ <sp> <string> ]"
#define TRACEI_SYNTAX        "214 Syntax: TRACEI [<sp> <file name>] (trace input)"
#define TRACEO_SYNTAX        "214 Syntax: TRACEO [<sp> <file name>] (trace output)"
#define TRACEF_SYNTAX        "214 Syntax: TRACEF [<sp> <file name>] (trace input)"
#define ILOG_SYNTAX          "214 Syntax: ILOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] (input log)"
#define OLOG_SYNTAX          "214 Syntax: OLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] (output log)"
#define SLOG_SYNTAX          "214 Syntax: SLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] (system log)"
#define TLOG_SYNTAX          "214 Syntax: TLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] (transfer log)"
#define TDLOG_SYNTAX         "214 Syntax: TDLOG [<sp> <search string>] [<sp> -<lines>] [<sp> +<duration>] (transfer debug log)"
#define PROC_SYNTAX          "214 Syntax: PROC (shows all process of the AFD)"
#define DISC_SYNTAX          "214 Syntax: DISC (shows disk usage)"
#define STAT_SYNTAX          "214 Syntax: STAT [<sp> <host name>] [<sp> -H | -D | -Y [<sp> n]]"
#define START_STAT_SYNTAX    "214 Syntax: SSTAT (start summary status of AFD)"
#define LDB_SYNTAX           "214 Syntax: LDB (list AMG database)"
#define LRF_SYNTAX           "214 Syntax: LRF (list rename file)"
#define INFO_SYNTAX          "214 Syntax: INFO <sp> <host name>"
#define AFDSTAT_SYNTAX       "214 Syntax: AFDSTAT [<sp> <host name>]"

/* Function prototypes */
extern int  get_display_data(char *, char *, int, int, int);
extern void check_changes(FILE *),
            display_file(FILE *),
            handle_request(int, int),
            show_host_list(FILE *),
            show_summary_stat(FILE *);

#endif /* __afdddefs_h */
