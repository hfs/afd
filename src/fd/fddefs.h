/*
 *  fddefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#define WITH_SIGNAL_WAKEUP

/* Definitions for group transfer rate limit (gtrl). */
#define TRL_FILENAME             "group.transfer_rate_limit"
#define TRL_MEMBER_ID            "members"
#define TRL_MEMBER_ID_LENGTH     (sizeof(TRL_MEMBER_ID) - 1)
#define TRL_LIMIT_ID             "limit"
#define TRL_LIMIT_ID_LENGTH      (sizeof(TRL_LIMIT_ID) - 1)

/* The different types of locking */
#define MAX_LOCK_FILENAME_LENGTH 32
#define LOCK_NOTATION_LENGTH     40
#define LOCK_FILENAME            "CONNECTED______.LCK"
#define DOT_NOTATION             "."

#define MAIL_HEADER_IDENTIFIER   "MAIL-"

#ifdef WITH_SSH_FINGERPRINT
# define SSH_RSA_KEY             1
# define SSH_DSS_KEY             2
# define SSH_PGP_DSS_KEY         3
# define SSH_PGP_RSA_KEY         4
#endif

/* Miscellaneous definitions */
#define FAILED_TO_CREATE_ARCHIVE_DIR 1 /* If archive_file() fails to     */
                                       /* create the archive directory   */
                                       /* this is set.                   */
#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH      1024
#endif
#define MAX_FD_DIR_CHECK         152   /* FD should only check the file  */
                                       /* directory if it is less then   */
                                       /* this value. Don't forget to    */
                                       /* add 2 for "." and "..".        */
#define ABORT_TIMEOUT            10    /* This is the time in seconds,   */
                                       /* that the transferring jobs     */
                                       /* have before they get killed.   */
#define FD_QUICK_TIMEOUT         10    /* The timeout when we have done  */
                                       /* a quick stop of the FD.        */
#define FD_TIMEOUT               30    /* The timeout when we have done  */
                                       /* a normal or save stop of the   */
                                       /* FD.                            */
#define CD_TIMEOUT               600   /* Timeout remote change          */
                                       /* directory (10min).             */
#define DIR_CHECK_TIME           1500  /* Time (every 25min) when to     */
                                       /* check in file directory for    */
                                       /* jobs without the corresponding */
                                       /* message.                       */
#define RETRY_THRESHOLD          4     /* Threshold when to increase     */
                                       /* aging factor for error jobs in */
                                       /* queue.                         */
#ifdef WITH_SSH_FINGERPRINT
# define MAX_FINGERPRINT_LENGTH  47
#endif

/* Definitions of different exit status */
#define NO_MESSAGE               -2    /* When there are no more jobs to */
                                       /* be done we return this value.  */

/* Exit status of sf_xxx and gf_xxx */
#define TRANSFER_SUCCESS         0
#define TRANSFER_SUCCESS_STR     "Transfer success"
#define CONNECT_ERROR            1
#define CONNECT_ERROR_STR        "Connect error"
#define USER_ERROR               2
#define USER_ERROR_STR           "User error"
#define PASSWORD_ERROR           3
#define PASSWORD_ERROR_STR       "Password error"
#define TYPE_ERROR               4
#define TYPE_ERROR_STR           "Type error"
#define LIST_ERROR               5
#define LIST_ERROR_STR           "List error"
#define MAIL_ERROR               6
#define MAIL_ERROR_STR           "Mail error"
#define JID_NUMBER_ERROR         7      /* When parameters for sf_xxx is */
                                        /* evaluated and it is not able  */
                                        /* to determine the JID number.  */
