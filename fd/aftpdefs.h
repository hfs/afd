/*
 *  aftpdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __aftpdefs_h
#define __aftpdefs_h

#define _AFTP

#ifndef _STANDALONE_
#include "afddefs.h"
#include "fddefs.h"
#endif

#ifdef _STANDALONE_
/* indicators for start and end of module description for man pages */
#define DESCR__S_M1             /* Start for User Command Man Page. */
#define DESCR__E_M1             /* End for User Command Man Page.   */
#define DESCR__S_M3             /* Start for Subroutines Man Page.  */
#define DESCR__E_M3             /* End for Subroutines Man Page.    */

/* Ready files for locking on remote site. */
#define _WITH_READY_FILES

#define NO                         0
#define YES                        1
#define ON                         1
#define OFF                        0
#define INCORRECT                  -1
#define SUCCESS                    0

#define DOT                        1
#define LOCK_DOT                   "DOT"
#define LOCK_OFF                   "OFF"
#ifdef _WITH_READY_FILES
#define READY_FILE_ASCII           "RDYA"
#define READY_A_FILE               2
#define READY_FILE_BINARY          "RDYB"
#define READY_B_FILE               3
#endif

#define INFO_SIGN                  "<I>"
#define WARN_SIGN                  "<W>"
#define ERROR_SIGN                 "<E>"
#define FATAL_SIGN                 "<F>"           /* donated by Paul M. */
#define DEBUG_SIGN                 "<D>"

/* Some default definitions */
#define DEFAULT_TRANSFER_TIMEOUT   120L
#define DEFAULT_TRANSFER_BLOCKSIZE 1024

/* Definitions for appending */
#define RESTART_FILE_SIZE          204800
#define MAX_SEND_BEFORE_APPEND     204800

/* Definitions for maximum values. */
#define MAX_HOSTNAME_LENGTH        8
#define MAX_FILENAME_LENGTH        256
#define MAX_PATH_LENGTH            1024
#define MAX_LINE_LENGTH            2048

/* Definitions for different exit status for aftp */
#define TRANSFER_SUCCESS           0
#define CONNECT_ERROR              1
#define USER_ERROR                 2
#define PASSWORD_ERROR             3
#define TYPE_ERROR                 4
#define OPEN_REMOTE_ERROR          10
#define WRITE_REMOTE_ERROR         11
#define CLOSE_REMOTE_ERROR         12
#define MOVE_REMOTE_ERROR          13
#define CHDIR_ERROR                14
#define FTP_TIMEOUT_ERROR          20
#define OPEN_LOCAL_ERROR           30
#define READ_LOCAL_ERROR           31
#define STAT_LOCAL_ERROR           32
#define ALLOC_ERROR                35
#define SYNTAX_ERROR               60

/* Runtime array */
#define RT_ARRAY(name, rows, columns, type)                                 \
        {                                                                   \
           int   row_counter;                                               \
                                                                            \
           if ((name = (type **)calloc((rows), sizeof(type *))) == NULL)    \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n", \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           if ((name[0] = (type *)calloc(((rows) * (columns)), sizeof(type))) == NULL)   \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN, "calloc() error : %s (%s %d)\n", \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           for (row_counter = 1; row_counter < (rows); row_counter++)       \
              name[row_counter] = (name[0] + (row_counter * (columns)));    \
        }
#define FREE_RT_ARRAY(name)         \
        {                           \
           free(name[0]);           \
           free(name);              \
        }

/* Function prototypes */
extern void my_usleep(unsigned long),
            t_hostname(char *, char *),
            trans_log(char *, char *, int, char *, ...);
extern int  rec(int, char *, char *, ...);
#endif

/* Error output in german */
/* #define _GERMAN */

#define DEFAULT_AFD_USER           "afd_ftp"
#define DEFAULT_AFD_PASSWORD       "dwd"

/* Structure that holds all data for one ftp job */
struct data
       {
          int          port;             /* TCP port.                      */
#ifdef _AGE_LIMIT
          int          age_limit;        /* If date of file is older then  */
                                         /* age limit, file gets removed.  */
#endif
          int          blocksize;        /* FTP transfer block size.       */
          int          no_of_files;      /* The number of files to be send.*/
          long         transfer_timeout; /* When to timeout the            */
                                         /* transmitting job.              */
          char         **filename;       /* Pointer to array that holds    */
                                         /* all file names.                */
          char         hostname[MAX_FILENAME_LENGTH];
          char         user[MAX_FILENAME_LENGTH];
          char         password[MAX_FILENAME_LENGTH];
          char         target_dir[MAX_PATH_LENGTH];
                                         /* Target directory on the remote */
                                         /* side.                          */
          char         restart_file[MAX_FILENAME_LENGTH];
                                         /* When a transmission fails      */
                                         /* while it was transmiiting a    */
                                         /* file, it writes the name of    */
                                         /* that file to the message, so   */
                                         /* the next time we try to send   */
                                         /* it we just append the file.    */
                                         /* This is useful for large files.*/
          char         lock_notation[MAX_FILENAME_LENGTH];
                                         /* Here the user can specify      */
                                         /* the notation of the locking on */
                                         /* the remote side.               */
          char         transfer_mode;    /* Transfer mode, A (ASCII) or I  */
                                         /* (Image, binary). (Default I)   */
          char         lock;             /* The type of lock on the remote */
                                         /* site. Their are so far two     */
                                         /* possibilities:                 */
                                         /*   DOT - send file name         */
                                         /*         starting with dot.     */
                                         /*         Then rename file.      */
                                         /*   LOCKFILE - Send a lock file  */
                                         /*              and after transfer*/
                                         /*              delete lock file. */
          char         verbose;          /* Flag to set verbose option.    */
          char         remove;           /* Remove file flag.              */
          signed char  file_size_offset; /* When doing an ls on the remote */
                                         /* site, this is the position     */
                                         /* where to find the size of the  */
                                         /* file. If it is less than 0, it */
                                         /* means that we do not want to   */
                                         /* append files that have been    */
                                         /* partly send.                   */
       };

extern int  init_aftp(int, char **, struct data *);

#endif /* __aftpdefs_h */
