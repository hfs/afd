/*
 *  afddefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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

#ifndef __afddefs_h
#define __afddefs_h

/* Indicators for start and end of module description for man pages */
#define DESCR__S_M1             /* Start for User Command Man Page. */
#define DESCR__E_M1             /* End for User Command Man Page.   */
#define DESCR__S_M3             /* Start for Subroutines Man Page.  */
#define DESCR__E_M3             /* End for Subroutines Man Page.    */

#include "config.h"
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>

/* Define the language to use */
#define GERMAN
/* #define ENGLISH */

#ifdef _LINK_MAX_TEST
#define LINKY_MAX                  4
#endif

#ifndef LINK_MAX
#define LINK_MAX                   1000 /* SCO does not define it. */
#else
#ifdef LINUX
#define REDUCED_LINK_MAX           4096 /* Reduced for testing.    */
#endif
#endif

#ifdef _NO_MMAP
/* Dummy definitions for systems that do not have mmap() */
#define PROT_READ                  1
#define PROT_WRITE                 1
#define MAP_SHARED                 1
#endif

/* Define the names of the program's */
#ifdef _NO_MMAP
#define MAPPER                     "mapper"
#endif
#define AFD                        "init_afd"
#define AMG                        "amg"
#define FD                         "fd"
#define SEND_FILE_FTP              "sf_ftp"
#define SEND_FILE_SMTP             "sf_smtp"
#define SEND_FILE_LOC              "sf_loc"
#ifdef _WITH_WMO_SUPPORT
#define SEND_FILE_WMO              "sf_wmo"
#endif
#ifdef _WITH_MAP_SUPPORT
#define SEND_FILE_MAP              "sf_map"
#endif
#define SLOG                       "system_log"
#define TLOG                       "transfer_log"
#define TDBLOG                     "trans_db_log"
#define MON_SYS_LOG                "mon_sys_log"
#define MON_LOG                    "monitor_log"
#define SHOW_ILOG                  "show_ilog"
#define SHOW_OLOG                  "show_olog"
#define SHOW_RLOG                  "show_dlog"
#define SHOW_QUEUE                 "show_queue"
#define SHOW_TRANS                 "show_trans"
#ifdef _INPUT_LOG
#define INPUT_LOG_PROCESS          "input_log"
#endif
#ifdef _OUTPUT_LOG
#define OUTPUT_LOG_PROCESS         "output_log"
#endif
#ifdef _DELETE_LOG
#define DELETE_LOG_PROCESS         "delete_log"
#endif
#define ARCHIVE_WATCH              "archive_watch"
#define SHOW_LOG                   "show_log"
#define SHOW_CMD                   "show_cmd"
#define AFD_STAT                   "afd_stat"
#define AFD_INFO                   "afd_info"
#define EDIT_HC                    "edit_hc"
#define AFD_LOAD                   "afd_load"
#define AFD_CTRL                   "afd_ctrl"
#define AFDD                       "afdd"
#define AFD_MON                    "afd_mon"
#define MON_CTRL                   "mon_ctrl"
#define MON_INFO                   "mon_info"
#define AFD_CMD                    "afdcmd"
#define VIEW_DC                    "view_dc"
#define MAX_PROCNAME_LENGTH        14
#define AFD_USER_NAME              "afd"
#define AFD_ACTIVE_FILE            "/AFD_ACTIVE"

#ifdef _DELETE_LOG
/* Reasons for deleting files. */
#define AGE_OUTPUT                 0
#define AGE_INPUT                  1
#define USER_DEL                   2
#define OTHER_DEL                  3
#endif

#ifdef _WITH_AFW2WMO
#define WMO_MESSAGE                2
#endif /* _WITH_AFW2WMO */

/* Exit status of afd program that starts the AFD. */
#define AFD_IS_ACTIVE              5
#define NO_DIR_CONFIG              -2

/* Definitions of lock ID's */
#define EDIT_HC_LOCK_ID            0    /* Edit host configuration      */
#define EDIT_DC_LOCK_ID            1    /* Edit directory configuration */
#define NO_OF_LOCK_PROC            2

/* Commands that can be send to DB_UPDATE_FIFO of the AMG */
#define HOST_CONFIG_UPDATE         4
#define DIR_CONFIG_UPDATE          5
#define REREAD_HOST_CONFIG         6
#define REREAD_DIR_CONFIG          7

#define WORK_DIR_ID                "-w"

/* Definitions when AFD file directory is running full. */
#define STOP_AMG_THRESHOLD         20
#define START_AMG_THRESHOLD        100
#ifdef _LINK_MAX_TEST
#define DANGER_NO_OF_JOBS          (LINKY_MAX / 2)
#else
#ifdef REDUCED_LINK_MAX
#define DANGER_NO_OF_JOBS          (REDUCED_LINK_MAX / 2)
#else
#define DANGER_NO_OF_JOBS          (LINK_MAX / 2)
#endif
#endif

