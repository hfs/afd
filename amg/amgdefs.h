/*
 *  amgdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __amgdefs_h
#define __amgdefs_h

#include <stdio.h>
#include <sys/types.h>

#define _LIMIT_PROCESS_PER_DIR

/* Definitions to be read from the AFD_CONFIG file. */
#define AMG_DIR_RESCAN_TIME_DEF    "AMG_DIR_RESCAN_TIME"
#define MAX_NO_OF_DIR_CHECKS_DEF   "MAX_NO_OF_DIR_CHECKS"

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
#define MAX_MSG_PER_SEC            9999 /* The maximum number of         */
                                        /* messages that may be          */
                                        /* generated in one second.      */
#define MAX_FIFO_BUFFER            13   /* This is how much we want to   */
                                        /* read or write to a fifo.      */
                                        /* Usually it is just one char-  */
                                        /* acter. But sometimes we also  */
                                        /* wish to send an integer.      */
                                        /* (eg. shm ID)                  */
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
#define FG_BUFFER_STEP_SIZE        4    /* Buffers allocated by default  */
                                        /* for file groups. May NOT be   */
                                        /* less then 2!                  */
#define DIR_NAME_BUF_SIZE          20   /* Buffers allocated for the     */
                                        /* dir_name_buf structure.       */
#define JOB_ID_DATA_STEP_SIZE      50   /* Buffers allocated for the     */
                                        /* job_id_data structure.        */
#define TIME_JOB_STEP_SIZE         1
#define JOB_ID_FILE                "/jid_number"
#define DIR_ID_FILE                "/did_number"
#define INSTANT_DB_FILE            "/instant_db"

/* Definitions of identifiers in options */
#define TIME_ID                    "time"
#define TIME_ID_LENGTH             4
#define PRIORITY_ID                "priority"
#define PRIORITY_ID_LENGTH         8
#define RENAME_ID                  "rename"
#define RENAME_ID_LENGTH           6
#define EXEC_ID                    "exec"
#define EXEC_ID_LENGTH             4
#define BASENAME_ID                "basename" /* If we want to send only */
                                              /* the basename of the     */
                                              /* file.                   */
#define EXTENSION_ID               "extension"/* If we want to send      */
                                              /* files without extension.*/
#define HOLD_ID                    "hold"     /* Hold the job for x min  */
                                              /* before we generate a    */
                                              /* message to allow some   */
                                              /* trailing files to be    */
                                              /* send as well.           */
#define TAR_ID                     "tar"
#define UNTAR_ID                   "untar"
#define COMPRESS_ID                "compress"
#define UNCOMPRESS_ID              "uncompress"
#define GZIP_ID                    "gzip"
#define GUNZIP_ID                  "gunzip"
#define ADD_PREFIX_ID              "prefix add"
#define ADD_PREFIX_ID_LENGTH       10
#define DEL_PREFIX_ID              "prefix del"
#define DEL_PREFIX_ID_LENGTH       10
#define TIFF2GTS_ID                "tiff2gts"
#define GTS2TIFF_ID                "gts2tiff"
#define GTS2TIFF_ID_LENGTH         8
#define EXTRACT_ID                 "extract"
#define EXTRACT_ID_LENGTH          7
#define ASSEMBLE_ID                "assemble"
#define ASSEMBLE_ID_LENGTH         8
#ifdef _WITH_AFW2WMO
#define AFW2WMO_ID                 "afw2wmo"
#define AFW2WMO_ID_LENGTH          7
#define LOCAL_OPTION_POOL_SIZE     19
#else
#define LOCAL_OPTION_POOL_SIZE     18
#endif

/* Definitions for assembling and extracting WMO bulletins */
#define TWO_BYTE                   1
#define FOUR_BYTE_LBF              2
#define FOUR_BYTE_HBF              3
#define FOUR_BYTE_MSS              4
#define FOUR_BYTE_MRZ              5
#define WMO_STANDARD               6
#define ASCII_STANDARD             7

#define NO_ACCESS                  10

