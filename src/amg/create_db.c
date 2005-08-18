/*
 *  create_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int create_db(void)
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
 **   04.01.2001 H.Kiehl Check for duplicate paused directories.
 **   05.04.2001 H.Kiehl Remove O_TRUNC.
 **   25.04.2001 H.Kiehl Rename files from pool directory if there is only
 **                      one job for this directory.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   30.10.2002 H.Kiehl Take afd_file_dir as the directory to determine
 **                      if we can just move a file.
 **   28.02.2003 H.Kiehl Added option grib2wmo.
 **   13.07.2003 H.Kiehl Option "age-limit" is now used by AMG as well.
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
#ifdef HAVE_MMAP
#include <sys/mman.h>              /* msync(), munmap()                  */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <unistd.h>                /* fork(), rmdir(), fstat()           */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        no_fork_jobs,
                                  no_of_hosts,
                                  no_of_local_dirs,
                                  no_of_time_jobs,
#ifdef _TEST_FILE_TABLE
                                  no_of_local_dirs,/* No. of directories */
                                                   /* in DIR_CONFIG file */
                                                   /* that are local.    */
#endif /* _TEST_FILE_TABLE */
                                  *time_job_list;
extern unsigned int               default_age_limit;
extern off_t                      amg_data_size;
extern char                       *afd_file_dir,
                                  *p_mmap,
                                  *p_work_dir;
extern struct fork_job_data       *fjd;
extern struct directory_entry     *de;
extern struct instant_db          *db;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;

/* Global variables */
int                               dnb_fd,
                                  fmd_fd = -1, /* File Mask Database fd  */
                                  jd_fd,
                                  *no_of_passwd,
                                  pwb_fd,
                                  *no_of_dir_names,
                                  *no_of_job_ids;
off_t                             fmd_size = 0;
char                              *fmd,
#ifdef WITH_GOTCHA_LIST
                                  *gotcha,
#endif
                                  msg_dir[MAX_PATH_LENGTH],
                                  *p_msg_dir;
struct job_id_data                *jd;
struct dir_name_buf               *dnb;
struct passwd_buf                 *pwb;

/* Local function prototypes. */
static void                       write_current_msg_list(int);

/* #define _WITH_JOB_LIST_TEST 1 */
#define POS_STEP_SIZE      20
#define FORK_JOB_STEP_SIZE 20