/* Bit map flag for AMG and FD communication. */
#define INST_JOB_ACTIVE            1
#define TIME_JOB_ACTIVE            2
#define WRITTING_JID_STRUCT        64
#define FD_DIR_CHECK_ACTIVE        128

#define DIRS_IN_FILE_DIR           5

#define HOST_DISABLED              32
#define HOST_IN_DIR_CONFIG         64  /* Host in DIR_CONFIG file (bit 7)*/
#define ERROR_FILE_UNDER_PROCESS   128 /* Current job is an error file.  */

/* Process numbers that are started by AFD */
#define AMG_NO                     0
#define FD_NO                      1
#define SLOG_NO                    2
#define TLOG_NO                    3
#define TDBLOG_NO                  4
#define AW_NO                      5
#define STAT_NO                    6
#define IT_NO                      7
#define AFDD_NO                    8
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define DL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define DL_NO                      9
#define NO_OF_PROCESS              10
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define IL_NO                      9
#define NO_OF_PROCESS              10
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define IL_NO                      9
#define DL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define IL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define IL_NO                      10
#define DL_NO                      11
#define NO_OF_PROCESS              12
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define OL_NO                      9
#define NO_OF_PROCESS              10
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define OL_NO                      9
#define DL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && !defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define OL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if defined (_OUTPUT_LOG) && !defined (_INPUT_LOG) && defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define OL_NO                      10
#define DL_NO                      11
#define NO_OF_PROCESS              12
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define IL_NO                      9
#define OL_NO                      10
#define NO_OF_PROCESS              11
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define IL_NO                      9
#define OL_NO                      10
#define DL_NO                      11
#define NO_OF_PROCESS              12
#endif
#if defined (_INPUT_LOG) && defined (_OUTPUT_LOG) && defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define IL_NO                      10
#define OL_NO                      11
#define DL_NO                      12
#define NO_OF_PROCESS              13
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && !defined (_NO_MMAP)
#define NO_OF_PROCESS              9
#endif
#if !defined (_INPUT_LOG) && !defined (_OUTPUT_LOG) && !defined (_DELETE_LOG) && defined (_NO_MMAP)
#define MAPPER_NO                  9
#define NO_OF_PROCESS              10
#endif

#define NA                         -1
#define NO                         0
#define YES                        1
#define NEITHER                    2
#define OK                         1
#define INCORRECT                  -1
#define SUCCESS                    0
#define STALE                      -1
#define ON                         1
#define OFF                        0
#define ALL                        0
#define ONE                        1
#define PAUSED                     2
#define DONE                       3
#define NORMAL                     4
#define NONE                       5
#define IS_LOCKED                  -2
#define IS_NOT_LOCKED              11

#define GO_PARALLEL                8
#define SPLIT_FILE_LIST            16
#define DO_NOT_LINK_FILES          32

#define NO_PRIORITY                -1      /* For function create_name() */
                                           /* So it knows it does not    */
                                           /* need to create the name    */
                                           /* with priority, which is    */
                                           /* used by the function       */
                                           /* check_files() of the AMG.  */

#define INFO_SIGN                  "<I>"
#define CONFIG_SIGN                "<C>"
#define WARN_SIGN                  "<W>"
#define ERROR_SIGN                 "<E>"
#define FATAL_SIGN                 "<F>"   /* donated by Paul Merken.    */
#define DEBUG_SIGN                 "<D>"
#define DUMMY_SIGN                 "<#>"   /* To always show some general*/
                                           /* information, eg month.     */
#define SEPARATOR                  "-->"

/* Definitions of different exit status */
#define NOT_RUNNING                -1
#define UNKNOWN_STATE              -2
#define STOPPED                    -3
#define DIED                       -4

/* Definitions for different addresses for one host */
#define HOST_ONE                   1
#define HOST_TWO                   2
#define DEFAULT_TOGGLE_HOST        HOST_ONE
#define AUTO_TOGGLE_OPEN           '{'
#define AUTO_TOGGLE_CLOSE          '}'
#define STATIC_TOGGLE_OPEN         '['
#define STATIC_TOGGLE_CLOSE        ']'

/* Definitions of the protocol's and extensions */
#define FTP                        0
#define FTP_FLAG                   1
#define LOC                        1
#define LOC_FLAG                   2
#define LOCAL_ID                   "local"
#define SMTP                       2
#define SMTP_FLAG                  4
#ifdef _WITH_MAP_SUPPORT
#define MAP                        3
#define MAP_FLAG                   8
#endif
#ifdef _WITH_WMO_SUPPORT
#define WMO                        4
#define WMO_FLAG                   16
#endif

