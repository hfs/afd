/*
 *  fddefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __fddefs_h
#define __fddefs_h

#include <time.h>                /* clock_t                              */

/* The different types of locking */
#define LOCK_FILENAME            "CONNECTED______.LCK"
#define DOT_NOTATION             "."

#define MAIL_HEADER_IDENTIFIER   "MAIL-"

/* Miscellaneous definitions */
#define FAILED_TO_CREATE_ARCHIVE_DIR 1 /* If archive_file() fails to    */
                                       /* create the archive directory  */
                                       /* this is set.                  */
#ifndef MAX_RET_MSG_LENGTH
#define MAX_RET_MSG_LENGTH       1024
#endif
#define MAX_FD_DIR_CHECK         152           /* FD should only check  */
                                               /* the file directory if */
                                               /* it is less then this  */
                                               /* value. Don't forget   */
                                               /* to add 2 for "." and  */
                                               /* "..".                 */
#define MAX_ERROR_RETRIES        3             /* The number of error   */
                                               /* messages that my be   */
                                               /* moved to the message  */
                                               /* directory.            */
#define ABORT_TIMEOUT            10            /* This is the time in   */
                                               /* seconds, that the     */
                                               /* transferring jobs have*/
                                               /* before they get       */
                                               /* killed.               */
#define FD_QUICK_TIMEOUT         10            /* The timeout when we   */
                                               /* have done a quick     */
                                               /* stop of the FD.       */
#define FD_TIMEOUT               30            /* The timeout when we   */
                                               /* have done a normal or */
                                               /* save stop of the FD.  */
#define CD_TIMEOUT               600           /* Timeout remote change */
                                               /* directory (10min).    */
#define DIR_CHECK_TIME           1500          /* Time (every 25min)    */
                                               /* when to check in file */
                                               /* directory for jobs    */
                                               /* without the           */
                                               /* corresponding message.*/

/* Definitions of different exit status */
#define NO_MESSAGE               -2            /* When there are no     */
                                               /* more jobs to be done  */
                                               /* we return this value. */

/* Exit status of sf_xxx */
#define TRANSFER_SUCCESS         0
#define CONNECT_ERROR            1
#define USER_ERROR               2
#define PASSWORD_ERROR           3
#define TYPE_ERROR               4
#define MAIL_ERROR               6
#define JID_NUMBER_ERROR         7      /* When parameters for sf_xxx is */
                                        /* evaluated and it is not able  */
                                        /* to determine the JID number.  */
#define GOT_KILLED               8
#define OPEN_REMOTE_ERROR        10
#define WRITE_REMOTE_ERROR       11
#define CLOSE_REMOTE_ERROR       12
#define MOVE_REMOTE_ERROR        13
#define CHDIR_ERROR              14
#define WRITE_LOCK_ERROR         15
#define REMOVE_LOCKFILE_ERROR    16
#define STAT_ERROR               17     /* Used by sf_loc()              */
#define MOVE_ERROR               18     /* Used by sf_loc()              */
#define RENAME_ERROR             19     /* Used by sf_loc()              */
#define FTP_TIMEOUT_ERROR        20     /* Only used for aftp            */
#ifdef _WITH_WMO_SUPPORT
#define CHECK_REPLY_ERROR        21     /* Used by sf_wmo()              */
#endif
#define OPEN_LOCAL_ERROR         30
#define READ_LOCAL_ERROR         31
#define STAT_LOCAL_ERROR         32
#define LOCK_REGION_ERROR        33     /* Process failed to lock region */
                                        /* in FSA                        */
#define UNLOCK_REGION_ERROR      34     /* Process failed to unlock      */
                                        /* region in FSA                 */
#define ALLOC_ERROR              35
#define SELECT_ERROR             36
#define OPEN_FILE_DIR_ERROR      40     /* File directory does not exist */
#define NO_MESSAGE_FILE          41     /* The message file does not     */
                                        /* exist.                        */