/*+++++++++++++++++++++++++++++ create_db() +++++++++++++++++++++++++++++*/
int
create_db(void)
{
   int             amg_data_fd,
                   exec_flag,
                   exec_flag_dir,
                   i,
                   j,
#ifdef _TEST_FILE_TABLE
                   k,
#endif
                   not_in_same_file_system = 0,
                   one_job_only_dir = 0,
                   size,
                   dir_counter = 0,
                   no_of_jobs;
   unsigned int    jid_number;
   dev_t           ldv;               /* local device number (filesystem) */
   char            amg_data_file[MAX_PATH_LENGTH],
                   *ptr,
                   *p_sheme,
                   *tmp_ptr,
                   *p_offset,
                   *p_loptions,
                   *p_file,
                   real_hostname[MAX_REAL_HOSTNAME_LENGTH];
   struct p_array  *p_ptr;
   struct stat     stat_buf;

   if (fjd != NULL)
   {
      free(fjd);
      fjd = NULL;
      no_fork_jobs = 0;
   }

   /* Set flag to indicate that we are writting in JID structure */
   if ((p_afd_status->amg_jobs & WRITTING_JID_STRUCT) == 0)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
   }

   /* Get device number for working directory */
   if (stat(afd_file_dir, &stat_buf) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", afd_file_dir, strerror(errno));
      exit(INCORRECT);
   }
   ldv = stat_buf.st_dev;

   (void)sprintf(amg_data_file, "%s%s%s", p_work_dir, FIFO_DIR, AMG_DATA_FILE);
   if ((amg_data_fd = open(amg_data_file, O_RDWR)) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s", amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
   if (fstat(amg_data_fd, &stat_buf) == -1)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s", amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
   amg_data_size = stat_buf.st_size;
#ifdef HAVE_MMAP
   if ((p_mmap = mmap(0, amg_data_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, amg_data_fd, 0)) == (caddr_t) -1)
#else
   if ((p_mmap = mmap_emu(0, amg_data_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, amg_data_file, 0)) == (caddr_t) -1)
#endif
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() `%s' : %s", amg_data_file, strerror(errno));
      exit(INCORRECT);
   }
   if (close(amg_data_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   ptr = p_mmap;

   /* First get the no of jobs */
   no_of_jobs = *(int *)ptr;
   ptr += sizeof(int);

   /* Allocate some memory to store instant database. */
   if ((db = malloc(no_of_jobs * sizeof(struct instant_db))) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   init_job_data();

#ifdef WITH_GOTCHA_LIST
   /* Allocate space for the gotchas. */
   size = ((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
           JOB_ID_DATA_STEP_SIZE * sizeof(char);
   if ((gotcha = malloc(size)) == NULL)
   {
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory : %s", strerror(errno));
      exit(INCORRECT);
   }
# ifdef _WITH_JOB_LIST_TEST
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
# else
   (void)memset(gotcha, NO, size);
# endif
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
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory : %s", strerror(errno));
      exit(INCORRECT);
   }
   p_ptr = (struct p_array *)tmp_ptr;
   memcpy(p_ptr, ptr, size);
   p_offset = ptr + size;

   de[0].nfg         = 0;
   de[0].fme         = NULL;
   de[0].flag        = 0;
   de[0].dir         = p_ptr[0].ptr[1] + p_offset;
   de[0].alias       = p_ptr[0].ptr[2] + p_offset;
   if ((de[0].fra_pos = lookup_fra_pos(de[0].alias)) == INCORRECT)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to locate dir alias `%s' for directory `%s'",
                 de[0].alias, de[0].dir);
      exit(INCORRECT);
   }
   de[0].dir_id      = dnb[fra[de[0].fra_pos].dir_pos].dir_id;
   de[0].mod_time    = -1;
   de[0].search_time = 0;
   if (fra[de[0].fra_pos].fsa_pos != -1)
   {
      size_t length;

      length = strlen(de[0].dir) + 1 + 1 +
               strlen(fsa[fra[de[0].fra_pos].fsa_pos].host_alias) + 1;
      if ((de[0].paused_dir = malloc(length)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(errno));
         exit(INCORRECT);
      }
      (void)sprintf(de[0].paused_dir, "%s/.%s", de[0].dir,
                    fsa[fra[de[0].fra_pos].fsa_pos].host_alias);
   }
   else
   {
      de[0].paused_dir = NULL;
   }
   if (stat(de[0].dir, &stat_buf) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", de[0].dir, strerror(errno));
      p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
      if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
      {
         p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
      }
      free(db); free(de);
      free(tmp_ptr);
      unmap_data(jd_fd, (void *)&jd);
      exit(INCORRECT);
   }
   if (stat_buf.st_dev != ldv)
   {
      not_in_same_file_system++;
   }
   else
   {
      de[0].flag |= IN_SAME_FILESYSTEM;
   }
   if (((no_of_jobs - 1) == 0) ||
       (de[0].dir != (p_ptr[1].ptr[1] + p_offset)))
   {
      de[0].flag |= RENAME_ONE_JOB_ONLY;
      one_job_only_dir++;
   }

   exec_flag_dir = 0;
   for (i = 0; i < no_of_jobs; i++)
   {
      exec_flag = 0;

      /* Store DIR_CONFIG ID. */
      db[i].dir_config_id = (unsigned int)strtoul(p_ptr[i].ptr[10] + p_offset, NULL, 16);

      /* Store directory pointer */
      db[i].dir = p_ptr[i].ptr[1] + p_offset;

      /* Store priority */
      db[i].priority = *(p_ptr[i].ptr[0] + p_offset);

      /* Store number of files to be send */
      db[i].no_of_files = atoi(p_ptr[i].ptr[3] + p_offset);

      /* Store pointer to first file (filter) */
      db[i].files = p_ptr[i].ptr[4] + p_offset;

      db[i].fra_pos = de[dir_counter].fra_pos;

      /*
       * Store all file names of one directory into one array. This
       * is necessary so we can specify overlapping wild cards in
       * different file sections for one directory section.
       */
      if ((i > 0) && (db[i].dir != db[i - 1].dir))
      {
         if ((de[dir_counter].flag & DELETE_ALL_FILES) ||
             ((de[dir_counter].flag & IN_SAME_FILESYSTEM) &&
              (exec_flag_dir == 0)))
         {
            if ((fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC) == 0)
            {
               fra[de[dir_counter].fra_pos].dir_flag |= LINK_NO_EXEC;
            }
         }
         else
         {
            if (fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC)
            {
               fra[de[dir_counter].fra_pos].dir_flag ^= LINK_NO_EXEC;
            }
         }
         exec_flag_dir = 0;
         dir_counter++;
         if (dir_counter >= no_of_local_dirs)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Aaarghhh, dir_counter (%d) >= no_of_local_dirs (%d)!?",
                       dir_counter, no_of_local_dirs);
            exit(INCORRECT);
         }
         de[dir_counter].dir           = db[i].dir;
         de[dir_counter].alias         = p_ptr[i].ptr[2] + p_offset;
         if ((de[dir_counter].fra_pos = lookup_fra_pos(de[dir_counter].alias)) == INCORRECT)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to locate dir alias `%s' for directory `%s'",
                       de[dir_counter].alias, de[dir_counter].dir);
            exit(INCORRECT);
         }
         de[dir_counter].dir_id        = dnb[fra[de[dir_counter].fra_pos].dir_pos].dir_id;
         de[dir_counter].nfg           = 0;
         de[dir_counter].fme           = NULL;
         de[dir_counter].flag          = 0;
         de[dir_counter].mod_time      = -1;
         de[dir_counter].search_time   = 0;
         if (fra[de[dir_counter].fra_pos].fsa_pos != -1)
         {
            size_t length;

            length = strlen(de[dir_counter].dir) + 1 + 1 +
                     strlen(fsa[fra[de[dir_counter].fra_pos].fsa_pos].host_alias) + 1;
            if ((de[dir_counter].paused_dir = malloc(length)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          length, strerror(errno));
               exit(INCORRECT);
            }
            (void)sprintf(de[dir_counter].paused_dir, "%s/.%s",
                          de[dir_counter].dir,
                          fsa[fra[de[dir_counter].fra_pos].fsa_pos].host_alias);
         }
         else
         {
            de[dir_counter].paused_dir = NULL;
         }
         if (stat(db[i].dir, &stat_buf) < 0)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to stat() `%s' : %s",
                       db[i].dir, strerror(errno));
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
            {
               p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
            }
            free(db); free(de);
            free(tmp_ptr);
            unmap_data(jd_fd, (void *)&jd);
            exit(INCORRECT);
         }
         if (stat_buf.st_dev != ldv)
         {
            not_in_same_file_system++;
         }
         else
         {
            de[dir_counter].flag |= IN_SAME_FILESYSTEM;
         }
         if ((i == (no_of_jobs - 1)) ||
             (db[i].dir != (p_ptr[i + 1].ptr[1] + p_offset)))
         {
            de[dir_counter].flag |= RENAME_ONE_JOB_ONLY;
            one_job_only_dir++;
         }
      }
      db[i].dir_id = de[dir_counter].dir_id;

      /*
       * Check if this directory is in the same file system
       * as file directory of the AFD. If this is not the case
       * lets fork when we copy.
       */
      db[i].lfs = 0;
      if (stat_buf.st_dev == ldv)
      {
         db[i].lfs |= IN_SAME_FILESYSTEM;
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

            if ((de[dir_counter].fme = realloc(de[dir_counter].fme, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to realloc() %d bytes : %s",
                          new_size, strerror(errno));
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
               {
                  p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
               }
               unmap_data(jd_fd, (void *)&jd);
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
            (void)memset(ptr_start, 0,
                         (FG_BUFFER_STEP_SIZE * sizeof(struct file_mask_entry)));
         }
         p_file = db[i].files;
         de[dir_counter].fme[de[dir_counter].nfg].nfm = db[i].no_of_files;
         if ((de[dir_counter].fme[de[dir_counter].nfg].file_mask = malloc((db[i].no_of_files * sizeof(char *)))) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       db[i].no_of_files * sizeof(char *), strerror(errno));
            p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
            if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
            {
               p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
            }
            unmap_data(jd_fd, (void *)&jd);
            exit(INCORRECT);
         }
         de[dir_counter].fme[de[dir_counter].nfg].dest_count = 0;
         for (j = 0; j < db[i].no_of_files; j++)
         {
            de[dir_counter].fme[de[dir_counter].nfg].file_mask[j] = p_file;
            if ((p_file[0] == '*') && (p_file[1] == '\0'))
            {
               de[dir_counter].flag |= ALL_FILES;
            }
            NEXT(p_file);
         }
         while (*p_file != '\0')
         {
            p_file++;
         }
         lookup_file_mask_id(&db[i], p_file - db[i].files);
         if ((de[dir_counter].flag & ALL_FILES) && (db[i].no_of_files > 1))
         {
            p_file = db[i].files;
            for (j = 0; j < db[i].no_of_files; j++)
            {
               if (p_file[0] == '!')
               {
                  de[dir_counter].flag ^= ALL_FILES;
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
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to realloc() %d bytes : %s",
                          new_size, strerror(errno));
               p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
               if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
               {
                  p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
               }
               unmap_data(jd_fd, (void *)&jd);
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
                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                               "Failed to realloc() %d bytes : %s",
                               new_size, strerror(errno));
                    p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
                    if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
                    {
                       p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
                    }
                    unmap_data(jd_fd, (void *)&jd);
                    exit(INCORRECT);
                 }
              }
              de[dir_counter].fme[de[dir_counter].nfg - 1].pos[de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count] = i;
              de[dir_counter].fme[de[dir_counter].nfg - 1].dest_count++;
              db[i].file_mask_id = db[i - 1].file_mask_id;
           }

      /* Store number of local options */
      db[i].no_of_loptions = atoi(p_ptr[i].ptr[5] + p_offset);
      db[i].next_start_time = 0;
      db[i].time_option_type = NO_TIME;

      /* Store pointer to first local option. */
      if (db[i].no_of_loptions > 0)
      {
         db[i].loptions = p_ptr[i].ptr[6] + p_offset;

         /*
          * Because extracting bulletins from files can take quit a
          * while, make shure that we fork. We can do this by setting
          * the lfs flag to GO_PARALLEL.
          */
         p_loptions = db[i].loptions;
         for (j = 0; j < db[i].no_of_loptions; j++)
         {
            if (strncmp(p_loptions, DELETE_ID, DELETE_ID_LENGTH) == 0)
            {
               db[i].lfs = DELETE_ALL_FILES;
               break;
            }
            if (strncmp(p_loptions, EXEC_ID, EXEC_ID_LENGTH) == 0)
            {
               db[i].lfs |= GO_PARALLEL;
               db[i].lfs |= DO_NOT_LINK_FILES;
               exec_flag = 1;
               exec_flag_dir = 1;
            }
            /*
             * NOTE: TIME_NO_COLLECT_ID option __must__ be checked before
             *       TIME_ID. Since both start with time and TIME_ID only
             *       consists only of the word time.
             */
            else if (strncmp(p_loptions, TIME_NO_COLLECT_ID,
                             TIME_NO_COLLECT_ID_LENGTH) == 0)
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
                       system_log(ERROR_SIGN, __FILE__, __LINE__, "%s", ptr);
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
                       db[i].next_start_time = calc_next_time(&db[i].te,
                                                              time(NULL));
                       db[i].time_option_type = SEND_COLLECT_TIME;
                    }
                    else
                    {
                       system_log(ERROR_SIGN, __FILE__, __LINE__, "%s", ptr);
                    }
                 }
            else if (strncmp(p_loptions, GTS2TIFF_ID, GTS2TIFF_ID_LENGTH) == 0)
                 {
                    db[i].lfs |= GO_PARALLEL;
                 }
            else if (strncmp(p_loptions, GRIB2WMO_ID, GRIB2WMO_ID_LENGTH) == 0)
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

      /*
       * If we have RENAME_ONE_JOB_ONLY and there are options that force
       * us to link the file, we cannot just rename the files! We must
       * copy them. Thus we must remove the flag when this is the case.
       */
      if ((i == 0) || ((i > 0) && (db[i].files != db[i - 1].files)))
      {
         if ((de[dir_counter].flag & RENAME_ONE_JOB_ONLY) &&
             (db[i].lfs & DO_NOT_LINK_FILES))
         {
            one_job_only_dir--;
            de[dir_counter].flag &= ~RENAME_ONE_JOB_ONLY;
         }
      }

      /* Store number of standard options. */
      db[i].no_of_soptions = atoi(p_ptr[i].ptr[7] + p_offset);

      /* Store pointer to first local option and age limit. */
      if (db[i].no_of_soptions > 0)
      {
         char *sptr;

         db[i].soptions = p_ptr[i].ptr[8] + p_offset;
         if ((sptr = posi(db[i].soptions, AGE_LIMIT_ID)) != NULL)
         {
            int  count = 0;
            char str_number[MAX_INT_LENGTH];

            while ((*sptr == ' ') || (*sptr == '\t'))
            {
               sptr++;
            }
            while ((*sptr != '\n') && (*sptr != '\0'))
            {
               str_number[count] = *sptr;
               count++; sptr++;
            }
            str_number[count] = '\0';
            errno = 0;
            db[i].age_limit = (unsigned int)strtoul(str_number,
                                                    (char **)NULL, 10);
            if (errno == ERANGE)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Option %s for directory `%s' out of range, resetting to default %u.",
                          AGE_LIMIT_ID, db[i].dir, default_age_limit);
               db[i].age_limit = default_age_limit;
            }
         }
         else
         {
            db[i].age_limit = default_age_limit;
         }
      }
      else
      {
         db[i].age_limit = default_age_limit;
         db[i].soptions = NULL;
      }

      /* Store pointer to recipient. */
      db[i].recipient = p_ptr[i].ptr[9] + p_offset;

      /* Extract hostname and position in FSA for each recipient. */
      if (get_hostname(db[i].recipient, real_hostname) == INCORRECT)
      {
         /*
          * If this should happen then something is really wrong.
          */
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not extract hostname.");
         p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
         if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
         {
            p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
         }
         free(db); free(de);
         free(tmp_ptr);
         unmap_data(jd_fd, (void *)&jd);
         exit(INCORRECT);
      }
      t_hostname(real_hostname, db[i].host_alias);

      if ((db[i].position = get_host_position(fsa, db[i].host_alias,
                                              no_of_hosts)) < 0)
      {
         /* This should be impossible !(?) */
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not locate host `%s' in FSA.", db[i].host_alias);
      }

      /*
       * Always check if this directory is not already specified.
       * This might help to reduce the number of directories that the
       * function check_paused_dir() has to check.
       */
      db[i].dup_paused_dir = NO;
      for (j = 0; j < i; j++)
      {
         if ((db[j].dir == db[i].dir) &&
             (CHECK_STRCMP(db[j].host_alias, db[i].host_alias) == 0))
         {
            db[i].dup_paused_dir = YES;
            break;
         }
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
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Impossible, could not determine the sheme!");
         p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
         if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
         {
            p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
         }
         free(db); free(de);
         free(tmp_ptr);
         unmap_data(jd_fd, (void *)&jd);
         exit(INCORRECT);
      }
      *p_sheme = '\0';
      if ((db[i].recipient[0] == 'f') && (db[i].recipient[1] == 't') &&
#ifdef WITH_SSL
          (db[i].recipient[2] == 'p') && ((db[i].recipient[3] == '\0') ||
          (((db[i].recipient[3] == 's') || (db[i].recipient[3] == 'S')) &&
          (db[i].recipient[4] == '\0'))))
#else
          (db[i].recipient[2] == 'p') && (db[i].recipient[3] == '\0'))
#endif
      {
         db[i].protocol = FTP;
      }
      else
      {
         if (CHECK_STRCMP(db[i].recipient, LOC_SHEME) != 0)
         {
#ifdef _WITH_SCP_SUPPORT
            if (CHECK_STRCMP(db[i].recipient, SCP_SHEME) != 0)
            {
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
               if (CHECK_STRCMP(db[i].recipient, WMO_SHEME) != 0)
               {
#endif
#ifdef _WITH_MAP_SUPPORT
                  if (CHECK_STRCMP(db[i].recipient, MAP_SHEME) != 0)
                  {
#endif
                     if ((db[i].recipient[0] == 'h') &&
                         (db[i].recipient[1] == 't') &&
                         (db[i].recipient[2] == 't') &&
                         (db[i].recipient[3] == 'p') &&
#ifdef WITH_SSL
                         ((db[i].recipient[4] == '\0') ||
                          ((db[i].recipient[4] == 's') &&
                           (db[i].recipient[5] == '\0'))))
#else
                         (db[i].recipient[4] == '\0'))
#endif
                     {
                        db[i].protocol = HTTP;
                     }
                     else
                     {
                        if ((db[i].recipient[0] == 'm') &&
                            (db[i].recipient[1] == 'a') &&
                            (db[i].recipient[2] == 'i') &&
                            (db[i].recipient[3] == 'l') &&
                            (db[i].recipient[4] == 't') &&
                            (db[i].recipient[5] == 'o') &&
#ifdef WITH_SSL
                            ((db[i].recipient[6] == '\0') ||
                             ((db[i].recipient[6] == 's') &&
                              (db[i].recipient[7] == '\0'))))
#else
                            (db[i].recipient[6] == '\0'))
#endif
                        {
                           db[i].protocol = SMTP;
                        }
                        else
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Unknown sheme <%s>.", db[i].recipient);
                           p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
                           if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
                           {
                              p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
                           }
                           free(db); free(de);
                           free(tmp_ptr);
                           unmap_data(jd_fd, (void *)&jd);
                           exit(INCORRECT);
                        }
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
#ifdef _WITH_SCP_SUPPORT
            }
            else
            {
               db[i].protocol = SCP;
            }
#endif /* _WITH_SCP_SUPPORT */
         }
         else
         {
            db[i].protocol = LOC;
         }
      }
      *p_sheme = ':';

      store_passwd(db[i].recipient, YES);
      lookup_job_id(&db[i], &jid_number);
      if (db[i].time_option_type == SEND_COLLECT_TIME)
      {
         enter_time_job(i);
      }
      if (exec_flag == 1)
      {
         if ((no_fork_jobs % FORK_JOB_STEP_SIZE) == 0)
         {
            size_t new_size;

            new_size = ((no_fork_jobs / FORK_JOB_STEP_SIZE) + 1) *
                       FORK_JOB_STEP_SIZE * sizeof(struct fork_job_data);
            if ((fjd = realloc(fjd, new_size)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Could not realloc() %d bytes : %s",
                          new_size, strerror(errno));
               exit(INCORRECT);
            }
         }
         fjd[no_fork_jobs].forks = 0;
         fjd[no_fork_jobs].job_id = db[i].job_id;
         fjd[no_fork_jobs].user_time = 0;
         fjd[no_fork_jobs].system_time = 0;
         no_fork_jobs++;
      }
   } /* for (i = 0; i < no_of_jobs; i++)  */

   if ((de[dir_counter].flag & DELETE_ALL_FILES) ||
       ((de[dir_counter].flag & IN_SAME_FILESYSTEM) && (exec_flag_dir == 0)))
   {
      if ((fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC) == 0)
      {
         fra[de[dir_counter].fra_pos].dir_flag |= LINK_NO_EXEC;
      }
   }
   else
   {
      if (fra[de[dir_counter].fra_pos].dir_flag & LINK_NO_EXEC)
      {
         fra[de[dir_counter].fra_pos].dir_flag ^= LINK_NO_EXEC;
      }
   }

   if (no_of_time_jobs > 1)
   {
      sort_time_job();
   }

   /* Check for duplicate job entries. */
   for (i = 0; i < no_of_jobs; i++)
   {
      for (j = (i + 1); j < no_of_jobs; j++)
      {
         if (db[i].job_id == db[j].job_id)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Duplicate job entries for job %x with directory %s and recipient %s!",
                       db[i].job_id, db[i].dir, db[i].recipient);
            break;
         }
      }
   }

   /* Write job list file. */
   write_current_msg_list(no_of_jobs);

   /* Remove old time job directories */
   check_old_time_jobs(no_of_jobs);

   /* Free all memory */
   free(tmp_ptr);
#ifdef WITH_GOTCHA_LIST
   free(gotcha);
#endif
   unmap_data(dnb_fd, (void *)&dnb);
   unmap_data(jd_fd, (void *)&jd);
   unmap_data(fmd_fd, (void *)&fmd);
   if (pwb != NULL)
   {
      unmap_data(pwb_fd, (void *)&pwb);
   }
   p_afd_status->amg_jobs ^= WRITTING_JID_STRUCT;
   if (p_afd_status->amg_jobs & REREADING_DIR_CONFIG)
   {
      p_afd_status->amg_jobs ^= REREADING_DIR_CONFIG;
   }

   if (p_afd_status->start_time == 0)
   {
      p_afd_status->start_time = time(NULL);
   }

   if (one_job_only_dir > 1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "%d directories with only one job and no need for linking.",
                 one_job_only_dir);
   }
   else if (one_job_only_dir == 1)
        {
           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                      "One directory with only one job.");
        }

   if (not_in_same_file_system > 1)
   {
      system_log(INFO_SIGN, __FILE__, __LINE__,
                 "%d directories not in the same filesystem as AFD.",
                 not_in_same_file_system);
   }
   else if (not_in_same_file_system == 1)
        {
           system_log(INFO_SIGN, __FILE__, __LINE__,
                      "One directory not in the same filesystem as AFD.");
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
         (void)fprintf(stdout, "\t\tNumber of destinations = %d\n",
                       de[i].fme[j].dest_count);
      }
      (void)fprintf(stdout, "\tNumber of file groups  = %d\n", de[i].nfg);
      if (de[i].flag & ALL_FILES)
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
   int          i,
                fd;
   unsigned int *int_buf;
   size_t       buf_size;
   char         current_msg_list_file[MAX_PATH_LENGTH];
   struct stat  stat_buf;

   (void)strcpy(current_msg_list_file, p_work_dir);
   (void)strcat(current_msg_list_file, FIFO_DIR);
   (void)strcat(current_msg_list_file, CURRENT_MSG_LIST_FILE);

   /* Overwrite current message list file. */
   if ((fd = open(current_msg_list_file, (O_WRONLY | O_CREAT | O_TRUNC),
                  FILE_MODE)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 current_msg_list_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef LOCK_DEBUG
   lock_region_w(fd, (off_t)0, __FILE__, __LINE__);
#else
   lock_region_w(fd, (off_t)0);
#endif

   /* Create buffer to write ID's in one hunk. */
   buf_size = (no_of_jobs + 1) * sizeof(unsigned int);
   if ((int_buf = malloc(buf_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s", buf_size, strerror(errno));
      exit(INCORRECT);
   }

   int_buf[0] = no_of_jobs;
   for (i = 0; i < no_of_jobs; i++)
   {
      int_buf[i + 1] = db[i].job_id;
   }

   if (write(fd, int_buf, buf_size) != buf_size)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to write() to `%s' : %s",
                 current_msg_list_file, strerror(errno));
      exit(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s",
                 current_msg_list_file, strerror(errno));
   }
   else
   {
      if (stat_buf.st_size > buf_size)
      {
         if (ftruncate(fd, buf_size) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to ftruncate() `%s' to %d bytes : %s",
                       current_msg_list_file, buf_size, strerror(errno));
         }
      }
   }
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   free(int_buf);

   return;
}