#define FTP_SHEME                  "ftp"
#define FTP_SHEME_LENGTH           3
#define LOC_SHEME                  "file"
#define LOC_SHEME_LENGTH           4
#ifdef _WITH_WMO_SUPPORT
#define WMO_SHEME                  "wmo"
#define WMO_SHEME_LENGTH           3
#endif
#ifdef _WITH_MAP_SUPPORT
#define MAP_SHEME                  "map"
#define MAP_SHEME_LENGTH           3
#endif
#define SMTP_SHEME                 "mailto"
#define SMTP_SHEME_LENGTH          6

/* Default definitions */
#define AFD_CONFIG_FILE            "/AFD_CONFIG"
#define DEFAULT_DIR_CONFIG_FILE    "/DIR_CONFIG"
#define DEFAULT_HOST_CONFIG_FILE   "/HOST_CONFIG"
#define RENAME_RULE_FILE           "/rename.rule"
#define AFD_USER_FILE              "/afd.users"
#define DEFAULT_FIFO_SIZE          4096
#define DEFAULT_BUFFER_SIZE        1024
#define DEFAULT_MAX_ERRORS         10
#define DEFAULT_SUCCESSFUL_RETRIES 10
#define DEFAULT_FILE_SIZE_OFFSET   -1
#define DEFAULT_TRANSFER_TIMEOUT   120L
#define DEFAULT_NO_OF_NO_BURSTS    0

/* Definitions to be read from the AFD_CONFIG file. */
#define DEFAULT_PRINTER_CMD_DEF    "DEFAULT_PRINTER_CMD"
#define DEFAULT_PRINTER_NAME_DEF   "DEFAULT_PRINTER_NAME"
#define MAX_CONNECTIONS_DEF        "MAX_CONNECTIONS"
#define MAX_COPIED_FILES_DEF       "MAX_COPIED_FILES"
#define MAX_COPIED_FILE_SIZE_DEF   "MAX_COPIED_FILE_SIZE"
#define ONE_DIR_COPY_TIMEOUT_DEF   "ONE_DIR_COPY_TIMEOUT"
#ifndef _WITH_PTHREAD
#define DIR_CHECK_TIMEOUT_DEF      "DIR_CHECK_TIMEOUT"
#endif
#define TRUSTED_REMOTE_IP_DEF      "TRUSTED_REMOTE_IP"
#define PING_CMD_DEF               "PING_CMD"
#define TRACEROUTE_CMD_DEF         "TRACEROUTE_CMD"

/* Heading identifiers for the AMG database file */
#define DIR_IDENTIFIER             "[directory]"
#define FILE_IDENTIFIER            "[files]"
#define DESTINATION_IDENTIFIER     "[destination]"
#define RECIPIENT_IDENTIFIER       "[recipient]"
#define OPTION_IDENTIFIER          "[options]"

/* Definitions of maximum values */
#define MAX_SHUTDOWN_TIME          60    /* When the AMG gets the order  */
                                         /* shutdown, this the time it   */
                                         /* waits for its children to    */
                                         /* return before they get       */
                                         /* eliminated.                  */
#define MAX_REAL_HOSTNAME_LENGTH   40    /* How long the real host name  */
                                         /* or its IP number may be.     */
#define MAX_PROXY_NAME_LENGTH      25    /* The maximum length of the    */
                                         /* remote proxy name.           */
#define MAX_MSG_NAME_LENGTH        30    /* Maximum length of message    */
                                         /* name.                        */
#define MAX_INT_LENGTH             11    /* When storing integer values  */
                                         /* as string this is the no.    */
                                         /* characters needed to store   */
                                         /* the largest integer value.   */
#define MAX_TOGGLE_STR_LENGTH      5
#define MAX_USER_NAME_LENGTH       80    /* Maximum length of the user   */
                                         /* name and password.           */
#define MAX_COPIED_FILES           100   /* The maximum number of files  */
                                         /* that the AMG may collect     */
                                         /* before it creates a message  */
                                         /* for the FD.                  */
#define MAX_COPIED_FILE_SIZE       100   /* Same as above only that this */
                                         /* limits the total size copied */
                                         /* in Megabytes.                */
    
/* Miscellaneous definitions */
#define LOG_SIGN_POSITION          13    /* Position in log file where   */
                                         /* type of log entry can be     */
                                         /* determined (I, W, E, F, D).  */
#define LOG_FIFO_SIZE              5     /* The number of letters        */
                                         /* defining the type of log.    */
                                         /* These are displayed in the   */
                                         /* button line.                 */