#define JID_NUMBER_ERROR_STR     "JID number not found"
#define GOT_KILLED               8
#define GOT_KILLED_STR           "Process was killed"
#ifdef WITH_SSL
# define AUTH_ERROR              9
# define AUTH_ERROR_STR          "TLS/SSL authentification failed"
#endif
#define OPEN_REMOTE_ERROR        10
#define OPEN_REMOTE_ERROR_STR    "Failed to open remote file"
#define WRITE_REMOTE_ERROR       11
#define WRITE_REMOTE_ERROR_STR   "Failed to write to remote file"
#define CLOSE_REMOTE_ERROR       12
#define CLOSE_REMOTE_ERROR_STR   "Failed to close remote file"
#define MOVE_REMOTE_ERROR        13
#define MOVE_REMOTE_ERROR_STR    "Failed to move remote file"
#define CHDIR_ERROR              14
#define CHDIR_ERROR_STR          "Failed to change remote directory"
#define WRITE_LOCK_ERROR         15
#define WRITE_LOCK_ERROR_STR     "Failed to create remote lock file"
#define REMOVE_LOCKFILE_ERROR    16
#define REMOVE_LOCKFILE_ERROR_STR "Failed to remove remote lock_file"
/* NOTE: STAT_ERROR              17 is defined in afddefs.h! */
#define STAT_ERROR_STR           "Failed to stat local file"
#define MOVE_ERROR               18     /* Used by sf_loc()              */
#define MOVE_ERROR_STR           "Failed to move local file"
#define RENAME_ERROR             19     /* Used by sf_loc()              */
#define RENAME_ERROR_STR         "Failed to rename local file"
#define TIMEOUT_ERROR            20
#define TIMEOUT_ERROR_STR        "Operation received timeout"
#ifdef _WITH_WMO_SUPPORT
# define CHECK_REPLY_ERROR       21     /* Used by sf_wmo()              */
# define CHECK_REPLY_ERROR_STR   "Received negative aknowledge"
#endif
#define READ_REMOTE_ERROR        22
#define READ_REMOTE_ERROR_STR    "Failed to read from remote file"
#define SIZE_ERROR               23
#define SIZE_ERROR_STR           "Failed to get size of remote file"
#define DATE_ERROR               24
#define DATE_ERROR_STR           "Failed to get date of remote file"
#define QUIT_ERROR               25
#define QUIT_ERROR_STR           "Failed to quit"
/* NOTE: MKDIR_ERROR             26 is defined in afddefs.h! */
#define MKDIR_ERROR_STR          "Failed to create directory"
/* NOTE: CHOWN_ERROR             27 is defined in afddefs.h! */
#define CHOWN_ERROR_STR          "Failed to change owner of file"
#define CONNECTION_RESET_ERROR   28
#define CONNECTION_RESET_ERROR_STR "Connection reset by peer"
#define CONNECTION_REFUSED_ERROR 29
#define CONNECTION_REFUSED_ERROR_STR "Connection refused"
#define OPEN_LOCAL_ERROR         30
#define OPEN_LOCAL_ERROR_STR     "Failed to open local file"
#define READ_LOCAL_ERROR         31
#define READ_LOCAL_ERROR_STR     "Failed to read from local file"
#define LOCK_REGION_ERROR        32     /* Process failed to lock region */
                                        /* in FSA                        */
#define LOCK_REGION_ERROR_STR    "Failed to lock region in FSA"
#define UNLOCK_REGION_ERROR      33     /* Process failed to unlock      */
                                        /* region in FSA                 */
#define UNLOCK_REGION_ERROR_STR  "Failed to unlock region in FSA"
/* NOTE: ALLOC_ERROR             34 is defined in afddefs.h! */
#define ALLOC_ERROR_STR          "Failed to allocate memory"
#define SELECT_ERROR             35
#define SELECT_ERROR_STR         "select error"
#define WRITE_LOCAL_ERROR        36
#define WRITE_LOCAL_ERROR_STR    "Failed to write to local file"
#define OPEN_FILE_DIR_ERROR      40     /* File directory does not exist */
#define OPEN_FILE_DIR_ERROR_STR  "Local file directory does not exist"
#define NO_MESSAGE_FILE          41     /* The message file does not     */
                                        /* exist.                        */
#define NO_MESSAGE_FILE_STR      "The message file does not exist"
#define REMOTE_USER_ERROR        50     /* Failed to send mail address.  */
#define REMOTE_USER_ERROR_STR    "Failed to send mail address"
#define DATA_ERROR               51     /* Failed to send data command.  */
#define DATA_ERROR_STR           "Failed to send SMTP DATA command"
#ifdef _WITH_WMO_SUPPORT
# define SIG_PIPE_ERROR          52
# define SIG_PIPE_ERROR_STR      "sigpipe error"
#endif
#ifdef _WITH_MAP_SUPPORT
# define MAP_FUNCTION_ERROR      55
# define MAP_FUNCTION_ERROR_STR  "Error in MAP function"
#endif
#define SYNTAX_ERROR             60
#define SYNTAX_ERROR_STR         "Syntax error"
#define NO_FILES_TO_SEND         61
#define NO_FILES_TO_SEND_STR     "No files to send"
#define STILL_FILES_TO_SEND      62
#define STILL_FILES_TO_SEND_STR  "More files to send"
/* NOTE: MAX_ERROR_STR_LENGTH    34 is defined in afddefs.h! */

