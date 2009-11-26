/*
 *  init_dir_check.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2009 Deutscher Wetterdienst (DWD),
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
#include <signal.h>                /* kill()                             */
#ifdef WITH_INOTIFY
# include <sys/inotify.h>          /* inotify_init()                     */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>                /* geteuid(), getegid()               */
#include <errno.h>
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include "amgdefs.h"


/* Global variables. */
extern int                        afd_file_dir_length,
                                  afd_status_fd,
                                  dcpl_fd,
                                  max_process,
                                  *no_of_process,
#ifndef _WITH_PTHREAD
                                  dir_check_timeout,
#endif
                                  full_scan_timeout,
#ifdef WITH_INOTIFY
                                  inotify_fd,
                                  *iwl,
                                  no_of_inotify_dirs,
#endif
                                  one_dir_copy_timeout,
                                  no_of_jobs,
                                  no_of_local_dirs, /* No. of directories in*/
                                                    /* the DIR_CONFIG file  */
                                                    /* that are local.      */
                                  no_of_orphaned_procs,
                                  *amg_counter,
                                  amg_counter_fd,   /* File descriptor for  */
                                                    /* AMG counter file.    */
                                  fin_fd,
#ifdef _INPUT_LOG
                                  il_fd,
#endif
#ifdef WITHOUT_FIFO_RW_SUPPORT
                                  dc_cmd_writefd,
                                  dc_resp_readfd,
                                  del_time_job_writefd,
                                  fin_writefd,
                                  receive_log_readfd,
#endif
                                  receive_log_fd;
extern time_t                     default_exec_timeout;
extern unsigned int               default_age_limit;
extern pid_t                      *opl;
#ifdef _WITH_PTHREAD
extern pthread_t                  *thread;
#else
extern unsigned int               max_file_buffer;
extern time_t                     *file_mtime_pool;
extern off_t                      *file_size_pool;
#endif
#ifdef _POSIX_SAVED_IDS
extern int                        no_of_sgids;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid,
                                  *afd_sgids;
#endif
extern char                       *p_work_dir,
                                  time_dir[],
                                  *p_time_dir,
#ifndef _WITH_PTHREAD
                                  **file_name_pool,
#endif
                                  *afd_file_dir,
                                  outgoing_file_dir[];
#ifndef _WITH_PTHREAD
extern unsigned char              *file_length_pool;
#endif
extern struct dc_proc_list        *dcpl;      /* Dir Check Process List. */
extern struct directory_entry     *de;
extern struct afd_status          *p_afd_status;
extern struct instant_db          *db;
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef _DISTRIBUTION_LOG
extern struct file_dist_list      **file_dist_pool;
#endif
#ifdef _WITH_PTHREAD
extern struct data_t              *p_data;
#endif

/* Local function prototypes. */
static void                       get_afd_config_value(void),
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
#ifdef _WITH_PTHREAD
# ifdef _DISTRIBUTION_LOG
   int         j;
# endif
#endif
   size_t      size;
   char        del_time_job_fifo[MAX_PATH_LENGTH],
#ifdef _INPUT_LOG
               input_log_fifo[MAX_PATH_LENGTH],