#define REMOTE_USER_ERROR        50     /* Failed to send mail address.  */
#define DATA_ERROR               51     /* Failed to send data command.  */
#ifdef _WITH_WMO_SUPPORT
#define SIG_PIPE_ERROR           52
#endif
#ifdef _WITH_MAP_SUPPORT
#define MAP_FUNCTION_ERROR       55
#endif
#define SYNTAX_ERROR             60
#define NO_FILES_TO_SEND         61

#ifdef _WITH_WMO_SUPPORT
#define NEGATIV_ACKNOWLEDGE      -10
#endif

#define PENDING                  -2
#define REMOVED                  -3

/* Definitions for functon reset_fsa(). So it knows which */
/* values it has to reset.                                */
#define CONNECT_STATUS_VAR        1
/* #define JOB_ID_VAR                2  NOT USED!!! */
#define NO_OF_FILES_DONE_VAR      4
#define FILE_SIZE_DONE_VAR        8
#define FILE_SIZE_IN_USE_VAR      16
#define FILE_SIZE_IN_USE_DONE_VAR 32
#define FILE_NAME_IN_USE_VAR      64
#define NO_OF_FILES_VAR           128
#define FILE_SIZE_VAR             256
#define PROC_ID_VAR               512
/* Maximum is 2147483648 (2^31) */

/* Definition of the different names of locking */
#define LOCK_DOT                 "DOT"      /* eg. .filename -> filename */
#define LOCK_DOT_VMS             "DOT_VMS"  /* Same as LOCK_DOT, however */
                                            /* VMS always puts a dot to  */
                                            /* end as well. So special   */
                                            /* care must be taken here.  */
#define LOCK_FILE                "LOCKFILE"
#define LOCK_OFF                 "OFF"
#define DOT                      1
#define DOT_VMS                  2
#define LOCKFILE                 3
#ifdef _WITH_READY_FILES
#define READY_FILE_ASCII         "RDYA"
#define LOCK_READY_A_FILE        "RDY A"
#define READY_A_FILE             4
#define READY_FILE_BINARY        "RDYB"
#define LOCK_READY_B_FILE        "RDY B"
#define READY_B_FILE             5
#endif

/* Default definitions */
#define DEFAULT_ERROR_REPEAT     1
#define DEFAULT_LOCK             DOT
#ifdef _AGE_LIMIT
#define DEFAULT_AGE_LIMIT        0
#endif
#define DEFAULT_TRANSFER_MODE    'I'

/* Definitions of identifiers in options */
#define OUTPUT_LOG_ID                  "no log output"
#define OUTPUT_LOG_ID_LENGTH           13
#define ARCHIVE_ID                     "archive"
#define ARCHIVE_ID_LENGTH              7
#define LOCK_ID                        "lock"
#define LOCK_ID_LENGTH                 4
#define AGE_LIMIT_ID                   "age-limit"
#define AGE_LIMIT_ID_LENGTH            9
#define RESTART_FILE_ID                "restart"
#define RESTART_FILE_ID_LENGTH         7
#define TRANS_RENAME_ID                "trans_rename"
#define TRANS_RENAME_ID_LENGTH         12
#define SECURE_FTP_ID                  "secure ftp"
#define SECURE_FTP_ID_LENGTH           10
#define NO_LOGIN_ID                    "no login"
#define NO_LOGIN_ID_LENGTH             8
#ifdef _WITH_WMO_SUPPORT
#define WITH_SEQUENCE_NUMBER_ID        "sequence numbering"
#define WITH_SEQUENCE_NUMBER_ID_LENGTH 18
#define CHECK_REPLY_ID                 "check reply"
#define CHECK_REPLY_ID_LENGTH          11
#endif /* _WITH_WMO_SUPPORT */
#define FILE_NAME_IS_HEADER_ID         "file name is header"
#define FILE_NAME_IS_HEADER_ID_LENGTH  19
#define FILE_NAME_IS_USER_ID           "file name is user"
#define FILE_NAME_IS_USER_ID_LENGTH    17
#define FILE_NAME_IS_SUBJECT_ID        "file name is subject"
#define FILE_NAME_IS_SUBJECT_ID_LENGTH 20
#if defined (_BURST_MODE) || defined (_OUPUT_LOG)
#define JOB_ID                         "job-id"
#define JOB_ID_LENGTH                  6
#endif
#define ADD_MAIL_HEADER_ID             "mail header"
#define ADD_MAIL_HEADER_ID_LENGTH      11
#define ATTACH_FILE_ID                 "attach file"
#define ATTACH_FILE_ID_LENGTH          11
#ifdef _WITH_EUMETSAT_HEADERS
#define EUMETSAT_HEADER_ID             "eumetsat"
#define EUMETSAT_HEADER_ID_LENGTH      8
#endif
#define CHMOD_ID                       "chmod"
#define CHMOD_ID_LENGTH                5
#define CHOWN_ID                       "chown"
#define CHOWN_ID_LENGTH                5
#define ENCODE_ANSI_ID                 "encode ansi"
#define ENCODE_ANSI_ID_LENGTH          11
#ifdef _RADAR_CHECK
#define RADAR_CHECK_ID                 "check radar time"
#define RADAR_CHECK_ID_LENGTH          16
#endif /* _RADAR_CHECK */
#define SUBJECT_ID                     "subject"
#define SUBJECT_ID_LENGTH              7
#define FORCE_COPY_ID                  "force copy"
#define FORCE_COPY_ID_LENGTH           10
#define PPROXY_ID                      "pproxy"
#define PPROXY_ID_LENGTH               6