/* The corresponding Un*x commands */
#define TAR_COMMAND                "/bin/tar cvf "
#define UNTAR_COMMAND              "/bin/tar xvf "
#define COMPRESS_COMMAND           "/bin/compress -c "
#define UNCOMPRESS_COMMAND         "/bin/uncompress -c "
#define GZIP_COMMAND               "gzip -c "
#define GUNZIP_COMMAND             "gzip -dc "

/* Extensions for gzip and compress */
#define COMPRESS_EXTENSION         ".Z"
#define GZIP_EXTENSION             ".gz"

/* Definition of fifos for the AMG to communicate */
/* with the above jobs.                           */
#define IP_CMD_FIFO                "/ip_cmd.fifo"
#define IP_RESP_FIFO               "/ip_resp.fifo"

/* Definitions of the process names that are started */
/* by the AMG main process.                          */
#define IT_PROC_NAME               "dir_check"
#define PROC_NAME_LENGTH           10

/* Return values for function eval_dir_config() */
#define NO_VALID_ENTRIES           -2

/* Heading identifiers for message */
#define FILE_HEADER                "[file]"
#define TIME_HEADER                "[time]"

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
#define HOST_BUF_SIZE             100         /* The initial size of the */
                                              /* structure holding the   */
                                              /* host list.              */
#define JOB_TIMEOUT                30         /* This is how long AMG    */
                                              /* waits (in seconds) for  */
                                              /* a reply from a job      */
                                              /* (ITT, OOT or IT) before */
                                              /* it receives a timeout   */
                                              /* error.                  */
#define AMG_TIMEOUT                30         /* This is how long a job  */
                                              /* (ITT, OOT or IT) waits  */
                                              /* (in seconds) for a      */
                                              /* reply from AMG before   */
                                              /* the job dies.           */
#define MESSAGE_BUF_FILE           "/tmp_msg_buffer"
#define MESSAGE_BUF_STEP_SIZE      500

#define OLD_FILE_TIME              24         /* Hours.                  */
#define OLD_FILE_SEARCH_INTERVAL   3600       /* Search for old files    */
                                              /* hour.                   */
#define NO_OF_HOST_DB_PARAMETERS   8

#ifdef _WITH_PTHREAD
/* Structure that holds data to be send passed to a function. */
struct data_t
       {
          int   i;
          char  **file_name_pool;
          off_t *file_size_pool;
          char  afd_file_dir[MAX_PATH_LENGTH];
       };
#endif

/* Structure holding pointers to all relevant information */
/* in the global shared memory regions.                   */
struct   p_array
         {
            char  *ptr[9];         /* Pointers pointing to the following */
                                   /* information:                       */
                                   /*    0  - priority      char x       */
                                   /*    1  - directory     char x[]\0   */
                                   /*    2  - no. of files  char x[]\0   */
                                   /*    3  - file          char x[][]\0 */
                                   /*    4  - no. loc. opt. char x[]\0   */
                                   /*    5  - loc. options  char x[][]\0 */
                                   /*    6  - no. std. opt. char x[]\0   */
                                   /*    7  - std. options  char x[][]\0 */
                                   /*    8  - recipient     char x[]\0   */
         };

/* Structure that holds one directory entry of the AMG database */
struct   dest_group
         {
            char  dest_group_name[MAX_GROUP_NAME_LENGTH];
            int   rc;                        /* recipient counter  */
            char  recipient[MAX_NO_RECIPIENT][MAX_RECIPIENT_LENGTH];
            int   oc;                        /* option counter     */
            char  options[MAX_NO_OPTIONS][MAX_OPTION_LENGTH];
         };

/* NOTE: Dynamic number of filters does NOT work since there is no */
/*       solution, on how to do it in the memory mapped file for   */
/*       struct job_id_data.                                       */
struct   file_group
         {
            char              file_group_name[MAX_GROUP_NAME_LENGTH];
            int               fc;             /* file counter      */
#ifdef _WITH_DYNAMIC_NO_OF_FILTERS
            char              **files;
#else
            char              files[MAX_NO_FILES][MAX_FILENAME_LENGTH];
#endif
            int               dgc;            /* destination group counter */
            struct dest_group dest[MAX_NO_DEST_GROUP];
         };

