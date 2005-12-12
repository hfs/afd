/*
 *  amgdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __amgdefs_h
#define __amgdefs_h

#include <stdio.h>
#include <sys/types.h>
#include <sys/times.h>

/* Definitions to be read from the AFD_CONFIG file. */
#define AMG_DIR_RESCAN_TIME_DEF    "AMG_DIR_RESCAN_TIME"
#define MAX_NO_OF_DIR_CHECKS_DEF   "MAX_NO_OF_DIR_CHECKS"
#define MAX_PROCESS_PER_DIR_DEF    "MAX_PROCESS_PER_DIR"
#define CREATE_SOURCE_DIR_MODE_DEF "CREATE_SOURCE_DIR_MODE"

/* Definitions of default values */
#define DEFAULT_PRIORITY           '9'  /* When priority is not          */
                                        /* specified, assume this value. */

/* Definitions of maximum values */
#define MAX_HOLDING_TIME           24   /* The maximum time that a file  */
                                        /* can be held before it gets    */
                                        /* deleted.                      */
#define MAX_DIR_OPTION_LENGTH      20   /* The size of the buffer where  */
                                        /* the time in hours, what is to */
                                        /* be done and if it should be   */
                                        /* reported if an unknown file   */
                                        /* appears in the source         */
                                        /* directory.                    */
#define MAX_GROUP_NAME_LENGTH      256  /* The maximum group name length */
                                        /* for any group.                */
#define MAX_DETACH_TIME            1    /* The maximum time that eval_-  */
                                        /* database will wait for all    */
                                        /* jobs to detach from the FSA.  */
#define MAX_FILES_TO_PROCESS       3    /* The maximum number of files   */
                                        /* that the AMG may copy in one  */
                                        /* go if extract option is set.  */
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG)
#define MAX_NO_OF_DB_PER_FILE      200  /* The maximum number of         */
                                        /* database that can be stored   */
                                        /* in a database history file.   */
#endif
#define DG_BUFFER_STEP_SIZE        2    /* The steps in which the buffer */
                                        /* is increased for the          */
                                        /* destination group.            */
#define FG_BUFFER_STEP_SIZE        4    /* The steps in which the buffer */
                                        /* is increased for the          */
                                        /* file group . May NOT be less  */
                                        /* then 2!                       */
#define DIR_NAME_BUF_SIZE          20   /* Buffers allocated for the     */
                                        /* dir_name_buf structure.       */
#define JOB_ID_DATA_STEP_SIZE      50   /* Buffers allocated for the     */
                                        /* job_id_data structure.        */
#define TIME_JOB_STEP_SIZE         1

/* Definitions of identifiers in options */
#define TIME_NO_COLLECT_ID         "time no collect"
#define TIME_NO_COLLECT_ID_LENGTH  (sizeof(TIME_NO_COLLECT_ID) - 1)
/* NOTE: TIME_ID has already been defined in afddefs.h for [dir options]. */
#define PRIORITY_ID                "priority"
#define PRIORITY_ID_LENGTH         (sizeof(PRIORITY_ID) - 1)
#define RENAME_ID                  "rename"
#define RENAME_ID_LENGTH           (sizeof(RENAME_ID) - 1)
#define EXEC_ID                    "exec"
#define EXEC_ID_LENGTH             (sizeof(EXEC_ID) - 1)
#define BASENAME_ID                "basename" /* If we want to send only */
                                              /* the basename of the     */
                                              /* file.                   */
#define BASENAME_ID_LENGTH         (sizeof(BASENAME_ID) - 1)
#define EXTENSION_ID               "extension"/* If we want to send      */
                                              /* files without extension.*/