/* Definition for special_flag in structure job */
#define FILE_NAME_IS_HEADER            1
#define FILE_NAME_IS_SUBJECT           2
#ifdef _RADAR_CHECK
#define RADAR_CHECK_FLAG               2
#endif /* _RADAR_CHECK */
#define FILE_NAME_IS_USER              4
#ifdef _WITH_EUMETSAT_HEADERS
#define ADD_EUMETSAT_HEADER            4
#endif
#define SECURE_FTP                     8
#define ADD_MAIL_HEADER                8
#define ATTACH_FILE                    16
#define CHANGE_UID_GID                 16
#ifdef _WITH_WMO_SUPPORT
#define WMO_CHECK_ACKNOWLEDGE          16
#define WITH_SEQUENCE_NUMBER           32
#endif
#define NO_LOGIN                       32
#define ENCODE_ANSI                    32
#define CHANGE_PERMISSION              64
/* NOTE:  ERROR_FILE_UNDER_PROCESS 128 is defined in afddefs.h!!! */
#define MAIL_SUBJECT                   256
#define FORCE_COPY                     256

/* Structure that hold the data for one internal messages of the FD */
#define MSG_QUEUE_FILE   "/fd_msg_queue"
#define MSG_QUE_BUF_SIZE 10000
struct queue_buf
       {
          double       msg_number;
          char         msg_name[MAX_MSG_NAME_LENGTH];
          pid_t        pid;
          time_t       creation_time;
          int          pos;                 /* Position in msg_cache_buf */
          int          connect_pos;
          char         in_error_dir;
       };

#define MSG_CACHE_FILE "/fd_msg_cache"
struct msg_cache_buf
       {
          char         host_name[MAX_HOSTNAME_LENGTH + 1];
          time_t       msg_time;       /* time of last modification */
          time_t       last_transfer_time;
          int          fsa_pos;
          unsigned int job_id;
#ifdef _AGE_LIMIT
          unsigned int age_limit;
#endif
          char         type;           /* FTP, SMTP or LOC (file) */
          char         in_current_fsa;
       };