struct   dir_group
         {
            char              location[MAX_PATH_LENGTH];
            char              option[MAX_DIR_OPTION_LENGTH + 1];
            int               fgc;            /* file group counter       */
            struct file_group *file;
         };

/* Structure to hold all possible bits for a time entry */
struct   bd_time_entry
         {
#ifdef _WORKING_LONG_LONG
            unsigned long long continuous_minute;
            unsigned long long minute;
#else
            unsigned char      continuous_minute[8];
            unsigned char      minute[8];
#endif
            unsigned int       hour;
            unsigned int       day_of_month;
            unsigned short     month;
            unsigned char      day_of_week;
         };

/* Structure that holds a single job for process dir_check */
struct   instant_db
         {
            int     job_id;            /* Since each host can have       */
                                       /* different type of jobs (other  */
                                       /* user, different directory,     */
                                       /* other options, etc), each of   */
                                       /* these is identified by this    */
                                       /* number. This job number is     */
                                       /* always unique even when re-    */
                                       /* reading the DIR_CONFIG file.   */
            char    str_job_id[MAX_INT_LENGTH];
            char    *dir;              /* Directory that has to be       */
                                       /* monitored.                     */
            int     dir_no;            /* Contains the directory ID!!!   */
            char    lfs;               /* Flag to indicate if this       */
                                       /* directory belongs to the same  */
                                       /* filesystem as the working      */
                                       /* directory of the AFD, i.e if   */
                                       /* we have to fork or not.        */
            time_t  next_start_time;
            struct bd_time_entry te;
            char    priority;          /* Priority of job.               */
            int     no_of_files;       /* Number of files to be send.    */
            char    *files;            /* The name of the file(s) to be  */
                                       /* send.                          */
            int     no_of_loptions;    /* Holds number of local          */
                                       /* options.                       */
            char    *loptions;         /* Storage of local options.      */
            int     no_of_soptions;    /* Holds number of standard       */
                                       /* options.                       */
            char    *soptions;         /* Storage of standard options.   */
            char    *recipient;        /* Storage of recipients.         */
            char    host_alias[MAX_HOSTNAME_LENGTH + 1];
                                       /* Alias of hostname of recipient.*/
            char    protocol;
            char    paused_dir[MAX_PATH_LENGTH];
                                       /* Stores absolute path of paused */
                                       /* or queued hosts.               */
            int     position;          /* Location of this host in the   */
                                       /* FSA.                           */
         };

/* Structure that holds all file information for one directory */
struct file_mask_entry
       {
          int  nfm;                          /* Number of file masks     */
          int  dest_count;                   /* Number of destinations   */
                                             /* for this directory.      */
          int  pos[MAX_NO_DEST_GROUP * MAX_NO_RECIPIENT];
                                             /* This holds the equivalent*/
                                             /* position in the struct   */
                                             /* instant_db.              */
          char *file_mask[MAX_NO_FILES];
       };
struct directory_entry
       {
          unsigned int           dir_no;     /* When reading a new       */
                                             /* DIR_CONFIG file this     */
                                             /* number is used to back-  */
                                             /* trace where a certain    */
                                             /* comes from in the input  */
                                             /* log.                     */
          int                    nfg;        /* Number of file groups    */
          int                    tnfm;       /* Total number of file     */
                                             /* masks.                   */
          int                    end_character; /* If this is not -1 the */
                                             /* function check_files()   */
                                             /* will always check the    */
                                             /* last character with      */
                                             /* end_character befor it   */
                                             /* accepts a file.          */
          int                    old_file_time;
          time_t                 mod_time;
          time_t                 search_time;/* Used when a new file     */
                                             /* arrives and we already   */
                                             /* have checked the         */
                                             /* directory and it is      */
                                             /* still the same second.   */
          struct file_mask_entry *fme;
          char                   remove_flag;
          char                   report_flag;
          char                   all_flag;   /* Set when all files in    */
                                             /* this directory are to be */
                                             /* distributed. Used to     */
                                             /* speed things up when     */
                                             /* searching for files.     */
          char                   *dir;       /* Pointer to directory     */
                                             /* name.                    */
       };