#ifdef _WITH_WMO_SUPPORT
# define NEGATIV_ACKNOWLEDGE     -10
#endif

#define PENDING                  -2
#define REMOVED                  -3

/* Definitions for functon reset_fsa(). So */
/* it knows which value it has to reset.   */
#define IS_FAULTY_VAR             2

/* Definition of the different names of locking. */
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
#define POSTFIX                  4
#ifdef WITH_READY_FILES
# define READY_FILE_ASCII        "RDYA"
# define LOCK_READY_A_FILE       "RDY A"
# define READY_A_FILE            4
# define READY_FILE_BINARY       "RDYB"
# define LOCK_READY_B_FILE       "RDY B"
# define READY_B_FILE            5
#endif

/* Definitions for tracing. */
#define BIN_R_TRACE              1
#define BIN_CMD_R_TRACE          2
#define R_TRACE                  3
#define BIN_W_TRACE              4
#define BIN_CMD_W_TRACE          5
#define W_TRACE                  6
#define C_TRACE                  7

/* Default definitions. */
#define DEFAULT_ERROR_REPEAT     1
#define DEFAULT_LOCK             DOT
#define DEFAULT_TRANSFER_MODE    'I'
#define DEFAULT_NOOP_INTERVAL    30

/* Definition for special_flag in structure job. */
#define FILE_NAME_IS_HEADER            1
#define FILE_NAME_IS_SUBJECT           2
#define FILE_NAME_IS_USER              4
#ifdef WITH_EUMETSAT_HEADERS
# define ADD_EUMETSAT_HEADER           4
#endif
#define EXEC_FTP                       8
#define ADD_MAIL_HEADER                8
#define ATTACH_FILE                    16
#define CHANGE_UID_GID                 16
#ifdef _WITH_WMO_SUPPORT
# define WMO_CHECK_ACKNOWLEDGE         16
# define WITH_SEQUENCE_NUMBER          32
#endif
#define ENCODE_ANSI                    32
#define CHANGE_PERMISSION              64
#define ATTACH_ALL_FILES               64
#define MAIL_SUBJECT                   256
#define FORCE_COPY                     256
#define CHANGE_FTP_MODE                512 /* We might at a latter stage */
                                           /* change the default mode.   */
#define FILE_NAME_IS_TARGET            512
#ifdef _WITH_TRANS_EXEC
# define TRANS_EXEC                    1024
#endif /* _WITH_TRANS_EXEC */
#define CREATE_TARGET_DIR              2048
#define OLD_ERROR_JOB                  4096
#define SMTP_SERVER_NAME_IN_AFD_CONFIG 8192
#define SEQUENCE_LOCKING               16384
#define LOGIN_EXEC_FTP                 32768
#define TRANS_RENAME_PRIMARY_ONLY      65536
#define TRANS_RENAME_SECONDARY_ONLY    131072
#ifdef WITH_ERROR_QUEUE
# define IN_ERROR_QUEUE                262144
#endif
#define SMTP_SERVER_NAME_IN_MESSAGE    524288

#ifdef _WITH_BURST_2
# define MORE_DATA_FIFO                "/more_data_"

/* Definition for values that have changed during a burst. */
# define USER_CHANGED                  1
# define TYPE_CHANGED                  2
# define TARGET_DIR_CHANGED            4
# ifdef WITH_SSL
#  define AUTH_CHANGED                 8
# endif
#endif /* _WITH_BURST_2 */

#ifdef _NEW_STUFF
/* Structure for holding all append data. */
struct append_data
       {
          char         file_name[MAX_FILENAME_LENGTH];
          time_t       file_time;
          unsigned int job_id;
       };
#endif /* _NEW_STUFF */

/* Structure that holds the data for one internal messages of the FD */
#define MSG_QUE_BUF_SIZE 10000

