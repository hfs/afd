/*
 *  logdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __logdefs_h
#define __logdefs_h

#define MAX_SYSTEM_LOG_FILES              4            /* Must be > 1!   */
#define SYSTEM_LOG_RESCAN_TIME            10
#define SYSTEM_LOG_NAME                   "SYSTEM_LOG."
#define MAX_RECEIVE_LOG_FILES             7            /* Must be > 1!   */
#define RECEIVE_LOG_NAME                 "RECEIVE_LOG."
#define MAX_RECEIVE_DB_LOG_FILES          2            /* Must be > 1!   */
#define MAX_TRANSFER_LOG_FILES            7            /* Must be > 1!   */
#define TRANSFER_LOG_NAME                 "TRANSFER_LOG."
#define MAX_TRANS_DB_LOG_FILES            2            /* Must be > 1!   */
#define TRANS_DB_LOG_RESCAN_TIME          10
#define TRANS_DB_LOG_NAME                 "TRANS_DB_LOG."

/* Definitions for the log process of afd_monitor. */
#define MAX_MON_SYS_LOG_FILES             4            /* Must be > 1!   */
#define MON_SYS_LOG_RESCAN_TIME           5
#define MON_SYS_LOG_NAME                  "MON_SYS_LOG."
#define MAX_MON_LOG_FILES                 14           /* Must be > 1!   */
#define MON_LOG_NAME                      "MONITOR_LOG."

#define BUFFERED_WRITES_BEFORE_FLUSH_FAST 5
#define BUFFERED_WRITES_BEFORE_FLUSH_SLOW 20

#ifdef _INPUT_LOG
/* Definitions for output logging */
#define INPUT_BUFFER_FILE                 "INPUT_LOG."
#endif
#ifdef _OUTPUT_LOG
/* Definitions for output logging */
#define OUTPUT_BUFFER_FILE                "OUTPUT_LOG."
#endif
#ifdef _DELETE_LOG
/* Definitions for delete logging */
#define DELETE_BUFFER_FILE                "DELETE_LOG."
#endif

/* Function prototypes. */
extern int  fprint_dup_msg(FILE *, int, char *, char *, time_t),
#ifdef _FIFO_DEBUG
            logger(FILE *, int, char *, int);
#else
            logger(FILE *, int, int);
#endif
extern FILE *open_log_file(char *);

#endif /* __logdefs_h */
