/*
 *  init_dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2005 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   init_dir_check - initialize variables and fifos for process
 **                    dir_check
 **
 ** SYNOPSIS
 **   void init_dir_check(int    argc,
 **                       char   *argv[],
 **                       char   *rule_file,
 **                       time_t *rescan_time,
 **                       int    *read_fd,
 **                       int    *write_fd,
 **                       int    *del_time_job_fd)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   On succcess it returns the rule_file, rescan_time and the file
 **   descriptors read_fd, write_fd and fin_fd.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.04.1999 H.Kiehl Created
 **   12.01.2000 H.Kiehl Added receive log.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), strcat(), strerror()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit()                             */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <unistd.h>                /* geteuid(), getegid()               */
#include <errno.h>
#ifdef _WITH_PTHREAD
#include <pthread.h>
#endif
#include "amgdefs.h"


/* global variables */
extern int                    max_process,
#ifndef _WITH_PTHREAD
                              dir_check_timeout,
#endif
                              one_dir_copy_timeout,
                              no_of_dirs,
                              no_of_jobs,
                              no_of_local_dirs, /* No. of directories in */
                                                /* the DIR_CONFIG file   */
                                                /* that are local.       */
                              amg_counter_fd,   /* File descriptor for   */
                                                /* AMG counter file.     */
                              fin_fd,
#ifdef _INPUT_LOG
                              il_fd,
#endif
                              receive_log_fd;
extern time_t                 default_exec_timeout;
extern unsigned int           default_age_limit;
#ifdef _WITH_PTHREAD
extern pthread_t              *thread;
#else
extern unsigned int           max_file_buffer;
extern time_t                 *file_mtime_pool;
extern off_t                  *file_size_pool;
#endif
#ifdef _POSIX_SAVED_IDS
extern uid_t                  afd_uid;
extern gid_t                  afd_gid;
#endif
extern char                   *p_work_dir,
                              time_dir[],
                              *p_time_dir,
#ifndef _WITH_PTHREAD
                              **file_name_pool,
#endif
                              *afd_file_dir,
                              outgoing_file_dir[];
extern struct dc_proc_list    *dcpl;       /* Dir Check Process List     */
extern struct directory_entry *de;
#ifdef _DELETE_LOG
extern struct delete_log      dl;
#endif
#ifdef _WITH_PTHREAD
extern struct data_t          *p_data;
#endif

/* Local function prototypes. */
static void                   get_afd_config_value(void),
                              usage(void);


