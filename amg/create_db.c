/*
 *  create_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   create_db - creates structure instant_db and initialises it
 **               for dir_check
 **
 ** SYNOPSIS
 **   int create_db(int shmem_id)
 **
 ** DESCRIPTION
 **   This function creates structure instant_db and initialises it
 **   with data from the shared memory area created by the AMG. See
 **   amgdefs.h for a more detailed description of structure instant_db.
 **
 ** RETURN VALUES
 **   Will exit with INCORRECT when itt encounters an error. On success
 **   it will return the number of jobs it has found in the shared
 **   memory area.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.10.1995 H.Kiehl Created
 **   02.01.1997 H.Kiehl Added logging of file names that are found by
 **                      by the AFD.
 **   01.02.1997 H.Kiehl Read renaming rule file.
 **   19.04.1997 H.Kiehl Add support for writing messages in one memory
 **                      mapped file.
 **   26.10.1997 H.Kiehl If disk is full do not give up.
 **   21.01.1998 H.Kiehl Rewrite to accommodate new messages and job ID's.
 **   14.08.2000 H.Kiehl File mask no longer a fixed array.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), strcat(), strcmp(),      */
                                   /* memcpy(), strerror(), strlen()     */
#include <stdlib.h>                /* atoi(), calloc(), malloc(), free(),*/
                                   /* exit()                             */
#include <ctype.h>                 /* isdigit()                          */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>              /* msync(), munmap()                  */
#include <sys/ipc.h>
#include <sys/shm.h>               /* shmat()                            */
#include <fcntl.h>
#include <unistd.h>                /* fork(), rmdir(), fstat()           */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        no_of_hosts,
                                  no_of_time_jobs,
#ifndef _WITH_PTHREAD
                                  no_of_important_dirs,
                                  *important_dir_no,
#endif
                                  no_of_local_dirs,/* No. of directories */
                                                   /* in DIR_CONFIG file */
                                                   /* that are local.    */
                                  counter_fd,     /* File descriptor for */
                                                  /* AFD counter file.   */
                                  fd_cmd_fd,
                                  sys_log_fd,
                                  *time_job_list;
extern char                       *p_work_dir,
                                  *p_shm;
extern struct directory_entry     *de;
extern struct instant_db          *db;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;

/* Global variables */
int                               dnb_fd,
                                  jd_fd,
                                  jid_fd,
                                  *no_of_dir_names,
                                  *no_of_job_ids;
char                              *gotcha,
                                  msg_dir[MAX_PATH_LENGTH],
                                  *p_msg_dir;
struct job_id_data                *jd;
struct dir_name_buf               *dnb;

/* Local function prototypes. */
static void                       write_numbers(int),
                                  write_current_msg_list(int),
                                  unmap_data(int, void *);

/* #define _WITH_JOB_LIST_TEST 1 */
#define POS_STEP_SIZE 20