#define RESEND_JOB       2
struct queue_buf
       {
          double        msg_number;
          pid_t         pid;
          time_t        creation_time;
          off_t         file_size_to_send;
          unsigned int  files_to_send;
          unsigned int  retries;            /* The number of times it    */
                                            /* was tried to send this    */
                                            /* job.                      */
          int           pos;                /* Position in msg_cache_buf */
                                            /* for sf_xxx or position in */
                                            /* FRA for gf_xxx.           */
          int           connect_pos;
          unsigned char special_flag;       /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*  2    | RESEND_JOB       |*/
                                            /*+------+------------------+*/
          char          msg_name[MAX_MSG_NAME_LENGTH];
       };

struct msg_cache_buf
       {
          char         host_name[MAX_HOSTNAME_LENGTH + 1];
          time_t       msg_time;       /* time of last modification */
          time_t       last_transfer_time;
          int          fsa_pos;
          int          port;           /* NOTE: only when the recipient   */
                                       /*       has a port specified will */
                                       /*       this be set, otherwise it */
                                       /*       will be -1.               */
          unsigned int job_id;
          unsigned int age_limit;
          char         type;           /* FTP, SMTP or LOC (file) */
          char         in_current_fsa;
       };

/* Structure that holds all data for one sf_xxx/gf_xxx job. */
struct job
       {
          int            fsa_pos;        /* Position of host in FSA      */
                                         /* structure.                   */
          off_t          lock_offset;    /* Position in FSA where to do  */
                                         /* the locking for this job.    */
          int            fra_pos;        /* Position of host in FRA      */
                                         /* structure.                   */
          int            archive_time;   /* The time how long the files  */
                                         /* should be held in the        */
                                         /* archive before they are      */
                                         /* deleted.                     */
          int            port;           /* TCP port.                    */
          int            sndbuf_size;    /* Socket send buffer size.     */
          int            rcvbuf_size;    /* Socket receive buffer size.  */
          unsigned int   unl;            /* Unique name length.          */
          unsigned int   job_id;         /* Since each host can have     */
                                         /* different type of jobs (other*/
                                         /* user, different directory,   */
                                         /* other options, etc), each of */
                                         /* these is identified by this  */
                                         /* number.                      */
#ifdef WITH_DUP_CHECK
          unsigned int   crc_id;         /* Which CRC ID file to use     */
                                         /* when dupcheck is enabled.    */
#endif
          pid_t          my_pid;         /* The process id of this       */
                                         /* process.                     */
          unsigned int   age_limit;      /* If date of file is older     */
                                         /* then age limit, file gets    */
                                         /* removed.                     */
          unsigned int   retries;        /* The number times we tried to */
                                         /* send this job.               */
#ifdef _OUTPUT_LOG
          int            archive_offset; /* When writting the archive    */
                                         /* directory to the output log, */
                                         /* only part of the path is     */
                                         /* used. This is the offset to  */
                                         /* the path we need.            */
#endif
          mode_t         chmod;          /* The permissions that the     */
                                         /* file should have.            */
          char           chmod_str[5];   /* String mode value for FTP.   */
          uid_t          user_id;        /* The user ID that the file    */
                                         /* should have.                 */
          gid_t          group_id;       /* The group ID that the file   */
                                         /* should have.                 */
          time_t         creation_time;
          unsigned int   keep_connected; /* How long the connection      */
                                         /* should be left open and wait */
                                         /* for new data.                */
          unsigned int   split_job_counter;
          unsigned short unique_number;
          char           dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char           hostname[MAX_REAL_HOSTNAME_LENGTH];
          char           host_alias[MAX_HOSTNAME_LENGTH + 1];
#if MAX_USER_NAME_LENGTH > MAX_REAL_HOSTNAME_LENGTH
          char           user[MAX_USER_NAME_LENGTH + 1];
#else
          char           user[MAX_REAL_HOSTNAME_LENGTH + 1];
#endif
#ifdef WITH_SSH_FINGERPRINT
          char           ssh_fingerprint[MAX_FINGERPRINT_LENGTH + 1];
          char           key_type;
#endif
          char           password[MAX_USER_NAME_LENGTH + 1];
          char           target_dir[MAX_RECIPIENT_LENGTH];
                                         /* Target directory on the      */
                                         /* remote side.                 */
          char           msg_name[MAX_MSG_NAME_LENGTH];
          char           smtp_server[MAX_REAL_HOSTNAME_LENGTH];
                                         /* SMTP server name.            */
          int            no_of_restart_files;
          int            subject_rule_pos;
          int            trans_rule_pos;
          int            user_rule_pos;
          char           **restart_file;
                                         /* When a transmission fails    */
                                         /* while it was transmitting a  */
                                         /* file, it writes the name of  */
                                         /* that file to the message, so */
                                         /* the next time we try to send */
                                         /* it we just append the file.  */
                                         /* This is useful for large     */
                                         /* files.                       */
          char           trans_rename_rule[MAX_RULE_HEADER_LENGTH + 1];
                                         /* FTP : Renaming files on      */
                                         /*       remote site. This is   */
                                         /*       useful when building   */
                                         /*       in directory names.    */
                                         /* SMTP: When attaching files   */
                                         /*       the rename rule will   */
                                         /*       be stored here.        */
          char           user_rename_rule[MAX_RULE_HEADER_LENGTH + 1];
                                         /* Used in conjunction with     */
                                         /* option 'file name is user'.  */
                                         /* The rename rule option       */
                                         /* allows the user to select    */
                                         /* only parts of the file name  */
                                         /* as the user name.            */
          char           subject_rename_rule[MAX_RULE_HEADER_LENGTH + 1];
                                         /* In option subject it is      */
                                         /* possible to add the filename */
                                         /* or part of it.               */
          char           lock_notation[LOCK_NOTATION_LENGTH];
                                         /* Here the user can specify    */
                                         /* the notation of the locking  */
                                         /* on the remote side.          */
          char           *lock_file_name;/* The file name to use to lock */
                                         /* on the remote host.          */
          char           archive_dir[MAX_PATH_LENGTH];
          char           mode_str[24];   /* Can be either: active,       */
                                         /* passive, extended active,    */
                                         /* extended passive or passive  */
                                         /* (with redirect).             */
          unsigned int   protocol;       /* Transmission protocol, eg:   */
                                         /* FTP, SMTP, LOC, WMO, etc.    */
#ifdef WITH_SSL
          char           auth;           /* TLS/SSL authentification.    */
                                         /*  NO   - NO authentification. */
                                         /*  YES  - Only control         */
                                         /*         connection.          */
                                         /*  BOTH - Control and data     */
                                         /*         connection.          */
#endif
          unsigned char  ssh_protocol;   /* SSH protocol version to use. */
          char           toggle_host;    /* Take the host that is        */
                                         /* currently not the active     */
                                         /* host.                        */
          char           resend;         /* Is this job resend, ie. does */
                                         /* it come from show_olog?      */
          char           transfer_mode;  /* Transfer mode, A (ASCII) or  */
                                         /* I (Image, binary).           */
                                         /* (Default I)                  */
          char           lock;           /* The type of lock on the      */
                                         /* remote site. Their are so    */
                                         /* far two possibilities:       */
                                         /*   DOT - send file name       */
                                         /*         starting with dot.   */
                                         /*         Then rename file.    */
                                         /*   LOCKFILE - Send a lock     */
                                         /*              file and after  */
                                         /*              transfer delete */
                                         /*              lock file.      */
          char           rename_file_busy;/* Character to append to file */
                                         /* name when we get file busy   */
                                         /* error when trying to open    */
                                         /* remote file.                 */
          int            no_listed;      /* No. of elements in a group.  */
          char           **group_list;   /* List of elements found in    */
                                         /* the group file.              */
          char           *charset;       /* MIME charset.                */
          char           *subject;       /* Subject for mail.            */
          char           *reply_to;      /* The address where the        */
                                         /* recipient sends the reply.   */
          char           *from;          /* The address who send this    */
                                         /* mail.                        */
#ifdef _WITH_TRANS_EXEC
          char           *trans_exec_cmd;/* String holding the exec cmd. */
          time_t         trans_exec_timeout;/* When exec command should  */
                                         /* be timed out.                */
          char           set_trans_exec_lock;/* When exec command should */
                                         /* be locked, so only one can   */
                                         /* be active for this host.     */
#endif
          char           *p_unique_name; /* Pointer to the unique name   */
                                         /* of this job.                 */
          char           *special_ptr;   /* Used to point to allocated   */
                                         /* memory, eg for option        */
                                         /* ADD_MAIL_HEADER_ID,          */
                                         /* EUMETSAT_HEADER_ID,          */
                                         /* FTP_EXEC_CMD.                */
          unsigned int   special_flag;   /* Special flag with the        */
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
                                         /*| 4 | SMTP: Add a mail header|*/
                                         /*|   | FTP : Execute a command|*/
                                         /*|   |       on the remote    |*/
                                         /*|   |       host via the     |*/
                                         /*|   |       SITE command.    |*/
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
                                         /*|10 | Trans EXEC.            |*/
                                         /*|11 | Create target dir.     |*/
                                         /*|12 | This is an old/error   |*/
                                         /*|   | job.                   |*/
                                         /*|13 | The SMTP server name   |*/
                                         /*|   | comes from AFD_CONFIG. |*/
                                         /*|14 | FTP: Sequence locking. |*/
                                         /*|15 | Apply trans_rename for |*/
                                         /*|   | primary only.          |*/
                                         /*|16 | Apply trans_rename for |*/
                                         /*|   | secondary only.        |*/
                                         /*|17 | Is set when the job is |*/
                                         /*|   | in the error queue.    |*/
                                         /*|18 | The SMTP server name is|*/
                                         /*|   | in message.            |*/
                                         /*+---+------------------------+*/
#ifdef WITH_DUP_CHECK
          unsigned int   dup_check_flag; /* Flag storing the type of     */
                                         /* check that is to be done and */
                                         /* what type of CRC to use:     */
                                         /*+------+---------------------+*/
                                         /*|Bit(s)|       Meaning       |*/
                                         /*+------+---------------------+*/
                                         /*|27-32 | Not used.           |*/
                                         /*|   26 | DC_WARN             |*/
                                         /*|   25 | DC_STORE            |*/
                                         /*|   24 | DC_DELETE           |*/
                                         /*|17-23 | Not used.           |*/
                                         /*|   16 | DC_CRC32            |*/
                                         /*| 4-15 | Not used.           |*/
                                         /*|    3 | DC_FILE_CONT_NAME   |*/
                                         /*|    2 | DC_FILE_CONTENT     |*/
                                         /*|    1 | DC_FILENAME_ONLY    |*/
                                         /*+------+---------------------+*/
          time_t         dup_check_timeout;/* When the stored CRC for    */
                                         /* duplicate checks are no      */
                                         /* longer valid. Value is in    */
                                         /* seconds.                     */
#endif
          int            filename_pos_subject;
                                         /* Where in subject the file-   */
                                         /* name is to be positioned.    */
          char           job_no;         /* The job number of current    */
                                         /* transfer process.            */
#ifdef _OUTPUT_LOG
          char           output_log;     /* When the file name is to be  */
                                         /* logged, this variable is set */
                                         /* YES.                         */
#endif
          char           mode_flag;      /* Currently only used for FTP  */
                                         /* to indicate either active,   */
                                         /* passive and extended mode.   */
                                         /*+------+---------------------+*/
                                         /*|Bit(s)|       Meaning       |*/
                                         /*+------+---------------------+*/
                                         /*|  5-8 | Not used.           |*/
                                         /*|    4 | ALLOW_DATA_REDIRECT |*/
                                         /*|    3 | EXTENDED_MODE       |*/
                                         /*|    2 | ACTIVE_MODE         |*/
                                         /*|    1 | PASSIVE_MODE        |*/
                                         /*+------+---------------------+*/
       };