/*########################### init_dir_check() ##########################*/
void
init_dir_check(int    argc,
               char   *argv[],
               char   *rule_file,
               time_t *rescan_time,
               int    *read_fd,
               int    *write_fd,
               int    *del_time_job_fd)
{
   int         i;
   char        del_time_job_fifo[MAX_PATH_LENGTH],
#ifdef _INPUT_LOG
               input_log_fifo[MAX_PATH_LENGTH],
#endif
               dc_cmd_fifo[MAX_PATH_LENGTH],
               dc_resp_fifo[MAX_PATH_LENGTH],
               fin_fifo[MAX_PATH_LENGTH],
               receive_log_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Get call-up parameters */
   if (argc != 5)
   {
      usage();
   }
   else
   {
      (void)strcpy(p_work_dir, argv[1]);
      *rescan_time = atoi(argv[2]);
      max_process = atoi(argv[3]);
      no_of_local_dirs = atoi(argv[4]);
   }

#ifdef _POSIX_SAVED_IDS
   /* User and group ID */
   afd_uid = geteuid();
   afd_gid = getegid();
#endif

   /* Allocate memory for the array containing all file names to  */
   /* be send for every directory section in the DIR_CONFIG file. */
   if ((de = malloc(no_of_local_dirs * sizeof(struct directory_entry))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 no_of_local_dirs * sizeof(struct directory_entry),
                 strerror(errno));
      exit(INCORRECT);
   }
   i = strlen(p_work_dir) + (sizeof(AFD_FILE_DIR) - 1) + 1;
   if ((afd_file_dir = malloc(i)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   (void)strcpy(afd_file_dir, p_work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);
   (void)strcpy(outgoing_file_dir, afd_file_dir);
   (void)strcat(outgoing_file_dir, OUTGOING_DIR);
   (void)strcat(outgoing_file_dir, "/");
   (void)strcpy(time_dir, afd_file_dir);
   (void)strcat(time_dir, AFD_TIME_DIR);
   (void)strcat(time_dir, "/");
   p_time_dir = time_dir + strlen(time_dir);
   (void)strcpy(dc_cmd_fifo, p_work_dir);
   (void)strcat(dc_cmd_fifo, FIFO_DIR);
   (void)strcpy(fin_fifo, dc_cmd_fifo);
   (void)strcat(fin_fifo, IP_FIN_FIFO);
#ifdef _INPUT_LOG
   (void)strcpy(input_log_fifo, dc_cmd_fifo);
   (void)strcat(input_log_fifo, INPUT_LOG_FIFO);
#endif
   (void)strcpy(dc_resp_fifo, dc_cmd_fifo);
   (void)strcat(dc_resp_fifo, DC_RESP_FIFO);
   (void)strcpy(del_time_job_fifo, dc_cmd_fifo);
   (void)strcat(del_time_job_fifo, DEL_TIME_JOB_FIFO);
   (void)strcpy(receive_log_fifo, dc_cmd_fifo);
   (void)strcat(receive_log_fifo, RECEIVE_LOG_FIFO);
   (void)strcat(dc_cmd_fifo, DC_CMD_FIFO);
   init_msg_buffer();

#ifdef _DELETE_LOG
   delete_log_ptrs(&dl);
#endif

   /*
    * We need to attach to AFD status area to see if the FD is
    * up running. If not we will very quickly fill up the
    * message fifo to the FD.
    */
   if (attach_afd_status() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }

   get_afd_config_value();

#ifdef _WITH_PTHREAD
   if ((thread = malloc(no_of_local_dirs * sizeof(pthread_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((p_data = malloc(no_of_local_dirs * sizeof(struct data_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_local_dirs; i++)
   {
      p_data[i].i = i;
      (void)strcpy(p_data[i].afd_file_dir, afd_file_dir);
      de[i].fme = NULL;
   }
#else
   for (i = 0; i < no_of_local_dirs; i++)
   {
      de[i].fme = NULL;
   }
   RT_ARRAY(file_name_pool, max_file_buffer, MAX_FILENAME_LENGTH, char);
   if ((file_mtime_pool = malloc(max_file_buffer * sizeof(off_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(off_t), strerror(errno));
      exit(INCORRECT);
   }
   if ((file_size_pool = malloc(max_file_buffer * sizeof(off_t))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(off_t), strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Open receive log fifo */
   if ((stat(receive_log_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(receive_log_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", receive_log_fifo);
         exit(INCORRECT);
      }
   }
   if ((receive_log_fd = coe_open(receive_log_fifo, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 receive_log_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Open counter file so we can create unique names for each job.
    */
   if ((amg_counter_fd = open_counter_file(AMG_COUNTER_FILE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open counter file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Get the fra_id and no of directories of the FRA */
   if (fra_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }

   /* Get the fsa_id and no of host of the FSA */
   if (fsa_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }

   /* Open fifos to communicate with AMG */
   if ((*write_fd = coe_open(dc_resp_fifo, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_resp_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Open fifo to wait for answer from job */
   if ((*read_fd = coe_open(dc_cmd_fifo, O_RDWR | O_NONBLOCK)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_cmd_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /*
    * Create and open fifo for process copying/moving files.
    * The child will tell the parent when it is finished via
    * this fifo.
    */
   if ((stat(fin_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(fin_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", fin_fifo);
         exit(INCORRECT);
      }
   }
   if ((fin_fd = coe_open(fin_fifo, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", fin_fifo, strerror(errno));
      exit(INCORRECT);
   }
   if ((stat(del_time_job_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(del_time_job_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create fifo %s.", del_time_job_fifo);
         exit(INCORRECT);
      }
   }
   if ((*del_time_job_fd = coe_open(del_time_job_fifo, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 del_time_job_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Now create the internal database of this process */
   no_of_jobs = create_db();

#ifdef _WITH_PTHREAD
   for (i = 0; i < no_of_local_dirs; i++)
   {
      RT_ARRAY(p_data[i].file_name_pool, fra[i].max_copied_files,
               MAX_FILENAME_LENGTH, char);
      if ((p_data[i].file_mtime_pool = malloc(fra[i].max_copied_files * sizeof(off_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      if ((p_data[i].file_size_pool = malloc(fra[i].max_copied_files * sizeof(off_t))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }
#endif /* _WITH_PTHREAD */

   /* Allocate space for process ID array. */
   if ((dcpl = malloc(max_process * sizeof(struct dc_proc_list))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Not enough memory to malloc() : %s", strerror(errno));
      exit(INCORRECT);
   }
   for (i = 0; i < max_process; i++)
   {
      dcpl[i].fra_pos = -1;
      dcpl[i].pid = -1;
   }

#ifdef _INPUT_LOG
   if ((stat(input_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(input_log_fifo) < 0)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                   "Could not create fifo %s.", input_log_fifo);
         exit(INCORRECT);
      }
   }
   if ((il_fd = coe_open(input_log_fifo, O_RDWR)) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Could not open fifo %s : %s", input_log_fifo, strerror(errno));
      exit(INCORRECT);
   }
#endif /* _INPUT_LOG */

   (void)strcpy(rule_file, p_work_dir);
   (void)strcat(rule_file, ETC_DIR);
   (void)strcat(rule_file, RENAME_RULE_FILE);
   get_rename_rules(rule_file, YES);

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : dir_check [--version] <AFD working directory> <rescan time> <no. of. process> <no. of dirs>\n");
   exit(INCORRECT);
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, ONE_DIR_COPY_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         one_dir_copy_timeout = atoi(value);
         if ((one_dir_copy_timeout < 2) || (one_dir_copy_timeout > 3600))
         {
            one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
         }
      }
      else
      {
         one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
      }
#ifndef _WITH_PTHREAD
      if (get_definition(buffer, DIR_CHECK_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         dir_check_timeout = atoi(value);
         if ((dir_check_timeout < 2) || (dir_check_timeout > 3600))
         {
            dir_check_timeout = DIR_CHECK_TIMEOUT;
         }
      }
      else
      {
         dir_check_timeout = DIR_CHECK_TIMEOUT;
      }
      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_file_buffer = atoi(value);
         if ((max_file_buffer < 1) || (max_file_buffer > 10240))
         {
            max_file_buffer = MAX_COPIED_FILES;
         }
      }
      else
      {
         max_file_buffer = MAX_COPIED_FILES;
      }
#endif
      if (get_definition(buffer, DEFAULT_AGE_LIMIT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_age_limit = (unsigned int)atoi(value);
      }
      else
      {
         default_age_limit = DEFAULT_AGE_LIMIT;
      }
      if (get_definition(buffer, EXEC_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         default_exec_timeout = atol(value);
      }
      else
      {
         default_exec_timeout = DEFAULT_EXEC_TIMEOUT;
      }
      free(buffer);
   }
   else
   {
      one_dir_copy_timeout = ONE_DIR_COPY_TIMEOUT;
#ifndef _WITH_PTHREAD
      dir_check_timeout = DIR_CHECK_TIMEOUT;
#endif
      default_age_limit = DEFAULT_AGE_LIMIT;
      max_file_buffer = MAX_COPIED_FILES;
      default_exec_timeout = DEFAULT_EXEC_TIMEOUT;
   }

   return;
}