/*+++++++++++++++++++++++++++++ create_db() +++++++++++++++++++++++++++++*/
int
create_db(int shmem_id)
{
   int             i,
                   j,
                   jid_number,
#ifdef _TEST_FILE_TABLE
                   k,
#endif
                   not_in_same_file_system = 0,
                   size,
                   dir_counter = 0,
                   no_of_jobs;
   dev_t           ldv;               /* local device number (filesystem) */
   char            *ptr,
                   *p_sheme,
                   *host_ptr,
                   *tmp_ptr,
                   *p_offset,
                   *p_loptions,
                   *p_file,
                   *p_hostname;
   struct p_array  *p_ptr;
   struct stat     stat_buf;

   /* Set flag to indicate that we are writting in JID structure */
   if ((p_afd_status->amg_jobs & WRITTING_JID_STRUCT) == 0)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
   }

   /* Get device number for working directory */
   if (stat(p_work_dir, &stat_buf) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to stat() %s : %s (%s %d)\n",
                p_work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   ldv = stat_buf.st_dev;

   /* Attach to shared memory regions */
   if ((void *)(p_shm = shmat(shmem_id, 0, 0)) == (void *) -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not attach to shared memory region %d : %s (%s %d)\n",
                shmem_id, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   ptr = p_shm;

   /* First get the no of jobs */
   no_of_jobs = *(int *)ptr;
   ptr += sizeof(int);

   /* Create space and map to instant database */
   if (map_instant_db(no_of_jobs * sizeof(struct instant_db)) == INCORRECT)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      exit(INCORRECT);
   }

   init_job_data(&jid_number);

   /* Allocate space for the gotchas. */
   size = ((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
           JOB_ID_DATA_STEP_SIZE * 1;
   if ((gotcha = malloc(size)) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to malloc() memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef _WITH_JOB_LIST_TEST
   {
      int changed = 0;

      for (i = 0; i < size; i++)
      {
         if (changed < 7)
         {
            gotcha[i] = YES;
            changed = 0;
         }
         else
         {
            gotcha[i] = NO;
         }
         changed++;
      }
   }
#else
   (void)memset(gotcha, NO, size);
#endif

   if (no_of_time_jobs > 0)
   {
      no_of_time_jobs = 0;
      free(time_job_list);
      time_job_list = NULL;
   }

   /* Create and copy pointer array */
   size = no_of_jobs * sizeof(struct p_array);
   if ((tmp_ptr = calloc(no_of_jobs, sizeof(struct p_array))) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not allocate memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   p_ptr = (struct p_array *)tmp_ptr;
   memcpy(p_ptr, ptr, size);
   p_offset = ptr + size;

   de[0].nfg           = 0;
   de[0].fme           = NULL;
   de[0].all_flag      = NO;
   de[0].dir           = (int)(p_ptr[0].ptr[1]) + p_offset;
   de[0].alias         = (int)(p_ptr[0].ptr[2]) + p_offset;
   if ((de[0].fra_pos = lookup_fra_pos(de[0].alias)) == INCORRECT)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to locate dir alias <%s> for directory %s (%s %d)\n",
                de[0].alias, de[0].dir, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   de[0].dir_no        = dnb[fra[de[0].fra_pos].dir_pos].dir_id;
   de[0].mod_time      = -1;
   de[0].search_time   = 0;
   if (stat(de[0].dir, &stat_buf) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to stat() %s : %s (%s %d)\n",
                de[0].dir, strerror(errno), __FILE__, __LINE__);
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      free(db); free(de);
      free(tmp_ptr);
      unmap_data(jd_fd, jd);
      exit(INCORRECT);
   }

   for (i = 0; i < no_of_jobs; i++)
   {
      /* Store directory pointer */
      db[i].dir = (int)(p_ptr[i].ptr[1]) + p_offset;

      /* Store priority */
      db[i].priority = *((int)(p_ptr[i].ptr[0]) + p_offset);

      /* Store number of files to be send */
      db[i].no_of_files = atoi((int)(p_ptr[i].ptr[3]) + p_offset);

      /* Store pointer to first file (filter) */
      db[i].files = (int)(p_ptr[i].ptr[4]) + p_offset;

      db[i].fra_pos = de[dir_counter].fra_pos;

      /*
       * Store all file names of one directory into one array. This
       * is necessary so we can specify overlapping wild cards in
       * different file sections for one directory section.
       */
      if ((i > 0) && (db[i].dir != db[i - 1].dir))
      {
         dir_counter++;
         de[dir_counter].dir           = db[i].dir;
         de[dir_counter].alias         = (int)(p_ptr[i].ptr[2]) + p_offset;
         if ((de[dir_counter].fra_pos = lookup_fra_pos(de[dir_counter].alias)) == INCORRECT)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to locate dir alias <%s> for directory %s (%s %d)\n",
                      de[dir_counter].alias, de[dir_counter].dir, __FILE__, __LINE__);
            exit(INCORRECT);
         }
         de[dir_counter].dir_no        = dnb[fra[de[dir_counter].fra_pos].dir_pos].dir_id;
         de[dir_counter].nfg           = 0;
         de[dir_counter].fme           = NULL;
         de[dir_counter].all_flag      = NO;
         de[dir_counter].mod_time      = -1;
         de[dir_counter].search_time   = 0;
         if (stat(db[i].dir, &stat_buf) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      db[i].dir, strerror(errno), __FILE__, __LINE__);
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            free(db); free(de);
            free(tmp_ptr);
            unmap_data(jd_fd, jd);
            exit(INCORRECT);
         }
         if (stat_buf.st_dev != ldv)
         {
            not_in_same_file_system++;
         }
      }
      db[i].dir_no = de[dir_counter].dir_no;

      /*
       * Check if this directory is in the same file system
       * as file directory of the AFD. If this is not the case
       * lets fork when we copy.
       */
      db[i].lfs = 0;
      if (stat_buf.st_dev == ldv)
      {
         db[i].lfs |= YES;
      }

      if ((i == 0) || ((i > 0) && (db[i].files != db[i - 1].files)))
      {
         if ((de[dir_counter].nfg % FG_BUFFER_STEP_SIZE) == 0)
         {
            char   *ptr_start;
            size_t new_size;

            /* Calculate new size of file group buffer */
             new_size = ((de[dir_counter].nfg / FG_BUFFER_STEP_SIZE) + 1) *
                        FG_BUFFER_STEP_SIZE * sizeof(struct file_mask_entry);

            if ((de[dir_counter].fme = (struct file_mask_entry *)realloc(de[dir_counter].fme, new_size)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "realloc() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               unmap_data(jd_fd, jd);
               exit(INCORRECT);
            }

            /* Initialise new memory area */
            if (de[dir_counter].nfg > (FG_BUFFER_STEP_SIZE - 1))
            {
               ptr_start = (char *)(de[dir_counter].fme) +
                           (de[dir_counter].nfg * sizeof(struct file_mask_entry));
            }
            else
            {
               ptr_start = (char *)de[dir_counter].fme;
            }
            (void)memset(ptr_start, 0, (FG_BUFFER_STEP_SIZE * sizeof(struct file_mask_entry)));
         }
         p_file = db[i].files;
         de[dir_counter].fme[de[dir_counter].nfg].nfm = db[i].no_of_files;
         if ((de[dir_counter].fme[de[dir_counter].nfg].file_mask = malloc((db[i].no_of_files * sizeof(char *)))) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            unmap_data(jd_fd, jd);
            exit(INCORRECT);
         }
         de[dir_counter].fme[de[dir_counter].nfg].dest_count = 0;
         for (j = 0; j < db[i].no_of_files; j++)
         {
            de[dir_counter].fme[de[dir_counter].nfg].file_mask[j] = p_file;
            if ((p_file[0] == '*') && (p_file[1] == '\0'))
            {
               de[dir_counter].all_flag = YES;
            }
            NEXT(p_file);
         }
         if ((de[dir_counter].all_flag == YES) && (db[i].no_of_files > 1))
         {
            p_file = db[i].files;
            for (j = 0; j < db[i].no_of_files; j++)
            {
               if (p_file[0] == '!')
               {
                  de[dir_counter].all_flag = NO;
                  break;
               }
               NEXT(p_file);
            }
         }
         if ((de[dir_counter].fme[de[dir_counter].nfg].dest_count % POS_STEP_SIZE) == 0)
         {
            size_t new_size;

            /* Calculate new size of file group buffer */
             new_size = ((de[dir_counter].fme[de[dir_counter].nfg].dest_count / POS_STEP_SIZE) + 1) *
                        POS_STEP_SIZE * sizeof(int);

            if ((de[dir_counter].fme[de[dir_counter].nfg].pos = realloc(de[dir_counter].fme[de[dir_counter].nfg].pos, new_size)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "realloc() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               unmap_data(jd_fd, jd);
               exit(INCORRECT);
            }
         }
         de[dir_counter].fme[de[dir_counter].nfg].pos[de[dir_counter].fme[de[dir_counter].nfg].dest_count] = i;
         de[dir_counter].fme[de[dir_counter].nfg].dest_count++;
         de[dir_counter].nfg++;
      }
      else if ((i > 0) && (db[i].files == db[i - 1].files))
           {
              if ((de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count % POS_STEP_SIZE) == 0)
              {
                 size_t new_size;

                 /* Calculate new size of file group buffer */
                  new_size = ((de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count / POS_STEP_SIZE) + 1) *
                             POS_STEP_SIZE * sizeof(int);

                 if ((de[dir_counter].fme[de[dir_counter].nfg - 1].pos = realloc(de[dir_counter].fme[de[dir_counter].nfg - 1].pos, new_size)) == NULL)
                 {
                    (void)rec(sys_log_fd, FATAL_SIGN,
                              "realloc() error : %s (%s %d)\n",
                              strerror(errno), __FILE__, __LINE__);
                    p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
                    unmap_data(jd_fd, jd);
                    exit(INCORRECT);
                 }
              }
              de[dir_counter].fme[de[dir_counter].nfg - 1].pos[de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count] = i;
              de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count++;
           }

      /* Store number of local options */
      db[i].no_of_loptions = atoi((int)(p_ptr[i].ptr[5]) + p_offset);
      db[i].next_start_time = 0;

      /* Store pointer to first local option */
      if (db[i].no_of_loptions > 0)
      {
         db[i].loptions = (int)(p_ptr[i].ptr[6]) + p_offset;
         db[i].time_option_type = NO_TIME;

         /*
          * Because extracting bulletins from files can take quit a
          * while, make shure that we fork. We can do this by setting
          * the lfs flag to GO_PARALLEL.
          */
         p_loptions = db[i].loptions;
         for (j = 0; j < db[i].no_of_loptions; j++)
         {
            if (strncmp(p_loptions, EXEC_ID, EXEC_ID_LENGTH) == 0)
            {
               db[i].lfs |= GO_PARALLEL;
               db[i].lfs |= DO_NOT_LINK_FILES;
            }
            /*
             * NOTE: TIME_NO_COLLECT_ID option __must__ be checked before
             *       TIME_ID. Since both start with time and TIME_ID only
             *       consists only of the word time.
             */
            else if (strncmp(p_loptions, TIME_NO_COLLECT_ID, TIME_NO_COLLECT_ID_LENGTH) == 0)
                 {
                    char *ptr = p_loptions + TIME_NO_COLLECT_ID_LENGTH;

                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (eval_time_str(ptr, &db[i].te) == SUCCESS)
                    {
                       db[i].time_option_type = SEND_NO_COLLECT_TIME;
                    }
                    else
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "%s (%s %d)\n", ptr, __FILE__, __LINE__);
                    }
                 }
            else if (strncmp(p_loptions, TIME_ID, TIME_ID_LENGTH) == 0)
                 {
                    char *ptr = p_loptions + TIME_ID_LENGTH;

                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (eval_time_str(ptr, &db[i].te) == SUCCESS)
                    {
                       db[i].next_start_time = calc_next_time(&db[i].te);
                       db[i].time_option_type = SEND_COLLECT_TIME;
                    }
                    else
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "%s (%s %d)\n", ptr, __FILE__, __LINE__);
                    }
                 }
            else if (strncmp(p_loptions, GTS2TIFF_ID, GTS2TIFF_ID_LENGTH) == 0)
                 {
                    db[i].lfs |= GO_PARALLEL;
                 }
#ifdef _WITH_AFW2WMO
            else if (strncmp(p_loptions, AFW2WMO_ID, AFW2WMO_ID_LENGTH) == 0)
                 {
                    db[i].lfs |= DO_NOT_LINK_FILES;
                 }
#endif /* _WITH_AFW2WMO */
            else if (strncmp(p_loptions, EXTRACT_ID, EXTRACT_ID_LENGTH) == 0)
                 {
                    db[i].lfs |= GO_PARALLEL;
                    db[i].lfs |= SPLIT_FILE_LIST;
                 }
            NEXT(p_loptions);
         }
      }
      else
      {
         db[i].loptions = NULL;
      }

      /* Store number of standard options */
      db[i].no_of_soptions = atoi((int)(p_ptr[i].ptr[7]) + p_offset);

      /* Store pointer to first local option */
      if (db[i].no_of_soptions > 0)
      {
         db[i].soptions = (int)(p_ptr[i].ptr[8]) + p_offset;
      }
      else
      {
         db[i].soptions = NULL;
      }

      /* Store pointer to recipient */
      db[i].recipient = (int)(p_ptr[i].ptr[9]) + p_offset;

      /* Extract hostname and position in FSA for each recipient */
      if ((p_hostname = get_hostname(db[i].recipient)) == NULL)
      {
         /*
          * If this should happen then something is really wrong.
          */
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not extract hostname. (%s %d)\n",
                   __FILE__, __LINE__);
         p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
         free(db); free(de);
         free(tmp_ptr);
         unmap_data(jd_fd, jd);
         exit(INCORRECT);
      }

      /* Remove address switching information from hostname */
      host_ptr = p_hostname;
      while ((*host_ptr != STATIC_TOGGLE_OPEN) &&
             (*host_ptr != AUTO_TOGGLE_OPEN) &&
             (*host_ptr != '\0'))
      {
         host_ptr++;
      }
      if ((*host_ptr == STATIC_TOGGLE_OPEN) || (*host_ptr == AUTO_TOGGLE_OPEN))
      {
         *host_ptr = '\0';
      }
      t_hostname(p_hostname, db[i].host_alias);

      if ((db[i].position = get_host_position(fsa,
                                              db[i].host_alias,
                                              no_of_hosts)) < 0)
      {
         /* This should be impossible !(?) */
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not locate host %s in FSA. (%s %d)\n",
                   db[i].host_alias, __FILE__, __LINE__);
      }
      (void)strcpy(db[i].paused_dir, db[i].dir);
      (void)strcat(db[i].paused_dir, "/.");
      (void)strcat(db[i].paused_dir, db[i].host_alias);

      /* Now lets determine what kind of protocol we have here. */
      p_sheme = db[i].recipient;
      while ((*p_sheme != ':') && (*p_sheme != '\0'))
      {
         p_sheme++;
      }
      if (*p_sheme != ':')
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Impossible, could not determine the sheme! (%s %d)\n",
                   __FILE__, __LINE__);
         p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
         free(db); free(de);
         free(tmp_ptr);
         unmap_data(jd_fd, jd);
         exit(INCORRECT);
      }
      *p_sheme = '\0';
      if (strcmp(db[i].recipient, FTP_SHEME) != 0)
      {
         if (strcmp(db[i].recipient, LOC_SHEME) != 0)
         {
#ifdef _WITH_WMO_SUPPORT
            if (strcmp(db[i].recipient, WMO_SHEME) != 0)
            {
#endif
#ifdef _WITH_MAP_SUPPORT
               if (strcmp(db[i].recipient, MAP_SHEME) != 0)
               {
#endif
                  if (strcmp(db[i].recipient, SMTP_SHEME) != 0)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Unknown sheme <%s>. (%s %d)\n",
                               db[i].recipient, __FILE__, __LINE__);
                     p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
                     free(db); free(de);
                     free(tmp_ptr);
                     unmap_data(jd_fd, jd);
                     exit(INCORRECT);
                  }
                  else
                  {
                     db[i].protocol = SMTP;
                  }
#ifdef _WITH_MAP_SUPPORT
               }
               else
               {
                  db[i].protocol = MAP;
               }
#endif
#ifdef _WITH_WMO_SUPPORT
            }
            else
            {
               db[i].protocol = WMO;
            }