/* Structure that holds all the informations of current connections. */
struct connection
       {
          int   job_no;                  /* Job number of this host.       */
          int   fsa_pos;                 /* Position of host in FSA        */
                                         /* structure.                     */
          int   fra_pos;                 /* Position of directory in FRA   */
                                         /* structure.                     */
          pid_t pid;                     /* Process ID of job transferring */
                                         /* the files.                     */
          int   protocol;                /* Transmission protocol, either  */
                                         /* FTP, SMTP or LOC.              */
          char  hostname[MAX_HOSTNAME_LENGTH + 1];
          char  msg_name[MAX_MSG_NAME_LENGTH];
          char  dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char  temp_toggle;             /* When host has been toggled     */
                                         /* automatically we occasionally  */
                                         /* have to see if the original    */
                                         /* host is working again.         */
          char  resend;                  /* Is this a resend job from      */
                                         /* show_olog?                     */
       };

/* Definition for holding the file mask list. */
struct file_mask
       {
          int  fc;             /* number of file mask stored */
          int  fbl;            /* file buffer length */
          char *file_list;
       };

/* Definition for holding group transfer rate limit information. */
struct trl_group
       {
          char  *group_name;
          int   *fsa_pos;
          int   no_of_hosts;
          off_t limit;
       };
struct trl_cache
       {
          int   pos;              /* Position in struct trl_group. */
          int   gotcha;
          off_t trl_per_process;
       };