#define EXTENSION_ID_LENGTH        (sizeof(EXTENSION_ID) - 1)
#define ADD_PREFIX_ID              "prefix add"
#define ADD_PREFIX_ID_LENGTH       (sizeof(ADD_PREFIX_ID) - 1)
#define DEL_PREFIX_ID              "prefix del"
#define DEL_PREFIX_ID_LENGTH       (sizeof(DEL_PREFIX_ID) - 1)
#define TOUPPER_ID                 "toupper"
#define TOUPPER_ID_LENGTH          (sizeof(TOUPPER_ID) - 1)
#define TOLOWER_ID                 "tolower"
#define TOLOWER_ID_LENGTH          (sizeof(TOLOWER_ID) - 1)
#define TIFF2GTS_ID                "tiff2gts"
#define TIFF2GTS_ID_LENGTH         (sizeof(TIFF2GTS_ID) - 1)
#define GTS2TIFF_ID                "gts2tiff"
#define GTS2TIFF_ID_LENGTH         (sizeof(GTS2TIFF_ID) - 1)
#define GRIB2WMO_ID                "grib2wmo"
#define GRIB2WMO_ID_LENGTH         (sizeof(GRIB2WMO_ID) - 1)
#define EXTRACT_ID                 "extract"
#define EXTRACT_ID_LENGTH          (sizeof(EXTRACT_ID) - 1)
#define ASSEMBLE_ID                "assemble"
#define ASSEMBLE_ID_LENGTH         (sizeof(ASSEMBLE_ID) - 1)
#define WMO2ASCII_ID               "wmo2ascii"
#define WMO2ASCII_ID_LENGTH        (sizeof(WMO2ASCII_ID) - 1)
#define DELETE_ID                  "delete"
#define DELETE_ID_LENGTH           (sizeof(DELETE_ID) - 1)
#define CONVERT_ID                 "convert"
#define CONVERT_ID_LENGTH          (sizeof(CONVERT_ID) - 1)
#ifdef _WITH_AFW2WMO
#define AFW2WMO_ID                 "afw2wmo"
#define AFW2WMO_ID_LENGTH          (sizeof(AFW2WMO_ID) - 1)
#define LOCAL_OPTION_POOL_SIZE     19
#else
#define LOCAL_OPTION_POOL_SIZE     18
#endif

/* Definitions for types of time options. */
#define NO_TIME                    0     /* No time options at all.      */
#define SEND_COLLECT_TIME          1     /* Only send during this time.  */
                                         /* Data outside this time will  */
                                         /* be stored and send when the  */
                                         /* next send time slot arrives. */
#define SEND_NO_COLLECT_TIME       2     /* Only send data during this   */
                                         /* time. Data outside this time */
                                         /* will NOT be stored and will  */
                                         /* be deleted!                  */

/* Definitions for assembling, converting and extracting WMO bulletins/files */
#define TWO_BYTE                   1
#define FOUR_BYTE_LBF              2
#define FOUR_BYTE_HBF              3
#define FOUR_BYTE_MSS              4
#define FOUR_BYTE_MRZ              5
#define FOUR_BYTE_GRIB             6
#define WMO_STANDARD               7
#define ASCII_STANDARD             8
#define FOUR_BYTE_DWD              9
#define SOHETX2WMO0                10
#define SOHETX2WMO1                11
#define SOHETX                     12
#define ONLY_WMO                   13
#define SOHETXWMO                  14

/* Definition of fifos for the AMG to communicate */
/* with the above jobs.                           */
#define DC_CMD_FIFO                "/dc_cmd.fifo"
#define DC_RESP_FIFO               "/dc_resp.fifo"

/* Definitions of the process names that are started */
/* by the AMG main process.                          */
#define DC_PROC_NAME               "dir_check"

/* Return values for function eval_dir_config() */
#define NO_VALID_ENTRIES           -2

/* Miscellaneous definitions */
#define PTR_BUF_SIZE               10         /* The initial size of the */
                                              /* pointer array and the   */
                                              /* step size by which it   */
                                              /* can increase. This      */
                                              /* can be large since      */
                                              /* pointers don't need a   */
                                              /* lot of space.           */
#define OPTION_BUF_SIZE            10         /* When allocating memory  */
                                              /* for the option buffer,  */
                                              /* this is the step size   */
                                              /* by which the memory will*/
                                              /* increase when memory is */
                                              /* used up.                */
                                              /* (* MAX_OPTION_LENGTH)   */
#define JOB_TIMEOUT                30         /* This is how long AMG    */
                                              /* waits (in seconds) for  */
                                              /* a reply from a job      */
                                              /* (ITT, OOT or IT) before */
                                              /* it receives a timeout   */
                                              /* error.                  */
                                              /* NOTE: Must be larger    */
                                              /*       than 5!           */

#define MESSAGE_BUF_STEP_SIZE      500