/* Structure that holds all data for one ftp job */
struct job
       {
          int           position;        /* Position of host in FSA      */
                                         /* structure.                   */
          int           archive_time;    /* The time how long the files  */
                                         /* should be held in the        */
                                         /* archive before they are      */
                                         /* deleted.                     */
          int           port;            /* TCP port.                    */
          int           job_id;          /* Since each host can have     */
                                         /* different type of jobs (other*/
                                         /* user, different directory,   */
                                         /* other options, etc), each of */
                                         /* these is identified by this  */
                                         /* number.                      */
#ifdef _AGE_LIMIT
          int           age_limit;       /* If date of file is older     */
                                         /* then age limit, file gets    */
                                         /* removed.                     */
#endif
#ifdef _OUTPUT_LOG
          int           archive_offset;  /* When writting the archive    */
                                         /* directory to the output log, */
                                         /* only part of the path is     */
                                         /* used. This is the offset to  */
                                         /* the path we need.            */
#endif
          mode_t        chmod;           /* The permissions that the     */
                                         /* file should have.            */
          uid_t         user_id;         /* The user ID that the file    */
                                         /* should have.                 */
          gid_t         group_id;        /* The group ID that the file   */
                                         /* should have.                 */
          char          **filename;      /* Pointer to array that holds  */
                                         /* all file names.              */
          char          hostname[MAX_FILENAME_LENGTH];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          user[MAX_USER_NAME_LENGTH + 1];
          char          password[MAX_USER_NAME_LENGTH + 1];
          char          target_dir[MAX_PATH_LENGTH];
                                         /* Target directory on the      */
                                         /* remote side.                 */
          char          msg_name[MAX_FILENAME_LENGTH];
          char          smtp_server[MAX_REAL_HOSTNAME_LENGTH];
                                         /* SMTP server name.            */
          int           no_of_restart_files;
          char          **restart_file;
                                         /* When a transmission fails    */
                                         /* while it was transmitting a  */
                                         /* file, it writes the name of  */
                                         /* that file to the message, so */
                                         /* the next time we try to send */
                                         /* it we just append the file.  */
                                         /* This is useful for large     */
                                         /* files.                       */
          char          trans_rename_rule[MAX_RULE_HEADER_LENGTH + 1];
                                         /* Renaming files on remote     */
                                         /* site. This is useful when    */
                                         /* building in directory names. */
          char          user_rename_rule[MAX_RULE_HEADER_LENGTH + 1];
                                         /* Used in conjunction with     */
                                         /* option 'file name is user'.  */
                                         /* The rename rule option       */
                                         /* allows the user to select    */
                                         /* only parts of the file name  */
                                         /* as the user name.            */
          char          lock_notation[40];
                                         /* Here the user can specify    */
                                         /* the notation of the locking  */
                                         /* on the remote side.          */
          char          archive_dir[MAX_PATH_LENGTH];
          char          msg_type;        /* Transmission protocol,       */
                                         /* either FTP, SMTP or LOC.     */
          char          error_file;      /* Is this an error file?       */
          char          toggle_host;     /* Take the host that is        */
                                         /* currently not the active     */
                                         /* host.                        */
          char          transfer_mode;   /* Transfer mode, A (ASCII) or  */
                                         /* I (Image, binary).           */
                                         /* (Default I)                  */
          char          lock;            /* The type of lock on the      */
                                         /* remote site. Their are so    */
                                         /* far two possibilities:       */
                                         /*   DOT - send file name       */
                                         /*         starting with dot.   */
                                         /*         Then rename file.    */
                                         /*   LOCKFILE - Send a lock     */
                                         /*              file and after  */
                                         /*              transfer delete */
                                         /*              lock file.      */
#ifdef _WITH_WMO_SUPPORT
          char          file_name_is_header; /* Assumes that the file name  */
                                         /* is bulletin header.            */
#endif
          char          *subject;        /* Subject for mail.            */
          char          *pproxy;         /* Polish proxy server.         */
          char          *special_ptr;    /* Used to point to allocated   */
                                         /* memory, eg for option        */
                                         /* ADD_MAIL_HEADER_ID.          */
          unsigned int  special_flag;    /* Special flag with the        */
                                         /* following meaning:           */
                                         /*+---+------------------------+*/
                                         /*|Bit|      Meaning           |*/
                                         /*+---+------------------------+*/
                                         /*| 1 | File name is bulletin  |*/
                                         /*|   | header. (sf_wmo)       |*/
                                         /*| 2 | SMTP: File name is     |*/
                                         /*|   |       subject.         |*/
                                         /*|   | FTP : Check radar      |*/
                                         /*|   |       files.           |*/
                                         /*| 3 | SMTP: File name is     |*/
                                         /*|   |       user.            |*/
                                         /*|   | FTP : Eumetsat header  |*/
                                         /*| 4 | Secure FTP. First login|*/
                                         /*|   | with anonymous and     |*/
                                         /*|   | password user@host.    |*/
                                         /*|   | Under SMTP this flag   |*/
                                         /*|   | indicates to add a mail|*/
                                         /*|   | header.                |*/
                                         /*| 5 | DWD-WMO check. This    |*/
                                         /*|   | checks if the remote   |*/
                                         /*|   | site has received the  |*/
                                         /*|   | message, by receiving  |*/
                                         /*|   | the following sequence:|*/
                                         /*|   | 00000000AK (msg ok)    |*/
                                         /*|   | 00000000NA (msg NOT ok)|*/
                                         /*|   | Under SMTP it means to |*/
                                         /*|   | send this file as an   |*/
                                         /*|   | attachment.            |*/
                                         /*|   | LOC : change UID & GID |*/
                                         /*| 6 | WMO : Sequence number. |*/
                                         /*|   | FTP : No login.        |*/
                                         /*| 7 | LOC : Change permission|*/
                                         /*| 8 | Error file under       |*/
                                         /*|   | process.               |*/
                                         /*| 9 | SMTP: Subject          |*/
                                         /*+---+------------------------+*/
          char          notify;          /* Flag to notify remote user   */
                                         /* that his files have arrived. */
          char          job_no;          /* The job number of current    */
                                         /* transfer process.            */
#ifdef _OUTPUT_LOG
          char          output_log;      /* When the file name is to be  */
                                         /* logged, this variable is set */
                                         /* YES.                         */
#endif
       };