#define ARCHIVE_UNIT               86400 /* Seconds => 1 day             */
#define UNIQUE_NAME_LENGTH         30    /* <priority>_<date>_<unique number>_<job_id> */
#define WD_ENV_NAME                "AFD_WORK_DIR"  /* The working dir-   */
                                                   /* ectory environment */

/* Different host status */
#define STOP_TRANSFER_STAT         1
#define PAUSE_QUEUE_STAT           2
#define AUTO_PAUSE_QUEUE_STAT      4
#define DANGER_PAUSE_QUEUE_STAT    8
#define AUTO_PAUSE_QUEUE_LOCK_STAT 16

/* Position of each colour in global array */
#define DEFAULT_BG                 0  /* Background                  */
#define WHITE                      1
#define DISCONNECT                 1  /* Successful completion of    */
                                      /* operation and disconnected. */
#define NOT_WORKING                2
#define NOT_WORKING2               3
#define ERROR_ID                   3
#define DISCONNECTED               3  /* AFD_MON not connected.      */
#define STOP_TRANSFER              4  /* Transfer to this host is    */
                                      /* stopped.                    */
#define WARNING_ID                 4
#define CONFIG_ID                  5
#define TRANSFER_ACTIVE            5  /* Creating remote lockfile    */
                                      /* and transferring files.     */
#define PAUSE_QUEUE                6
#define NORMAL_STATUS              7
#define INFO_ID                    7
#define CONNECTING                 8  /* Open connection to remote   */
                                      /* host, sending user and pass-*/
                                      /* word, setting transfer type */
                                      /* and changing directory.     */
#define BLACK                      9
#define FG                         9  /* Foreground                  */
#define FAULTY_ID                  9
#define LOCKED_INVERSE             10
#define TR_BAR                     11 /* Colour for transfer rate bar*/
#define DEBUG_MODE                 11
#ifdef _WITH_WMO_SUPPORT
#define WMO_ACTIVE                 11
#endif
#define LABEL_BG                   12 /* Background for label        */
#ifdef _WITH_MAP_SUPPORT
#define MAP_ACTIVE                 12
#endif
#define FAULTY_TRANSFERS           13 /* When logging in failed or   */
                                      /* we cannot access the        */
                                      /* directory where we have to  */
                                      /* store the files.            */
#define EMAIL_ACTIVE               14
#define CHAR_BACKGROUND            15 /* Background color for characters */
#define FTP_BURST_TRANSFER_ACTIVE  16
#ifdef _WITH_WMO_SUPPORT
#define WMO_BURST_TRANSFER_ACTIVE  17
#define AUTO_PAUSE_QUEUE           18
#define COLOR_POOL_SIZE            19
#else
#define AUTO_PAUSE_QUEUE           17
#define COLOR_POOL_SIZE            18
#endif


/* Directory definitions */
#define AFD_MSG_DIR                "/messages"
#define AFD_FILE_DIR               "/files"
#define AFD_TMP_DIR                "/pool"
#define AFD_TIME_DIR               "/time"
#define AFD_ARCHIVE_DIR            "/archive"
#define FIFO_DIR                   "/fifodir"
#define LOG_DIR                    "/log"
#define ETC_DIR                    "/etc"
#define ERROR_DIR                  "/error"     /* Here are all messages/ */
                                                /* files of jobs that     */
                                                /* could not be           */
                                                /* transfered.            */

#define FSA_ID_FILE                "/fsa.id"
#define STATUS_SHMID_FILE          "/afd.status"
#define BLOCK_FILE                 "/NO_AUTO_RESTART"
#define COUNTER_FILE               "/amg_counter"

/* Definitions for the AFD name */
#define AFD_NAME                   "afd.name"
#define MAX_AFD_NAME_LENGTH         30

/* Definition of file that holds status of the FSA */
#define FSA_STAT_FILE              "/fsa_status"

/* Definitions of fifo names */
#define SYSTEM_LOG_FIFO            "/system_log.fifo"
#define TRANSFER_LOG_FIFO          "/transfer_log.fifo"
#define TRANS_DEBUG_LOG_FIFO       "/trans_db_log.fifo"
#define MON_SYS_LOG_FIFO           "/mon_sys_log.fifo"
#define MON_LOG_FIFO               "/monitor_log.fifo"
#define AFD_CMD_FIFO               "/afd_cmd.fifo"
#define AFD_RESP_FIFO              "/afd_resp.fifo"
#define AMG_CMD_FIFO               "/amg_cmd.fifo"
#define DB_UPDATE_FIFO             "/db_update.fifo"
#define FD_CMD_FIFO                "/fd_cmd.fifo"
#define FD_RESP_FIFO               "/fd_resp.fifo"
#define AW_CMD_FIFO                "/aw_cmd.fifo"
#define IP_FIN_FIFO                "/ip_fin.fifo"
#define SF_FIN_FIFO                "/sf_fin.fifo"
#define RETRY_FD_FIFO              "/retry_fd.fifo"
#define DELETE_JOBS_FIFO           "/delete_jobs.fifo"
#define DELETE_JOBS_HOST_FIFO      "/delete_jobs_host.fifo"
#define FD_WAKE_UP_FIFO            "/fd_wake_up.fifo"
#define PROBE_ONLY_FIFO            "/probe_only.fifo"
#ifdef _INPUT_LOG
#define INPUT_LOG_FIFO             "/input_log.fifo"
#endif
#ifdef _OUTPUT_LOG
#define OUTPUT_LOG_FIFO            "/output_log.fifo"
#endif
#ifdef _DELETE_LOG
#define DELETE_LOG_FIFO            "/delete_log.fifo"
#endif
#define RETRY_MON_FIFO             "/retry_mon.fifo."
#define DEL_TIME_JOB_FIFO          "/del_time_job.fifo"