#define ABS_REDUCE(value)                                     \
        {                                                     \
           unsigned int tmp_value;                            \
                                                              \
           tmp_value = fsa[(value)].jobs_queued;              \
           fsa[(value)].jobs_queued = fsa[(value)].jobs_queued - 1; \
           if (fsa[(value)].jobs_queued > tmp_value)          \
           {                                                  \
              system_log(DEBUG_SIGN, __FILE__, __LINE__,      \
                         "Overflow from <%u> for %s. Trying to correct.", \
                         tmp_value, fsa[(value)].host_dsp_name);\
              fsa[(value)].jobs_queued = recount_jobs_queued((value)); \
           }                                                  \
        }

/* Function prototypes. */
extern int   append_compare(char *, char *),
             archive_file(char *, char *, struct job *),
             check_burst_2(char *, int *, unsigned int *),
             check_file_dir(int),
             check_fra_fd(void),
             eval_input_gf(int, char **, struct job *),
             eval_input_sf(int, char **, struct job *),
             eval_message(char *, struct job *),
             eval_recipient(char *, struct job *, char *, time_t),
             fd_check_fsa(void),
             fsa_attach_pos(int),
             get_file_names(char *, off_t *),
             get_job_data(unsigned int, int, time_t, off_t),
             get_remote_file_names_ftp(off_t *),
             get_remote_file_names_http(off_t *),
             get_remote_file_names_sftp(off_t *),
             gsf_check_fsa(void),
             init_fifos_fd(void),
             init_sf(int, char **, char *, int),
             init_sf_burst2(struct job *, char *, unsigned int *),
             lookup_job_id(unsigned int),
             read_current_msg_list(unsigned int **, int *),
             read_file_mask(char *, int *, struct file_mask **),
             recount_jobs_queued(int),
             recreate_msg(unsigned int),
             send_mail(char *, char *, char *, char *, char *);