/* Structure that holds all the informations of current connections */
struct connection
       {
          int   job_no;                  /* Job number of this host.       */
          int   position;                /* Position of host in FSA        */
                                         /* structure.                     */
          pid_t pid;                     /* Process ID of job transferring */
                                         /* the files.                     */
          int   protocol;                /* Transmission protocol, either  */
                                         /* FTP, SMTP or LOC.              */
          char  hostname[MAX_HOSTNAME_LENGTH + 1];
          char  msg_name[MAX_MSG_NAME_LENGTH];
          char  error_file;              /* If this file comes from the    */
                                         /* error directory, define YES.   */
          char  temp_toggle;             /* When host has been toggled     */
                                         /* automatically we occasionally  */
                                         /* have to see if the original    */
                                         /* host is working again.         */
       };

/* Function prototypes */
extern int   archive_file(char *, char *, struct job *),
             check_file_dir(int),
             check_job_name(char *),
             eval_input_sf(int, char **, struct job *),
             eval_recipient(char *, struct job *, char *),
             eval_message(char *, struct job *),
             get_file_names(char *, off_t *),
             get_job_data(unsigned int, int),
             init_fifos_fd(void),
             lookup_job_id(int),
             recreate_msg(unsigned int),
             send_mail(char *, char *, char *, char *, char *);
extern void  check_fsa_entries(void),
             check_msg_time(void),
             init_msg_buffer(void),
             init_msg_ptrs(size_t *, time_t **, int **, unsigned short **,
                           char **, char **),
             init_sf(int, char **, char *, int *, int *, int),
             log_append(int, char *),
             output_log_ptrs(int *, unsigned int **, char **, char **,
                             unsigned short **, off_t **, size_t *,
                             clock_t **, char *, int),
             remove_all_appends(int),
             remove_append(int, char *),
#ifdef _DELETE_LOG
             remove_files(char *, int, int, char),
#else
             remove_files(char *, int),
#endif
             remove_msg(int),
             reset_fsa(struct job *, int, int),
             trans_log(char *, char *, int, char *, ...);
#endif /* __fddefs_h */