#endif
               dc_cmd_fifo[MAX_PATH_LENGTH],
               dcpl_data_file[MAX_PATH_LENGTH],
               dc_resp_fifo[MAX_PATH_LENGTH],
               fin_fifo[MAX_PATH_LENGTH],
               other_fifo[MAX_PATH_LENGTH],
               *p_other_fifo,
               *ptr,
               receive_log_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   /* Get call-up parameters. */
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
   /* User and group ID. */
   afd_uid = geteuid();
   afd_gid = getegid();
   no_of_sgids = getgroups(0, NULL);
   if (no_of_sgids > 0)
   {
      if ((afd_sgids = malloc((no_of_sgids * sizeof(gid_t)))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (no_of_sgids * sizeof(gid_t)), strerror(errno));
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Unable to check supplementary groups!");
         no_of_sgids = 0;
      }
      else
      {
         if (getgroups(no_of_sgids, afd_sgids) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "getgroups() error : %s", strerror(errno));
            no_of_sgids = 0;
         }
      }
   }
   else
   {
      no_of_sgids = 0;
      afd_sgids = NULL;
   }
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
   afd_file_dir_length = strlen(p_work_dir) + AFD_FILE_DIR_LENGTH;
   if ((afd_file_dir = malloc(afd_file_dir_length + 1)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(afd_file_dir, p_work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);
   (void)strcpy(outgoing_file_dir, afd_file_dir);
   (void)strcpy(outgoing_file_dir + afd_file_dir_length, OUTGOING_DIR);
   outgoing_file_dir[afd_file_dir_length + OUTGOING_DIR_LENGTH] = '/';
   outgoing_file_dir[afd_file_dir_length + OUTGOING_DIR_LENGTH + 1] = '\0';
   (void)strcpy(time_dir, afd_file_dir);
   (void)strcpy(time_dir + afd_file_dir_length, AFD_TIME_DIR);
   time_dir[afd_file_dir_length + AFD_TIME_DIR_LENGTH] = '/';
   time_dir[afd_file_dir_length + AFD_TIME_DIR_LENGTH + 1] = '\0';
   p_time_dir = time_dir + afd_file_dir_length + AFD_TIME_DIR_LENGTH + 1;
   (void)strcpy(dc_cmd_fifo, p_work_dir);
   (void)strcat(dc_cmd_fifo, FIFO_DIR);
   (void)strcpy(other_fifo, dc_cmd_fifo);
   p_other_fifo = other_fifo + strlen(other_fifo);
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
   (void)strcpy(dcpl_data_file, dc_cmd_fifo);
   (void)strcat(dcpl_data_file, DCPL_FILE_NAME);
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
   if (attach_afd_status(&afd_status_fd) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }
   p_afd_status->amg_jobs &= ~CHECK_FILE_DIR_ACTIVE;

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
      de[i].fme   = NULL;
      de[i].rl_fd = -1;
      de[i].rl    = NULL;
   }
#else
   for (i = 0; i < no_of_local_dirs; i++)
   {
      de[i].fme   = NULL;
      de[i].rl_fd = -1;
      de[i].rl    = NULL;
   }
   RT_ARRAY(file_name_pool, max_file_buffer, MAX_FILENAME_LENGTH, char);
   if ((file_length_pool = malloc(max_file_buffer * sizeof(unsigned char))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(unsigned char), strerror(errno));
      exit(INCORRECT);
   }
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
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
   RT_ARRAY(file_dist_pool, max_file_buffer, NO_OF_DISTRIBUTION_TYPES,
            (struct file_dist_list));
#  else
   if ((file_dist_pool = (struct file_dist_list **)malloc(max_file_buffer * sizeof(struct file_dist_list *))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * sizeof(struct file_dist_list *),
                 strerror(errno));
      exit(INCORRECT);
   }
   if ((file_dist_pool[0] = (struct file_dist_list *)malloc(max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list),
                 strerror(errno));
      exit(INCORRECT);
   }
   for (i = 1; i < max_file_buffer; i++)
   {
      file_dist_pool[i] = file_dist_pool[0] + (i * NO_OF_DISTRIBUTION_TYPES);
   }
#  endif
# endif
#endif

   /* Open receive log fifo. */
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
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(receive_log_fifo, &receive_log_readfd, &receive_log_fd) == -1)
#else
   if ((receive_log_fd = coe_open(receive_log_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 receive_log_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Check if the queue list fifos exist, if not create them. */
   (void)strcpy(p_other_fifo, QUEUE_LIST_READY_FIFO);
   if ((stat(other_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(other_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", other_fifo);
         exit(INCORRECT);
      }
   }
   (void)strcpy(p_other_fifo, QUEUE_LIST_DONE_FIFO);
   if ((stat(other_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(other_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", other_fifo);
         exit(INCORRECT);
      }
   }

   /* Open counter file so we can create unique names for each job. */
   if ((amg_counter_fd = open_counter_file(AMG_COUNTER_FILE, &amg_counter)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open counter file : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Get the fra_id and no of directories of the FRA. */
   if (fra_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }

   /* Get the fsa_id and no of host of the FSA. */
   if (fsa_attach() < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }

   /* Open fifos to communicate with AMG. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(dc_resp_fifo, &dc_resp_readfd, write_fd) == -1)
#else
   if ((*write_fd = coe_open(dc_resp_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_resp_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Open fifo to wait for answer from job. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(dc_cmd_fifo, read_fd, &dc_cmd_writefd) == -1)
#else
   if ((*read_fd = coe_open(dc_cmd_fifo, O_RDWR | O_NONBLOCK)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", dc_cmd_fifo, strerror(errno));
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   set_fl(*read_fd, O_NONBLOCK);
#endif

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
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(fin_fifo, &fin_fd, &fin_writefd) == -1)
#else
   if ((fin_fd = coe_open(fin_fifo, O_RDWR)) == -1)
#endif
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
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(del_time_job_fifo, del_time_job_fd, &del_time_job_writefd) == -1)
#else
   if ((*del_time_job_fd = coe_open(del_time_job_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s",
                 del_time_job_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Now create the internal database of this process. */
   no_of_jobs = create_db();

#ifdef _WITH_PTHREAD
   for (i = 0; i < no_of_local_dirs; i++)
   {
      RT_ARRAY(p_data[i].file_name_pool, fra[i].max_copied_files,
               MAX_FILENAME_LENGTH, char);
      if ((p_data[i].file_length_pool = malloc(fra[i].max_copied_files * sizeof(unsigned char))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * sizeof(unsigned char),
                    strerror(errno));
         exit(INCORRECT);
      }
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
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
      RT_ARRAY(p_data[i].file_dist_pool, fra[i].max_copied_files, NO_OF_DISTRIBUTION_TYPES,
               (struct file_dist_list));
#  else
      if ((p_data[i].file_dist_pool = (struct file_dist_list **)malloc(fra[i].max_copied_files * sizeof(struct file_dist_list *))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * sizeof(struct file_dist_list *),
                    strerror(errno));
         exit(INCORRECT);
      }
      if ((p_data[i].file_dist_pool[0] = (struct file_dist_list *)malloc(fra[i].max_copied_files * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fra[i].max_copied_files * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list),
                    strerror(errno));
         exit(INCORRECT);
      }
      for (j = 1; j < fra[i].max_copied_files; j++)
      {
         p_data[i].file_dist_pool[j] = p_data[i].file_dist_pool[0] + (j * NO_OF_DISTRIBUTION_TYPES);
      }
#  endif
# endif
   }
#endif

   /* Attach to process ID array. */
   size = (max_process * sizeof(struct dc_proc_list)) + AFD_WORD_OFFSET;
   if ((ptr = attach_buf(dcpl_data_file, &dcpl_fd, &size,
                         DIR_CHECK, FILE_MODE, NO)) == (caddr_t) -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() `%s' : %s", dcpl_data_file, strerror(errno));
      exit(INCORRECT);
   }
   no_of_process = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   dcpl = (struct dc_proc_list *)ptr;

   /* Initialize, but don't overwrite existing process! */
   for (i = *no_of_process; i < max_process; i++)
   {
      dcpl[i].fra_pos = -1;
      dcpl[i].pid = -1;
   }

   /* Check if the process in the list still exist. */
   no_of_orphaned_procs = 0;
   opl = NULL;
   if (*no_of_process > 0)
   {
      for (i = 0; i < *no_of_process; i++)
      {
         if (dcpl[i].pid > 0)
         {
            if (kill(dcpl[i].pid, 0) == -1)
            {
               /* The process no longer exist, so lets remove it. */
               (*no_of_process)--;
               if (i < *no_of_process)
               {
                  (void)memmove(&dcpl[i], &dcpl[i + 1],
                                ((*no_of_process - i) * sizeof(struct dc_proc_list)));
               }
               dcpl[*no_of_process].pid = -1;
               dcpl[*no_of_process].fra_pos = -1;
               i--;
            }
            else
            {
               int gotcha,
                   j;

               /*
                * The process still exists. We now need to check if
                * the job still exist and if fsa_pos is still correct.
                */
               gotcha = NO;
               for (j = 0; j < no_of_jobs; j++)
               {
                  if (db[j].job_id == dcpl[i].job_id)
                  {
                     if (db[j].fra_pos != dcpl[i].fra_pos)
                     {
                        dcpl[i].fra_pos = db[j].fra_pos;
                     }
                     if ((no_of_orphaned_procs % ORPHANED_PROC_STEP_SIZE) == 0)
                     {
                        size = ((no_of_orphaned_procs / ORPHANED_PROC_STEP_SIZE) + 1) *
                               ORPHANED_PROC_STEP_SIZE * sizeof(pid_t);

                        if ((opl = realloc(opl, size)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to realloc() %d bytes : %s",
                                      size, strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     fra[db[j].fra_pos].no_of_process++;
                     opl[no_of_orphaned_procs] = dcpl[i].pid;
                     no_of_orphaned_procs++;
                     gotcha = YES;
                     break;
                  }
               }
               if (gotcha == NO)
               {
                  /*
                   * The process is no longer in the current job list.
                   * We may not kill this process, since we do NOT
                   * know if this is one of our process that we
                   * started. But lets remove it from our job list.
                   */
                  (*no_of_process)--;
                  if (i < *no_of_process)
                  {
                     (void)memmove(&dcpl[i], &dcpl[i + 1],
                                   ((*no_of_process - i) * sizeof(struct dc_proc_list)));
                  }
                  dcpl[*no_of_process].pid = -1;
                  dcpl[*no_of_process].fra_pos = -1;
                  i--;
               }
            }
         }
         else
         {
            /* Hmm, looks like garbage. Lets remove this. */
            (*no_of_process)--;
            if (i < *no_of_process)
            {
               (void)memmove(&dcpl[i], &dcpl[i + 1],
                             ((*no_of_process - i) * sizeof(struct dc_proc_list)));
            }
            dcpl[*no_of_process].pid = -1;
            dcpl[*no_of_process].fra_pos = -1;
            i--;
         }
      }
   }
   if (no_of_orphaned_procs)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmm, there are %d orphaned process.", no_of_orphaned_procs);
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
#endif

   (void)strcpy(rule_file, p_work_dir);
   (void)strcat(rule_file, ETC_DIR);
   (void)strcat(rule_file, RENAME_RULE_FILE);
   get_rename_rules(rule_file, YES);

#ifdef WITH_ERROR_QUEUE
   if (attach_error_queue() == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to attach to the error queue!");
   }
#endif

   remove_old_ls_data_files();

#ifdef WITH_INOTIFY
   no_of_inotify_dirs = 0;
   for (i = 0; i < no_of_local_dirs; i++)
   {
      if (((fra[de[i].fra_pos].dir_flag & INOTIFY_RENAME) ||
           (fra[de[i].fra_pos].dir_flag & INOTIFY_CLOSE)) &&
          (fra[de[i].fra_pos].no_of_time_entries == 0) &&
          (fra[de[i].fra_pos].force_reread == NO))
      {
         no_of_inotify_dirs++;
      }
   }
   if (no_of_inotify_dirs > 0)
   {
      int      j;
      uint32_t mask;

      if ((iwl = malloc(no_of_inotify_dirs * sizeof(int))) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (no_of_inotify_dirs * sizeof(int)), strerror(errno));
         exit(INCORRECT);
      }
      if ((inotify_fd = inotify_init()) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to inotify_init() : %s", strerror(errno));
         exit(INCORRECT);
      }
      j = 0;
      for (i = 0; i < no_of_local_dirs; i++)
      {
         if (((fra[de[i].fra_pos].dir_flag & INOTIFY_RENAME) ||
              (fra[de[i].fra_pos].dir_flag & INOTIFY_CLOSE)) &&
             (fra[de[i].fra_pos].no_of_time_entries == 0) &&
             (fra[de[i].fra_pos].force_reread == NO))
         {
            mask = 0;
            if (fra[de[i].fra_pos].dir_flag & INOTIFY_RENAME)
            {
               mask |= IN_MOVED_TO;
            }
            if (fra[de[i].fra_pos].dir_flag & INOTIFY_CLOSE)
            {
               mask |= (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE);
            }

            if ((iwl[j] = inotify_add_watch(inotify_fd, de[i].dir, mask)) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "inotify_add_watch() error for `%s' : %s",
                          de[i].dir, strerror(errno));
            }
            j++;
            if (j > no_of_inotify_dirs)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "How can this be? This is an error of the programmer!");
               break;
            }
         }
      }
   }
   else
   {
      inotify_fd = -1;
   }
#endif

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
       (read_file_no_cr(config_file, &buffer) != INCORRECT))
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
      if (get_definition(buffer, FULL_SCAN_TIMEOUT_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         full_scan_timeout = atoi(value);
         if ((full_scan_timeout < 0) || (full_scan_timeout > 3600))
         {
            full_scan_timeout = FULL_SCAN_TIMEOUT;
         }
      }
      else
      {
         full_scan_timeout = FULL_SCAN_TIMEOUT;
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