#define DIR_NAME_FILE              "/directory_names"
#define JOB_ID_DATA_FILE           "/job_id_data"
#define MSG_FIFO                   "/msg.fifo"
#define CURRENT_MSG_LIST_FILE      "/current_job_id_list"
#define TIME_JOB_FILE              "/time_jobs"

#define MSG_CACHE_BUF_SIZE         10000

/* Definitions for the different actions over fifos */
#define HALT                       0
#define STOP                       1
#define START                      2
#define SAVE_STOP                  3
#define QUICK_STOP                 4
#define ACKN                       5
#define NEW_DATA                   6    /* Used by AMG-DB-Editor */
#define START_AMG                  7
#define START_FD                   8
#define STOP_AMG                   9
#define STOP_FD                    10
#define AMG_READY                  11
#define FD_READY                   12
#define PROC_TERM                  13
#define DEBUG                      14
#define RETRY                      15
#define QUEUE                      16
#define TRANSFER                   17
#define IS_ALIVE                   18
#define SHUTDOWN                   19
#define FSA_UPDATED                20
#define CHECK_FILE_DIR             21 /* Check for jobs without message. */

/* In process AFD we have various stop flags */
#define STARTUP_ID                 -1
#define NONE_ID                    0
#define ALL_ID                     1
#define AMG_ID                     2
#define FD_ID                      3

#ifdef _BURST_MODE
#define NO_ID                      -1
#endif

/* The following definitions are used for the function */
/* write_fsa(), so it knows where to write the info.   */
#define ERROR_COUNTER              1
#define TOTAL_FILE_SIZE            3
#define TRANSFER_RATE              9
#define NO_OF_FILES                11
#define CONNECT_STATUS             20

#define NO_BURST_COUNT_MASK        0x1f /* The mask to read the number   */
                                        /* of transfers that may not     */
                                        /* burst.                        */

#define AFDD_SHUTDOWN_MESSAGE      "500 AFDD shutdown."

/*-----------------------------------------------------------------------*
 * Word offset for memory mapped structures of the AFD. Best is to leave
 * this value as it is. If you do change it you must remove all existing
 * memory mapped files from the fifo_dir, before starting the AFD with the
 * new value.
 * For the FSA these bytes are used to store information about the hole
 * AFD with the following meaning:
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of hosts served by the AFD.
 *           |               | If this FSA in no longer in use there
 *           |               | will be -1 here.
 *    -------+---------------+---------------------------------------
 *     5     | unsigned char | Counter that is increased each time
 *           |               | there was a change in the HOST_CONFIG.
 *    -------+---------------+---------------------------------------
 *     6 - 8 | unsigned char | Not used.
 *-----------------------------------------------------------------------*/
#define AFD_WORD_OFFSET 8

/* Structure that holds status of the file transfer for each host */
struct status
       {
          pid_t         proc_id;                /* Process ID of trans-  */
                                                /* fering job.           */
#ifdef _BURST_MODE
          char          error_file;
          char          unique_name[UNIQUE_NAME_LENGTH];
          unsigned char burst_counter;          /* Without this counter  */
                                                /* sf_ftp() would not    */
                                                /* know that there are   */
                                                /* additional jobs. At   */
                                                /* the same time we can  */
                                                /* make certain that not */
                                                /* to many jobs get      */
                                                /* queued for bursting.  */
          unsigned int  job_id;                 /* Since each host can   */
                                                /* have different type   */
                                                /* of jobs (other user,  */
                                                /* different directory,  */
                                                /* other options, etc),  */
                                                /* each of these is      */
                                                /* identified by this    */
                                                /* number.               */
#endif /* _BURST_MODE */
          char          connect_status;         /* The status of what    */
                                                /* sf_xxx() is doing.    */
          int           no_of_files;            /* Total number of all   */
                                                /* files when job        */
                                                /* started.              */
          int           no_of_files_done;       /* Number of files done  */
                                                /* since the job has been*/
                                                /* started.              */
          unsigned long file_size;              /* Total size of all     */
                                                /* files when we started */
                                                /* this job.             */
          unsigned long file_size_done;         /* The total number of   */
                                                /* bytes we have send so */
                                                /* far.                  */
          unsigned long bytes_send;             /* Overall number of     */
                                                /* bytes send so far for */
                                                /* this job.             */
          char          file_name_in_use[MAX_FILENAME_LENGTH];
                                                /* The name of the file  */
                                                /* that is in transfer.  */
          unsigned long file_size_in_use;       /* Total size of current */
                                                /* file.                 */
          unsigned long file_size_in_use_done;  /* Number of bytes send  */
                                                /* for current file.     */
       };
  