#endif
         }
         else
         {
            db[i].protocol = LOC;
         }
      }
      else
      {
         db[i].protocol = FTP;
      }
      *p_sheme = ':';

      lookup_job_id(&db[i], &jid_number);
      if (db[i].time_option_type == SEND_COLLECT_TIME)
      {
         enter_time_job(i);
      }
   } /* for (i = 0; i < no_of_jobs; i++)  */

   if (no_of_time_jobs > 1)
   {
      sort_time_job();
   }

   write_numbers(jid_number);

   /* Write job list file. */
   write_current_msg_list(no_of_jobs);

   /* Remove old time job directories */
   check_old_time_jobs(no_of_jobs);

   /* Free all memory */
   free(tmp_ptr);
   free(gotcha);
   unmap_data(dnb_fd, dnb);
   unmap_data(jd_fd, jd);
   p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;

   /* Tell FD we have updated the FSA and message list. */
   if ((i = send_cmd(FSA_UPDATED, fd_cmd_fd)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to send update command to FD : %s (%s %d)\n",
                strerror(-i), __FILE__, __LINE__);
   }
   if (p_afd_status->start_time == 0)
   {
      p_afd_status->start_time = time(NULL);
   }

   if (not_in_same_file_system > 1)
   {
      (void)rec(sys_log_fd, INFO_SIGN,
                "%d directories not in the same filesystem as AFD. (%s %d)\n",
                not_in_same_file_system, __FILE__, __LINE__);
   }
   else if (not_in_same_file_system == 1)
        {
           (void)rec(sys_log_fd, INFO_SIGN,
                     "One directory not in the same filesystem as AFD. (%s %d)\n",
                     __FILE__, __LINE__);
        }