extern void  calc_trl_per_process(int),
             check_fsa_entries(void),
             check_msg_time(void),
             check_queue_space(void),
             check_trl_file(void),
             fsa_detach_pos(int),
             get_group_list(void),
             get_new_positions(void),
             handle_delete_fifo(int, size_t, char *),
             handle_proxy(void),
             init_fra_data(void),
             init_gf(int, char **, int),
             init_limit_transfer_rate(void),
             init_msg_buffer(void),
             init_msg_ptrs(time_t **, unsigned int **, unsigned int **,
                           unsigned int **, off_t **, unsigned short **,
                           unsigned short **, char **, char **, char **),
             init_trl_data(void),
             limit_transfer_rate(int, off_t, clock_t),
             log_append(struct job *, char *, char *),
#ifdef WITHOUT_FIFO_RW_SUPPORT
             output_log_ptrs(int *, int *, unsigned int **, char **, char **,
#else
             output_log_ptrs(int *, unsigned int **, char **, char **,
#endif
                             unsigned short **, unsigned short **, off_t **,
                             unsigned short **, size_t *, clock_t **, char *,
                             int),
             remove_all_appends(unsigned int),
             remove_append(unsigned int, char *),
             remove_connection(struct connection *, int, time_t),
#ifdef _DELETE_LOG
             remove_job_files(char *, int, unsigned int, char),
#else
             remove_job_files(char *, int),
#endif
             remove_msg(int),
             reset_fsa(struct job *, int),
             reset_values(int, off_t, int, off_t),
             system_log(char *, char *, int, char *, ...),
             trace_log(char *, int, int, char *, int, char *, ...),
             trans_db_log(char *, char *, int, char *, char *, ...),
             trans_exec(char *, char *, char *),
             trans_log(char *, char *, int, char *, char *, ...);
#endif /* __fddefs_h */