struct filetransfer_status
       {
          char           host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char           real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                            /* This is the real hostname */
                                            /* where the data should be  */
                                            /* send to.                  */
          char           host_dsp_name[MAX_HOSTNAME_LENGTH + 1];
                                            /* This is the hostname that */
                                            /* is being displayed by     */
                                            /* afd_ctrl. It's the same   */
                                            /* as stored in host_alias   */
                                            /* plus the toggling         */
                                            /* character.                */
          char           proxy_name[MAX_PROXY_NAME_LENGTH];
          char           host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char           toggle_pos;        /* The position of the       */
                                            /* toggling character in the */
                                            /* host name.                */
          char           original_toggle_pos;/* The original position    */
                                            /* before it was toggled     */
                                            /* automatically.            */
          char           auto_toggle;       /* When ON and an error      */
                                            /* occurs it switches auto-  */
                                            /* matically to the other    */
                                            /* host.                     */
          signed char    file_size_offset;  /* When doing an ls on the   */
                                            /* remote site, this is the  */
                                            /* position where to find    */
                                            /* the size of the file. If  */
                                            /* it is less than 0, it     */
                                            /* means that we do not want */
                                            /* to append files that have */
					    /* been partly send.         */
          int            successful_retries;/* Number of current         */
                                            /* successful retries.       */
          int            max_successful_retries; /* Retries before       */
                                            /* switching hosts.          */
          unsigned char  special_flag;      /* Special flag with the     */
                                            /* following meaning:        */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*| 8    | Error job under  |*/
                                            /*|      | process.         |*/
                                            /*| 7    | Host is in       |*/
                                            /*|      | DIR_CONFIG file. |*/
                                            /*| 6    | Not used.        |*/
                                            /*| 1 - 5| Number of jobs   |*/
                                            /*|      | that may NOT     |*/
                                            /*|      | burst.           |*/
                                            /*+------+------------------+*/
          unsigned char  protocol;          /* Transfer protocol that    */
                                            /* is being used.            */
          char           debug;             /* When this flag is set all */
                                            /* transfer information is   */
                                            /* logged.                   */
          char           host_toggle;       /* If their is more then one */
                                            /* host you can toggle       */
                                            /* between these two         */
                                            /* addresses by toggling     */
                                            /* this switch.              */
          int            host_status;       /* What is the status for    */
                                            /* this host?                */
          int            error_counter;     /* Errors that have so far   */
                                            /* occurred. With the next   */
                                            /* successful transfer this  */
                                            /* will be set to 0.         */
          unsigned int   total_errors;      /* No. of errors so far.     */
          int            max_errors;        /* The maximum errors that   */
                                            /* may occur before we ring  */
                                            /* the alarm bells ;-).      */
          int            retry_interval;    /* After an error has        */
                                            /* occurred, when should we  */
                                            /* retry?                    */
          int            block_size;        /* Block size at which the   */
                                            /* files get transfered.     */
          time_t         last_retry_time;   /* When was the last time we */
                                            /* tried to send a file for  */
                                            /* this host?                */
          time_t         last_connection;   /* Time of last connection.  */
          int            total_file_counter;/* The overall number of     */
                                            /* files still to be send.   */
          unsigned long  total_file_size;   /* The overall number of     */
                                            /* bytes still to be send.   */
          int            total_connect_time;/******** NOT USED !!! *******/
          unsigned int   file_counter_done; /* No. of files done so far. */
          unsigned long  bytes_send;        /* No. of bytes send so far. */
          unsigned int   connections;       /* No. of connections.       */
          int            active_transfers;  /* No. of jobs transferring  */
                                            /* data.                     */
          int            allowed_transfers; /* Maximum no. of parallel   */
                                            /* transfers for this host.  */
          int            transfer_rate;     /* Overall transfer rate for */
                                            /* this host.                */
          long           transfer_timeout;  /* When to timeout the       */
                                            /* transmitting job.         */
          struct status  job_status[MAX_NO_PARALLEL_JOBS];
       };
      