#define OLD_FILE_SEARCH_INTERVAL   3600       /* Search for old files    */
                                              /* hour.                   */
#define NO_OF_HOST_DB_PARAMETERS   8

#ifdef _WITH_PTHREAD
/* Structure that holds data to be send passed to a function. */
struct data_t
       {
          int    i;
          char   **file_name_pool;
          off_t  *file_size_pool;
          time_t *file_mtime_pool;
          char   afd_file_dir[MAX_PATH_LENGTH];
       };
#endif /* _WITH_PTHREAD */

/* Structure holding pointers to all relevant information */
/* in the global shared memory regions.                   */
struct p_array
       {
          long   ptr[11];          /* Pointer offdet to the following    */
                                   /* information:                       */
                                   /*    0  - priority      char x       */
                                   /*    1  - directory     char x[]\0   */
                                   /*    2  - alias name    char x[]\0   */
                                   /*    3  - no. of files  char x[]\0   */
                                   /*    4  - file          char x[]\0   */
                                   /*    5  - no. loc. opt. char x[]\0   */
                                   /*    6  - loc. options  char x[][]\0 */
                                   /*    7  - no. std. opt. char x[]\0   */
                                   /*    8  - std. options  char x[][]\0 */
                                   /*    9  - recipient     char x[]\0   */
                                   /*   10  - DIR_CONFIG ID char x[]\0   */
       };