/* Structure that holds all hosts */
struct host_list
       {
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          fullname[MAX_FILENAME_LENGTH];
                                              /* This is needed when we   */
                                              /* have hostname with []    */
                                              /* syntax.                  */
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          char          host_toggle_str[MAX_TOGGLE_STR_LENGTH];
          char          proxy_name[MAX_PROXY_NAME_LENGTH];
          int           allowed_transfers;
          int           max_errors;
          int           retry_interval;
          int           transfer_blksize;
          int           successful_retries; /* NOTE: Corresponds to      */
                                            /* max_successful_retries in */
                                            /* FSA.                      */
          signed char   file_size_offset;
          long          transfer_timeout;
          unsigned char number_of_no_bursts;
          signed char   in_dir_config;
          unsigned char protocol;
       };

#define MAX_BIN_MSG_LENGTH 20
struct message_buf
       {
          char bin_msg_name[MAX_BIN_MSG_LENGTH];
       };

#ifdef _LIMIT_PROCESS_PER_DIR
struct dc_proc_list
       {
          pid_t pid;
          int   dir_no;
       };
#endif /* _LIMIT_PROCESS_PER_DIR */

/* Function prototypes */
extern int    amg_zombie_check(pid_t *, int),
              com(char),
#ifdef _WITH_PTHREAD
              check_files(struct directory_entry *, char *, char *,
                          char *, off_t *, char **, size_t *),
              link_files(char *, char *, off_t *, char **,
                         struct directory_entry *, struct instant_db *,
                         time_t *, unsigned short *, int, int,
                         char *, off_t *),
              rename_files(char *, char *, off_t *, char **,
                           struct directory_entry *, struct instant_db *,
                           time_t *, unsigned short *, int, int, char *,
                           off_t *, int),
              save_files(char *, char *, char **,
                         struct directory_entry *, int, int, char),
#else
              check_files(struct directory_entry *, char *, char *,
                          char *, size_t *),
              link_files(char *, char *, struct directory_entry *,
                         struct instant_db *, time_t *, unsigned short *,
                         int, int, char *, off_t *),
              rename_files(char *, char *, struct directory_entry *,
                           struct instant_db *, time_t *, unsigned short *,
                           int, int, char *, off_t *, int),
              save_files(char *, char *, struct directory_entry *,
                         int, int, char),
#endif
              check_process_list(int),
              create_db(int),
              eval_dir_config(size_t),
              eval_host_config(void),
              eval_time_str(char *, struct bd_time_entry *),
              exec_cmd(char *, char *),
              handle_options(int, char *, char *, int *, off_t *),
              lookup_dir_no(char *, int *),
              map_instant_db(size_t);
extern time_t calc_next_time(struct bd_time_entry *),
              write_host_config(void);
extern pid_t  make_process_amg(char *, int, int, int, int);
extern char   *check_paused_dir(struct directory_entry *),
              *get_hostname(char *),
              *next(char *);
extern void   check_old_time_jobs(int),
              clear_msg_buffer(void),
              clear_pool_dir(void),
              create_fsa(void),
              enter_time_job(int),
              eval_input_amg(int, char **, char *, char *, char *,
                             int *, int *),
              handle_time_jobs(int *, char *),
              init_dir_check(int, char **, char *, time_t *, int *,
                             int *, int *, int *),
              init_job_data(int *, int *),
              init_msg_buffer(void),
              lookup_job_id(struct instant_db *, int *),
              remove_pool_directory(char *, int),
              remove_time_dir(char *, int, int),
              reread_dir_config(time_t *, time_t *, int, int, size_t,
                                int, int, struct host_list *),
              reread_host_config(time_t *, int *, int *, size_t *,
                                 struct host_list **),
              search_old_files(void),
              send_message(char *, char *, unsigned short, time_t,
                           int, int, off_t),
              sort_time_job(void);
#ifdef _DEBUG
extern void   show_shm(FILE *);
#endif
#endif /* __amgdefs_h */