/* Structure that holds status of all process */
struct afd_status
       {
          signed char   amg;              /* Automatic Message Generator */
          unsigned char amg_jobs;         /* Bitmap to show if jobs of   */
                                          /* the AMG (dir_check(),       */
                                          /* time_job(), ...) are active:*/
                                          /*+------+--------------------+*/
                                          /*|Bit(s)|      Meaning       |*/
                                          /*+------+--------------------+*/
                                          /*| 1    | dir_check() active |*/
                                          /*| 2 - 6| Not used.          |*/
                                          /*| 7    | AMG writting to    |*/
                                          /*|      | JID structure.     |*/
                                          /*| 8    | FD searching dirs. |*/
                                          /*+------+--------------------+*/
          signed char   fd;               /* File Distributor            */
          signed char   sys_log;          /* System Log                  */
          signed char   trans_log;        /* Transfer Log                */
          signed char   trans_db_log;     /* Transfer Debug Log          */
          signed char   archive_watch;
          signed char   afd_stat;         /* Statistic program           */
          signed char   afdd;
#ifdef _NO_MMAP
          signed char   mapper;
#endif
#ifdef _INPUT_LOG
          signed char   input_log;
#endif
#ifdef _OUTPUT_LOG
          signed char   output_log;
#endif
#ifdef _DELETE_LOG
          signed char   delete_log;
#endif
          unsigned int  sys_log_ec;         /* System log entry counter. */
          char          sys_log_fifo[LOG_FIFO_SIZE + 1];
          unsigned int  trans_log_ec;       /* Transfer log entry        */
                                            /* counter.                  */
          char          trans_log_fifo[LOG_FIFO_SIZE + 1];
          int           no_of_transfers;    /* The number of active      */
                                            /* transfers system wide.    */
          nlink_t       jobs_in_queue;      /* The number of jobs still  */
                                            /* to be done by the FD.     */
          time_t        start_time;         /* Time when AFD was started.*/
                                            /* This value is used for    */
                                            /* eval_database() so it     */
                                            /* when it has started the   */
                                            /* first time.               */
       };

/* Structure that holds all relevant information of */
/* all process that have been started by the AFD.   */
struct proc_table
       {
          pid_t       pid;
          signed char *status;
          char        proc_name[MAX_PROCNAME_LENGTH];
       };

/* Definitions for renaming */
#define READ_RULES_INTERVAL        20          /* in seconds             */
#define MAX_RULE_HEADER_LENGTH     50
struct rule
       {
          int  no_of_rules;
          char header[MAX_RULE_HEADER_LENGTH + 1];
          char **filter;
          char **rename_to;
       };

/* Definition for structure that holds all data for one job ID */
struct job_id_data
       {
          int  job_id;
          int  dir_id_pos;
          char priority;
          int  no_of_files;
#ifdef _WITH_DYNAMIC_NO_OF_FILTERS
          char **file_list;
#else
          char file_list[MAX_NO_FILES][MAX_FILENAME_LENGTH];
#endif
          int  no_of_loptions;
          char loptions[MAX_OPTION_LENGTH];
          int  no_of_soptions;
          char soptions[MAX_OPTION_LENGTH];
          char recipient[MAX_RECIPIENT_LENGTH];
          char host_alias[MAX_HOSTNAME_LENGTH + 1];
       };
struct dir_name_buf
       {
          char dir_name[MAX_PATH_LENGTH];
          int  dir_id;
       };

struct delete_log
       {
          int           fd;
          unsigned int  *job_number;
          char          *data;
          char          *file_name;
          unsigned char *file_name_length;
          off_t         *file_size;
          char          *host_name;
          size_t        size;
       };

/* Runtime array */
#define RT_ARRAY(name, rows, columns, type)                                 \
        {                                                                   \
           int row_counter;                                                 \
                                                                            \
           if ((name = (type **)calloc((rows), sizeof(type *))) == NULL)    \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN,                             \
                        "calloc() error : %s (%s %d)\n",                    \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           if ((name[0] = (type *)calloc(((rows) * (columns)),              \
                                         sizeof(type))) == NULL)            \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN,                             \
                        "calloc() error : %s (%s %d)\n",                    \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           for (row_counter = 1; row_counter < (rows); row_counter++)       \
              name[row_counter] = (name[0] + (row_counter * (columns)));    \
        }
#define FREE_RT_ARRAY(name)     \
        {                       \
           free(name[0]);       \
           free(name);          \
        }