/* Structure that holds one directory entry of the AMG database */
struct dest_group
       {
          char  dest_group_name[MAX_GROUP_NAME_LENGTH];
          int   rc;                        /* recipient counter  */
          char  **recipient;
          int   oc;                        /* option counter     */
          char  options[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
       };

struct file_group
       {
          char              file_group_name[MAX_GROUP_NAME_LENGTH];
          int               fbl;            /* file buffer length */
          int               fc;             /* file counter      */
          char              *files;
          int               dgc;            /* destination group counter */
          struct dest_group *dest;
       };

struct dir_group
       {
          char              location[MAX_PATH_LENGTH];
          char              orig_dir_name[MAX_PATH_LENGTH];
                                            /* Directory name as it is   */
                                            /* in DIR_CONFIG.            */
          char              url[MAX_RECIPIENT_LENGTH];
          char              alias[MAX_DIR_ALIAS_LENGTH + 1];
          char              option[MAX_DIR_OPTION_LENGTH + 1];
                                            /* This is the old way of    */
                                            /* specifying options for a  */
                                            /* directory.                */
          char              dir_options[MAX_OPTION_LENGTH];
                                            /* As of version 1.2.x this  */
                                            /* is the new place where    */
                                            /* options are being         */
                                            /* specified.                */
          char              type;           /* Either local or remote.   */
          unsigned int      protocol;
          unsigned int      dir_config_id;
          int               fgc;            /* file group counter        */
          struct file_group *file;
       };

struct dir_data
       {
          char          dir_name[MAX_PATH_LENGTH];
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
                                            /* Here the alias hostname is*/
                                            /* stored. When a secondary  */
                                            /* host can be specified,    */
                                            /* only that part is stored  */
                                            /* up to the position of the */
                                            /* toggling character eg:    */
                                            /* mrz_mfa + mrz_mfb =>      */
                                            /*       mrz_mf              */
          char          url[MAX_RECIPIENT_LENGTH];
          char          wait_for_filename[MAX_WAIT_FOR_LENGTH]; /* Wait  */
                                            /* for the given file name|  */
                                            /* pattern before we take    */
                                            /* files from this directory.*/
          struct bd_time_entry te;
          unsigned char remove;             /* Should the files be       */
                                            /* removed when they are     */
                                            /* being retrieved.          */
          unsigned char stupid_mode;        /* If set it will NOT        */
                                            /* collect information about */
                                            /* files that where found in */
                                            /* directory. So that when   */
                                            /* remove is not set we will */
                                            /* not always collect the    */
                                            /* same files. This ensures  */
                                            /* that files are collected  */
                                            /* only once.                */
          unsigned char delete_files_flag;  /* UNKNOWN_FILES: All unknown*/
                                            /* files will be deleted.    */
                                            /* QUEUED_FILES: Queues will */
                                            /* also be checked for old   */
                                            /* files.                    */
          unsigned char report_unknown_files;
          unsigned char force_reread;
#ifndef _WITH_PTHREAD
          unsigned char important_dir;
#endif
          unsigned char time_option;        /* Flag to indicate if the   */
                                            /* time option is used.      */
          char          priority;           /* Priority for this         */
                                            /* directory.                */
          unsigned int  protocol;           /* Transfer protocol that    */
                                            /* is being used.            */
          int           unknown_file_time;  /* After how many hours can  */
                                            /* a unknown file be deleted.*/
          int           queued_file_time;   /* After how many hours can  */
                                            /* a queued file be deleted. */
          int           locked_file_time;   /* After how many hours can  */
                                            /* a locked file be deleted. */
          int           end_character;
          unsigned int  in_dc_flag;         /* Flag to indicate which of */
                                            /* the options have been     */
                                            /* stored in DIR_CONFIG. This*/
                                            /* is usefull for restoring  */
                                            /* the DIR_CONFIG from       */
                                            /* scratch.                  */
          unsigned int  accumulate;         /* How many files need to    */
                                            /* accumulate before start   */
                                            /* sending from this dir.    */
          unsigned int  ignore_file_time;   /* Ignore files which are    */
                                            /* older, equal or newer     */
                                            /* the given time in sec.    */
          unsigned int  gt_lt_sign;         /* The sign for the following*/
                                            /* variables:                */
                                            /*     ignore_size           */
                                            /*     ignore_file_time      */
                                            /* These are bit masked      */
                                            /* for each variable.        */
                                            /*+---+---------------------+*/
                                            /*|Bit|       Meaning       |*/
                                            /*+---+---------------------+*/
                                            /*| 1 | ISIZE_EQUAL         |*/
                                            /*| 2 | ISIZE_LESS_THEN     |*/
                                            /*| 3 | ISIZE_GREATER_THEN  |*/
                                            /*+---+---------------------+*/
                                            /*| 4 | IFTIME_EQUAL        |*/
                                            /*| 5 | IFTIME_LESS_THEN    |*/
                                            /*| 6 | IFTIME_GREATER_THEN |*/
                                            /*+---+---------------------+*/
                                            /*| * | Rest not used.      |*/
                                            /*+---+---------------------+*/
#ifdef WITH_DUP_CHECK
          unsigned int  dup_check_flag;     /* Flag storing the type of  */
                                            /* check that is to be done  */
                                            /* and what type of CRC to   */
                                            /* use:                      */
                                            /*+------+------------------+*/
                                            /*|Bit(s)|     Meaning      |*/
                                            /*+------+------------------+*/
                                            /*|17-32 | Not used.        |*/
                                            /*|   16 | DC_CRC32         |*/
                                            /*| 4-15 | Not used.        |*/
                                            /*|    3 | DC_FILE_CONT_NAME|*/
                                            /*|    2 | DC_FILE_CONTENT  |*/
                                            /*|    1 | DC_FILENAME_ONLY |*/
                                            /*+------+------------------+*/
#endif
          unsigned int  max_copied_files;   /* Maximum number of files   */
                                            /* that we copy in one go.   */
          unsigned int  dir_id;             /* CRC-32 checksum of this   */
                                            /* directory entry.          */
          int           fsa_pos;            /* FSA position, only used   */
                                            /* for retrieving files.     */
          int           dir_pos;            /* Position of this directory*/
                                            /* in the directory name     */
                                            /* buffer.                   */
          int           max_process;
#ifdef WITH_DUP_CHECK
          time_t        dup_check_timeout;  /* When the stored CRC for   */
                                            /* duplicate checks are no   */
                                            /* longer valid. Value is in */
                                            /* seconds.                  */
#endif
          off_t         max_copied_file_size; /* The maximum number of   */
                                            /* bytes that we copy in one */
                                            /* go.                       */
          off_t         accumulate_size;    /* Ditto, only here we look  */
                                            /* at the size.              */
          off_t         ignore_size;        /* Ignore any files less     */
                                            /* then, equal or greater    */
                                            /* then the given size.      */
       };

/* Definitions for a flag in the structure instant_db (lfs) or */
/* the structure directory_entry (flag).                       */
#define IN_SAME_FILESYSTEM  1
#define ALL_FILES           2
#define RENAME_ONE_JOB_ONLY 4
#define GO_PARALLEL         8
#define SPLIT_FILE_LIST     16
#define DO_NOT_LINK_FILES   32
#define DELETE_ALL_FILES    64

/* Structure that holds a single job for process dir_check */
struct instant_db
       {
          unsigned int  job_id;        /* Since each host can have       */
                                       /* different type of jobs (other  */
                                       /* user, different directory,     */
                                       /* other options, etc), each of   */
                                       /* these is identified by this    */
                                       /* number. This job number is     */
                                       /* always unique even when re-    */
                                       /* reading the DIR_CONFIG file.   */
          unsigned int  dir_config_id; /* To find out from which         */
                                       /* DIR_CONFIG this job comes.     */
          char          str_job_id[MAX_INT_LENGTH];
          char          *dir;          /* Directory that has to be       */
                                       /* monitored.                     */
          unsigned int  dir_id;        /* Contains the directory ID!!!   */
          unsigned char lfs;           /* Flag to indicate if this       */
                                       /* directory belongs to the same  */
                                       /* filesystem as the working      */
                                       /* directory of the AFD, i.e if   */
                                       /* we have to fork or not.        */
          char          time_option_type;/* The type of time option:     */
                                       /* SEND_COLLECT_TIME,             */
                                       /* SEND_NO_COLLECT_TIME and       */
                                       /* NO_TIME.                       */
          time_t        next_start_time;
          struct bd_time_entry te;
          char          priority;      /* Priority of job.               */
          unsigned int  file_mask_id;  /* CRC-32 checksum of file masks. */
          int           no_of_files;   /* Number of files to be send.    */
          int           fbl;           /* File mask buffer length.       */
          char          *files;        /* The name of the file(s) to be  */
                                       /* send.                          */
          int           no_of_loptions;/* Holds number of local          */
                                       /* options.                       */
          char          *loptions;     /* Storage of local options.      */
          int           no_of_soptions;/* Holds number of standard       */
                                       /* options.                       */
          char          *soptions;     /* Storage of standard options.   */
          char          *recipient;    /* Storage of recipients.         */
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
                                       /* Alias of hostname of recipient.*/
          unsigned int  protocol;
          unsigned int  age_limit;
          char          dup_paused_dir;/* Flag to indicate that this     */
                                       /* paused directory is already    */
                                       /* in the list and does not have  */
                                       /* to be checked again by         */
                                       /* check_paused_dir().            */
          char          paused_dir[MAX_PATH_LENGTH];
                                       /* Stores absolute path of paused */
                                       /* or queued hosts.               */
          int           position;      /* Location of this host in the   */
                                       /* FSA.                           */
          int           fra_pos;       /* Location of this job in the    */
                                       /* FRA.                           */
       };

/* Structure that holds all file information for one directory */
struct file_mask_entry
       {
          int  nfm;                          /* Number of file masks     */
          int  dest_count;                   /* Number of destinations   */
                                             /* for this directory.      */
          int  *pos;                         /* This holds the equivalent*/
                                             /* position in the struct   */
                                             /* instant_db.              */
          char **file_mask;
       };
struct directory_entry
       {
          unsigned int           dir_id;     /* When reading a new       */
                                             /* DIR_CONFIG file this     */
                                             /* number is used to back-  */
                                             /* trace where a certain    */
                                             /* file comes from in the   */
                                             /* input log.               */
          int                    nfg;        /* Number of file groups    */
          int                    fra_pos;
          time_t                 mod_time;
          time_t                 search_time;/* Used when a new file     */
                                             /* arrives and we already   */
                                             /* have checked the         */
                                             /* directory and it is      */
                                             /* still the same second.   */
          struct file_mask_entry *fme;
          char                   flag;       /* Flag to if all files in  */
                                             /* this directory are to be */
                                             /* distributed (ALL_FILES)  */
                                             /* or if this directory is  */
                                             /* in the same filesystem   */
                                             /* (IN_SAME_FILESYSTEM).    */
          char                   *alias;     /* Alias name of the        */
                                             /* directory.               */
          char                   *dir;       /* Pointer to directory     */
                                             /* name.                    */
          char                   *paused_dir;/* Holds the full paused    */
                                             /* REMOTE directory.        */
       };

struct message_buf
       {
          char bin_msg_name[MAX_BIN_MSG_LENGTH];
       };

struct dc_proc_list
       {
          pid_t        pid;
          int          fra_pos;
          unsigned int job_id;
       };

struct dir_config_buf
       {
          unsigned int dc_id;
          time_t       dc_old_time;
          char         *dir_config_file;
       };

struct fork_job_data
       {
          unsigned int job_id;
          unsigned int forks;
          clock_t      user_time;
          clock_t      system_time;
       };

/* Function prototypes */
extern int    amg_zombie_check(pid_t *, int),
              com(char),
              convert(char *, char *, int, off_t *),
#ifdef _WITH_PTHREAD
              check_files(struct directory_entry *, char *, char *, char *,
                          int, int *, time_t, off_t *, char **, off_t *),
              count_pool_files(int *, char *, off_t *, time_t *, char **),
              link_files(char *, char *, time_t, off_t *, time_t *, char **,
                         struct directory_entry *, struct instant_db *,
                         time_t *, unsigned int *, int, int, int, char *,
                         off_t *),
              save_files(char *, char *, time_t, unsigned int, off_t *,
                         time_t *, char **, struct directory_entry *,
                         int, int, char, int),
#else
              check_files(struct directory_entry *, char *, char *,
                          char *, int, int *, time_t, off_t *),
              count_pool_files(int *, char *),
              link_files(char *, char *, time_t, struct directory_entry *,
                         struct instant_db *, time_t *, unsigned int *,
                         int, int, int, char *, off_t *),
              save_files(char *, char *, time_t, unsigned int,
                         struct directory_entry *, int, int, char, int),
#endif
              check_process_list(int),
              create_db(void),
              eval_dir_config(off_t),
              eval_time_str(char *, struct bd_time_entry *),
              handle_options(int, char *, unsigned int,
#ifdef _DELETE_LOG
                             char *,
#endif
#ifdef _PRODUCTION_LOG
                             time_t, unsigned short, unsigned int,
#endif
                             char *, int *, off_t *),
              in_time(time_t, struct bd_time_entry *),
              lookup_dir_id(char *, char *),
              lookup_fra_pos(char *),
              rename_files(char *, char *, int, struct instant_db *, time_t *,
                           int, unsigned int *, char *, off_t *);
extern pid_t  make_process_amg(char *, char *, int, int);
extern char   *check_paused_dir(struct directory_entry *, int *, int *, int *),
              *convert_fsa(int, char *, off_t *, int, unsigned char,
                           unsigned char),
              *convert_fra(int, char *, off_t *, int, unsigned char,
                           unsigned char),
              *next(char *);
extern void   check_old_time_jobs(int),
              clear_msg_buffer(void),
              clear_pool_dir(void),
              create_fsa(void),
              create_fra(int),
              create_sa(int),
              enter_time_job(int),
              eval_dir_options(int, char *, char *),
              handle_time_jobs(int *, time_t),
              init_dir_check(int, char **, char *, time_t *, int *,
                             int *, int *),
              init_job_data(void),
              init_msg_buffer(void),
              lookup_dc_id(struct dir_config_buf **, int),
              lookup_file_mask_id(struct instant_db *, int),
              lookup_job_id(struct instant_db *, unsigned int *),
              receive_log(char *, char *, int, time_t, char *, ...),
              remove_pool_directory(char *, unsigned int),
#ifdef _DELETE_LOG
              remove_time_dir(char *, unsigned int, int),
#else
              remove_time_dir(char *, unsigned int),
#endif
              reread_dir_config(int, off_t, time_t *, int, int, size_t,
                                int, int, struct host_list *),
              reread_host_config(time_t *, int *, int *, size_t *,
                                 struct host_list **, int),
              search_old_files(time_t),
              send_message(char *, char *, unsigned int, unsigned short,
                           time_t, int,
#ifdef _WITH_PTHREAD
                           char *,
# ifdef _DELETE_LOG
                           off_t *, char **,
# endif
#endif
                           int, off_t),
              show_shm(FILE *),
              sort_time_job(void),
              store_file_mask(char *, struct dir_group *),
              store_passwd(char *, int);
#endif /* __amgdefs_h */