#ifdef _TEST_FILE_TABLE
   for (i = 0; i < no_of_local_dirs; i++)
   {
      (void)fprintf(stdout, "Directory entry %d : %s\n", i, de[i].dir);
      for (j = 0; j < de[i].nfg; j++)
      {
         (void)fprintf(stdout, "\t%d:\t", j);
         for (k = 0; k < de[i].fme[j].nfm; k++)
         {
            (void)fprintf(stdout, "%s ", de[i].fme[j].file_mask[k]);
         }
         (void)fprintf(stdout, "(%d)\n", de[i].fme[j].nfm);
         (void)fprintf(stdout, "\t\tNumber of destinations = %d\n", de[i].fme[j].dest_count);
      }
      (void)fprintf(stdout, "\tNumber of file groups  = %d\n", de[i].nfg);
      if (de[i].all_flag == YES)
      {
         (void)fprintf(stdout, "\tAll files selected    = YES\n");
      }
      else
      {
         (void)fprintf(stdout, "\tAll files selected    = NO\n");
      }
   }
#endif

   return(no_of_jobs);
}


/*++++++++++++++++++++++++ write_current_msg_list() +++++++++++++++++++++*/
static void
write_current_msg_list(int no_of_jobs)
{
   int    i,
          *int_buf,
          fd;
   size_t buf_size;
   char   current_msg_list_file[MAX_PATH_LENGTH];

   (void)strcpy(current_msg_list_file, p_work_dir);
   (void)strcat(current_msg_list_file, FIFO_DIR);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);

   /* Overwrite current message list file. */
   if ((fd = open(current_msg_list_file, (O_WRONLY | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR))) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                current_msg_list_file, strerror(errno),
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   lock_region_w(fd, 0);

   /* Create buffer to write ID's in one hunk. */
   buf_size = (no_of_jobs + 1) * sizeof(int);
   if ((int_buf = malloc(buf_size)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   int_buf[0] = no_of_jobs;
   for (i = 0; i < no_of_jobs; i++)
   {
      int_buf[i + 1] = db[i].job_id;
   }

   if (write(fd, int_buf, buf_size) != buf_size)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to write() to %s : %s (%s %d)\n",
                current_msg_list_file, strerror(errno),
                __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   free(int_buf);

   return;
}


/*+++++++++++++++++++++++++++ write_numbers() +++++++++++++++++++++++++++*/
static void
write_numbers(int jid_number)
{
   size_t length;
   char   str_number[MAX_INT_LENGTH];

   /* Save current JID number */
   length = sprintf(str_number, "%d", jid_number);
   if (lseek(jid_fd, SEEK_SET, 0) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to lseek() in JID number file : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      if (write(jid_fd, str_number, length) != length)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to write() highest JID number : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }
   if (close(jid_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}


/*++++++++++++++++++++++++++++ unmap_data() +++++++++++++++++++++++++++++*/
static void
unmap_data(int fd, void *area)
{
   struct stat stat_buf;

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "fstat() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char *ptr = (char *)area - AFD_WORD_OFFSET;

      if (msync(ptr, stat_buf.st_size, MS_ASYNC) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "msync() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (munmap(ptr, stat_buf.st_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "munmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}