#define REALLOC_RT_ARRAY(name, rows, columns, type)                         \
        {                                                                   \
           int  row_counter;                                                \
           char *ptr = name[0];                                             \
                                                                            \
           if ((name = (type **)realloc((name), (rows) * sizeof(type *))) == NULL) \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN,                             \
                        "realloc() error : %s (%s %d)\n",                   \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           if ((name[0] = (type *)realloc(ptr,                              \
                           (((rows) * (columns)) * sizeof(type)))) == NULL) \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN,                             \
                        "realloc() error : %s (%s %d)\n",                   \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
                                                                            \
           for (row_counter = 1; row_counter < (rows); row_counter++)       \
              name[row_counter] = (name[0] + (row_counter * (columns)));    \
        }

/* Runtime pointer array */
#define RT_P_ARRAY(name, rows, columns, type)                               \
        {                                                                   \
           register int i;                                                  \
                                                                            \
           if ((name = (type ***)malloc(rows * sizeof(type **))) == NULL)   \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n", \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
           if ((name[0] = (type **)malloc(rows * columns * sizeof(type *))) == NULL)  \
           {                                                                \
              (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n", \
                        strerror(errno), __FILE__, __LINE__);               \
              exit(INCORRECT);                                              \
           }                                                                \
           for (i = 0; i < rows; i++)                                       \
           {                                                                \
              name[i] = name[0] + (i * columns);                            \
           }                                                                \
        }
#define FREE_RT_P_ARRAY(name)   \
        {                       \
           free(name);          \
           free(name[0]);       \
        }

/* Macro that positions pointer just after binary zero. */
#define NEXT(ptr)               \
        {                       \
           while (*ptr != '\0') \
           {                    \
              ptr++;            \
           }                    \
           ptr++;               \
        }

/* Function prototypes */
extern void    change_name(char *, char *, char *, char *),
               get_user(char *),
               get_rename_rules(char *),
               init_fifos_afd(void),
#ifdef _NO_MMAP
               log_shmid(int, int),
#endif
               my_usleep(unsigned long),
               rlock_region(int, off_t);
extern char    *get_definition(char *, char *, char *, int),
               *lock_proc(int, int),
               *posi(char *, char *);
extern int     assemble(char *, char *, int, char *, int, int *, off_t *),
               attach_afd_status(void),
               afw2wmo(char *, int *, char **, char *),
               bin_file_chopper(char *, int *, off_t *),
               bittest(unsigned char *, int),
               check_afd(long),
#ifdef _BURST_MODE
               check_burst(char, int, unsigned int, char *, char *, int),
#endif
               check_dir(char *, int),
               check_fsa(void),
               check_lock(char *, char),
               check_msa(void),
               check_msg_name(char *),
               coe_open(char *, int, ...),
               copy_file(char *, char *),
               create_message(int, char *, char *),
               create_name(char *, signed char, time_t, int, unsigned short *, char *),
               extract(char *, char *, int, int *, off_t *),
               filter(char *, char *),
               fprint_dup_msg(FILE *, int, char *, char *, time_t),
               fsa_attach(void),
               fsa_detach(void),
               get_afd_name(char *),
               get_afd_path(int *, char **, char *),
               get_mon_path(int *, char **, char *),
               get_permissions(char **),
               get_position(struct filetransfer_status *, char *, int),
               get_rule(char *, int),
               lock_file(char *, int),
               lock_region(int, off_t),
#ifdef _FIFO_DEBUG
               logger(FILE *, int, char *, int),
#else
               logger(FILE *, int, int),
#endif
               make_fifo(char *),
               move_file(char *, char *),
               msa_attach(void),
               msa_detach(void),
               next_counter(int *),
               normal_copy(char *, char *),
               open_counter_file(char *),
               read_file(char *, char **buffer),
               rec(int, char *, char *, ...),
               rec_rmdir(char *),
               send_cmd(char, int);
extern off_t   gts2tiff(char *, char *),
               tiff2gts(char *, char *);
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG)
extern pid_t   start_log(char *);
#endif
extern ssize_t readn(int, void *, int);
extern void    *attach_buf(char *, int *, size_t, char *),
               bitset(unsigned char *, int),
               lock_region_w(int, off_t),
               change_alias_order(char **, int),
               clr_fl(int, int),
               daemon_init(char *),
               delete_log_ptrs(struct delete_log *),
               get_log_number(int *, int, char *, int),
               *map_file(char *, int *),
               *mmap_resize(int, void *, size_t),
               reshuffel_log_files(int, char *, char *),
               t_hostname(char *, char *),
               set_fl(int, int),
               shutdown_afd(void),
               unlock_region(int, off_t);
#ifdef _FIFO_DEBUG
extern void    show_fifo_data(char, char *, char *, int, char *, int);
#endif
extern caddr_t mmap_emu(caddr_t, size_t, int, int, char *, off_t);
extern int     msync_emu(char *),
               munmap_emu(char *);

#endif /* __afddefs_h */
