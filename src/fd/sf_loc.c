/*
 *  sf_loc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Deutscher Wetterdienst (DWD),
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
 **   sf_loc - copies files from one directory to another
 **
 ** SYNOPSIS
 **   sf_loc <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version        Version Number
 **       -a <age limit>   The age limit for the files being send.
 **       -A               Disable archiving of files.
 **       -o <retries>     Old/Error message and number of retries.
 **       -r               Resend from archive (job from show_olog).
 **       -t               Temp toggle.
 **
 ** DESCRIPTION
 **   sf_loc is very similar to sf_ftp only that it sends files
 **   locally (i.e. moves/copies files from one directory to another).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.03.1996 H.Kiehl Created
 **   27.01.1997 H.Kiehl Include support for output logging.
 **   18.04.1997 H.Kiehl Do a hard link when we are in the same file
 **                      system.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   12.06.1999 H.Kiehl Added option to change user and group ID.
 **   07.10.1999 H.Kiehl Added option to force a copy.
 **   09.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   03.03.2004 H.Kiehl Create target directory if it does not exist.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* getenv(), abort()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>                  /* struct timeval                 */
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), alarm()              */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables. */
int                        event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,
                           no_of_hosts,    /* This variable is not used */
                                           /* in this module.           */
                           *p_no_of_hosts = NULL,
                           fsa_id,
                           fsa_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag = OFF,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           trans_rename_blocked = NO,
                           amg_flag = NO;
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0;
#endif
long                       transfer_timeout; /* Not used [init_sf()]    */
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer;
char                       *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           *del_file_name_buffer = NULL,
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa = NULL;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions. */
static int  copy_file_mkdir(char *, char*);
static void sf_loc_exit(void),
            sig_bus(int),
            sig_segv(int),
            sig_kill(int),
            sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_BURST_2
   int              cb2_ret;
#endif
   int              counter_fd = -1,
                    exit_status = TRANSFER_SUCCESS,
                    i,
                    fd,
                    lfs,                    /* local file system */
                    files_to_send = 0,
                    ret;
   off_t            *p_file_size_buffer;
   char             *ptr,
                    *p_if_name,
                    *p_ff_name,
                    *p_source_file,
                    *p_to_name,
                    *p_file_name_buffer,
                    file_name[MAX_FILENAME_LENGTH],
                    if_name[MAX_PATH_LENGTH],
                    ff_name[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    source_file[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
# ifdef WITHOUT_FIFO_RW_SUPPORT
   int              ol_readfd = -1;
# endif
   unsigned int     *ol_job_number;
   char             *ol_data = NULL,
                    *ol_file_name;
   unsigned short   *ol_archive_name_length,
                    *ol_file_name_length,
                    *ol_unl;
   off_t            *ol_file_size;
   size_t           ol_size,
                    ol_real_size;
   clock_t          end_time = 0,
                    start_time = 0,
                    *ol_transfer_time;
   struct tms       tmsdummy;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_loc_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   files_to_send = init_sf(argc, argv, file_path, LOC_FLAG);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Inform FSA that we have are ready to copy the files. */
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
      fsa->job_status[(int)db.job_no].connect_status = LOC_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
# ifdef WITHOUT_FIFO_RW_SUPPORT
      output_log_ptrs(&ol_fd, &ol_readfd, &ol_job_number, &ol_data, &ol_file_name,
# else
      output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
# endif
                      &ol_file_name_length, &ol_archive_name_length,
                      &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                      db.host_alias, LOC);
   }
#endif

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Bursting.");
         }
#ifdef _OUTPUT_LOG
         if ((db.output_log == YES) && (ol_data == NULL))
         {
# ifdef WITHOUT_FIFO_RW_SUPPORT
            output_log_ptrs(&ol_fd, &ol_readfd, &ol_job_number, &ol_data, &ol_file_name,
# else
            output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
# endif
                            &ol_file_name_length, &ol_archive_name_length,
                            &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                            db.host_alias, LOC);
         }
#endif
      }
#endif /* _WITH_BURST_2 */
      /* If we send a lockfile, do it now. */
      if (db.lock == LOCKFILE)
      {
         /* Create lock file in directory */
         if ((fd = open(db.lock_file_name, (O_WRONLY | O_CREAT | O_TRUNC),
                        (S_IRUSR | S_IWUSR))) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to create lock file `%s' : %s",
                      db.lock_file_name, strerror(errno));
            exit(WRITE_LOCK_ERROR);
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Created lockfile to `%s'.", db.lock_file_name);
            }
         }
         if (close(fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to close() `%s' : %s",
                      db.lock_file_name, strerror(errno));
         }
      }

      /*
       * Since we do not know if the directory where the file
       * will be moved is in the same file system, lets determine
       * this by comparing the device numbers.
       */
      if ((db.special_flag & FORCE_COPY) == 0)
      {
         struct stat stat_buf;

         if (stat(file_path, &stat_buf) == 0)
         {
            dev_t ldv;               /* local device number (file system) */

            ldv = stat_buf.st_dev;
            if (stat(db.target_dir, &stat_buf) == 0)
            {
               if (stat_buf.st_dev == ldv)
               {
                  lfs = YES;
               }
               else
               {
                  lfs = NO;
               }
            }
            else if ((errno == ENOENT) && (db.special_flag & CREATE_TARGET_DIR))
                 {
                    char *error_ptr;

                    if (((ret = check_create_path(db.target_dir, 0, &error_ptr,
                                                  YES)) == CREATED_DIR) ||
                        (ret == CHOWN_ERROR))
                    {
                       trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                 "Created path `%s'", db.target_dir);
                       if (ret == CHOWN_ERROR)
                       {
                          trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to chown() of directory `%s' : %s",
                                    db.target_dir, strerror(errno));
                       }
                       if (stat(db.target_dir, &stat_buf) == 0)
                       {
                          if (stat_buf.st_dev == ldv)
                          {
                             lfs = YES;
                          }
                          else
                          {
                             lfs = NO;
                          }
                       }
                       else
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to stat() `%s' : %s",
                                    db.target_dir, strerror(errno));
                          exit(STAT_ERROR);
                       }
                    }
                    else if (ret == MKDIR_ERROR)
                         {
                            if (error_ptr != NULL)
                            {
                               *error_ptr = '\0';
                            }
                            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                      "Failed to mkdir() `%s' error : %s",
                                      db.target_dir, strerror(errno));
                         }
                    else if (ret == STAT_ERROR)
                         {
                            if (error_ptr != NULL)
                            {
                               *error_ptr = '\0';
                            }
                            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                      "Failed to stat() `%s' error : %s",
                                      db.target_dir, strerror(errno));
                         }
                    else if (ret == NO_ACCESS)
                         {
                            if (error_ptr != NULL)
                            {
                               *error_ptr = '\0';
                            }
                            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                      "Cannot access directory `%s' : %s",
                                      db.target_dir, strerror(errno));
                            ret = MOVE_ERROR;
                         }
                    else if (ret == ALLOC_ERROR)
                         {
                            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                      "Failed to allocate memory : %s",
                                      strerror(errno));
                         }
                    else if (ret == SUCCESS)
                         {
                            trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                      "Hmmm, directory does seem to be ok, so why can we not open the file!?");
                            ret = MOVE_ERROR;
                         }
                    if (ret != CREATED_DIR)
                    {
                       exit(ret);
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Failed to stat() %s : %s [%d]",
                              db.target_dir, strerror(errno),
                              (*(unsigned char *)((char *)p_no_of_hosts + 5)));
                    exit(STAT_ERROR);
                 }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to stat() %s : %s", file_path, strerror(errno));
            exit(STAT_ERROR);
         }
      }
      else
      {
         lfs = NO;
      }

      /* Prepare pointers and directory name */
      (void)strcpy(source_file, file_path);
      p_source_file = source_file + strlen(source_file);
      *p_source_file++ = '/';
      (void)strcpy(if_name, db.target_dir);
      p_if_name = if_name + strlen(if_name);
      *p_if_name++ = '/';
      *p_if_name = '\0';
      (void)strcpy(ff_name, db.target_dir);
      p_ff_name = ff_name + strlen(ff_name);
      *p_ff_name++ = '/';
      *p_ff_name = '\0';

      if ((db.lock == DOT) || (db.lock == DOT_VMS))
      {
         p_to_name = if_name;
      }
      else
      {
         p_to_name = ff_name;
      }

      /* Copy all files */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      for (i = 0; i < files_to_send; i++)
      {
         /* Get the the name of the file we want to send next */
         *p_ff_name = '\0';
         (void)strcat(ff_name, p_file_name_buffer);
         (void)strcpy(file_name, p_file_name_buffer);
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            *p_if_name = '\0';
            (void)strcat(if_name, db.lock_notation);
            (void)strcat(if_name, p_file_name_buffer);
         }
         else if (db.lock == POSTFIX)
              {
                 *p_if_name = '\0';
                 (void)strcat(if_name, p_file_name_buffer);
                 (void)strcat(if_name, db.lock_notation);
              }
         (void)strcpy(p_source_file, p_file_name_buffer);

         /* Write status to FSA? */
         (void)gsf_check_fsa();
         if (db.fsa_pos != INCORRECT)
         {
            fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
            (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                         p_file_name_buffer);
            }

         if (db.trans_rename_rule[0] != '\0')
         {
            register int k;
   
            for (k = 0; k < rule[db.trans_rule_pos].no_of_rules; k++)
            {
               if (pmatch(rule[db.trans_rule_pos].filter[k],
                          p_file_name_buffer, NULL) == 0)
               {
                  change_name(p_file_name_buffer,
                              rule[db.trans_rule_pos].filter[k],
                              rule[db.trans_rule_pos].rename_to[k],
                              p_ff_name, &counter_fd, db.job_id);
                  break;
               }
            }
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            start_time = times(&tmsdummy);
         }
#endif

         /* Here comes the BIG move .... */
         if (lfs == YES)
         {
            if (link(source_file, p_to_name) == -1)
            {
               if (errno == EEXIST)
               {
                  if ((unlink(p_to_name) == -1) && (errno != ENOENT))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to unlink() `%s' : %s",
                               p_to_name, strerror(errno));
                     exit(MOVE_ERROR);
                  }
                  else
                  {
#ifndef DO_NOT_INFORM_ABOUT_OVERWRITE
                     if (errno != ENOENT)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                  "File `%s' did already exist, removed it and linked again.",
                                  p_to_name);
                     }
#endif

                     if (link(source_file, p_to_name) == -1)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "Failed to link file `%s' to `%s' : %s",
                                  source_file, p_to_name, strerror(errno));
                        exit(MOVE_ERROR);
                     }
                  }
               }
               else if ((errno == ENOENT) &&
                        (db.special_flag & CREATE_TARGET_DIR))
                    {
                       char *p_file = p_to_name;

                       p_file += strlen(p_to_name);
                       while ((*p_file != '/') && (p_file != p_to_name))
                       {
                          p_file--;
                       }
                       if (*p_file == '/')
                       {
                          char *error_ptr;

                          *p_file = '\0';
                          if (((ret = check_create_path(p_to_name, 0, &error_ptr,
                                                        YES)) == CREATED_DIR) ||
                              (ret == CHOWN_ERROR))
                          {
                             trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                       "Created path `%s'", p_to_name);
                             if (ret == CHOWN_ERROR)
                             {
                                trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                                          "Failed to chown() of directory `%s' : %s",
                                          p_to_name, strerror(errno));
                             }
                             *p_file = '/';
                             if (link(source_file, p_to_name) == -1)
                             {
                                if (errno == EEXIST)
                                {
                                   if ((unlink(p_to_name) == -1) && (errno != ENOENT))
                                   {
                                      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                "Failed to unlink() `%s' : %s",
                                                p_to_name, strerror(errno));
                                      exit(MOVE_ERROR);
                                   }
                                   else
                                   {
#ifndef DO_NOT_INFORM_ABOUT_OVERWRITE
                                      if (errno != ENOENT)
                                      {
                                         trans_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                                   "File `%s' did already exist, removed it and linked again.",
                                                   p_to_name);
                                      }
#endif

                                      if (link(source_file, p_to_name) == -1)
                                      {
                                         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                                   "Failed to link file `%s' to `%s' : %s",
                                                   source_file, p_to_name, strerror(errno));
                                         exit(MOVE_ERROR);
                                      }
                                   }
                                }
                                else
                                {
                                   trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                             "Failed to link file `%s' to `%s' : %s",
                                             source_file, p_to_name, strerror(errno));
                                   exit(MOVE_ERROR);
                                }
                             }
                          }
                          else if (ret == MKDIR_ERROR)
                               {
                                  if (error_ptr != NULL)
                                  {
                                     *error_ptr = '\0';
                                  }
                                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                            "Failed to mkdir() `%s' error : %s",
                                            p_to_name, strerror(errno));
                               }
                          else if (ret == STAT_ERROR)
                               {
                                  if (error_ptr != NULL)
                                  {
                                     *error_ptr = '\0';
                                  }
                                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                            "Failed to stat() `%s' error : %s",
                                            p_to_name, strerror(errno));
                               }
                          else if (ret == NO_ACCESS)
                               {
                                  if (error_ptr != NULL)
                                  {
                                     *error_ptr = '\0';
                                  }
                                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                            "Cannot access directory `%s' : %s",
                                            p_to_name, strerror(errno));
                                  ret = MOVE_ERROR;
                               }
                          else if (ret == ALLOC_ERROR)
                               {
                                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                            "Failed to allocate memory : %s",
                                            strerror(errno));
                               }
                          else if (ret == SUCCESS)
                               {
                                  trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                            "Hmmm, directory does seem to be ok, so why can we not open the file!?");
                                  ret = MOVE_ERROR;
                               }
                          if (ret != CREATED_DIR)
                          {
                             exit(ret);
                          }
                       }
                       else
                       {
                          *p_file = '/';
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to link file `%s' to `%s' : %s",
                                    source_file, p_to_name, strerror(errno));
                          exit(MOVE_ERROR);
                       }
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "Failed to link file `%s' to `%s' : %s",
                                 source_file, p_to_name, strerror(errno));
                       exit(MOVE_ERROR);
                    }
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Linked file `%s' to `%s'.",
                               source_file, p_to_name);
               }
            }
         }
         else
         {
            if ((ret = copy_file_mkdir(source_file, p_to_name)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to copy file `%s' to `%s'",
                         source_file, p_to_name);
               exit(ret);
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Copied file `%s' to `%s'.",
                               source_file, p_to_name);
               }
            }
         }

         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            if (db.lock == DOT_VMS)
            {
               (void)strcat(ff_name, DOT_NOTATION);
            }
            if (rename(if_name, ff_name) == -1)
            {
               if ((errno == ENOENT) &&
                   (db.special_flag & CREATE_TARGET_DIR))
               {
                  char *p_file = ff_name;

                  p_file += strlen(ff_name);
                  while ((*p_file != '/') && (p_file != ff_name))
                  {
                     p_file--;
                  }
                  if (*p_file == '/')
                  {
                     char *error_ptr;

                     *p_file = '\0';
                     if (((ret = check_create_path(ff_name, 0, &error_ptr,
                                                   YES)) == CREATED_DIR) ||
                         (ret == CHOWN_ERROR))
                     {
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                  "Created path `%s'", ff_name);
                        if (ret == CHOWN_ERROR)
                        {
                           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                                     "Failed to chown() of directory `%s' : %s",
                                     ff_name, strerror(errno));
                        }
                        *p_file = '/';
                        if (rename(if_name, ff_name) == -1)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                     "Failed to rename() file `%s' to `%s' : %s",
                                     if_name, ff_name, strerror(errno));
                           exit(RENAME_ERROR);
                        }
                     }
                     else if (ret == MKDIR_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "Failed to mkdir() `%s' error : %s",
                                       ff_name, strerror(errno));
                          }
                     else if (ret == STAT_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "Failed to stat() `%s' error : %s",
                                       ff_name, strerror(errno));
                          }
                     else if (ret == NO_ACCESS)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "Cannot access directory `%s' : %s",
                                       ff_name, strerror(errno));
                             ret = MOVE_ERROR;
                          }
                     else if (ret == ALLOC_ERROR)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                       "Failed to allocate memory : %s",
                                       strerror(errno));
                          }
                     else if (ret == SUCCESS)
                          {
                             trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                       "Hmmm, directory does seem to be ok, so why can we not open the file!?");
                             ret = MOVE_ERROR;
                          }
                     if (ret != CREATED_DIR)
                     {
                        exit(ret);
                     }
                  }
                  else
                  {
                     *p_file = '/';
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to rename() file `%s' to `%s' : %s",
                               if_name, ff_name, strerror(errno));
                     exit(RENAME_ERROR);
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to rename() file `%s' to `%s' : %s",
                            if_name, ff_name, strerror(errno));
                  exit(RENAME_ERROR);
               }
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Renamed file `%s' to `%s'.", if_name, ff_name);
               }
            }
            if (db.lock == DOT_VMS)
            {
               /* Take away the dot at the end */
               ptr = ff_name + strlen(ff_name) - 1;
               *ptr = '\0';
            }
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            end_time = times(&tmsdummy);
         }
#endif

         if (db.special_flag & CHANGE_PERMISSION)
         {
            if (chmod(ff_name, db.chmod) == -1)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to chmod() file `%s' : %s",
                         ff_name, strerror(errno));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Changed permission of file `%s' to %d",
                               ff_name, db.chmod);
               }
            }
         } /* if (db.special_flag & CHANGE_PERMISSION) */

         if (db.special_flag & CHANGE_UID_GID)
         {
            if (chown(ff_name, db.user_id, db.group_id) == -1)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to chown() of file `%s' : %s",
                         ff_name, strerror(errno));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Changed owner of file `%s' to %d:%d.",
                               ff_name, db.user_id, db.group_id);
               }
            }
         } /* if (db.special_flag & CHANGE_UID_GID) */

         /* Tell FSA we have copied a file !!!! */
         (void)gsf_check_fsa();
         if (db.fsa_pos != INCORRECT)
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
            fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
            fsa->job_status[(int)db.job_no].no_of_files_done++;
            fsa->job_status[(int)db.job_no].file_size_done += *p_file_size_buffer;
            fsa->job_status[(int)db.job_no].file_size_in_use = 0;
            fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

            /* Total file counter. */
            fsa->total_file_counter -= 1;
#ifdef _VERIFY_FSA
            if (fsa->total_file_counter < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file counter for host %s less then zero. Correcting to %d.",
                          fsa->host_dsp_name, files_to_send - (i + 1));
               fsa->total_file_counter = files_to_send - (i + 1);
            }
#endif

            /* Total file size */
            fsa->total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
            if (fsa->total_file_size < 0)
            {
               int   k;
               off_t *tmp_ptr = p_file_size_buffer;

               tmp_ptr++;
               fsa->total_file_size = 0;
               for (k = (i + 1); k < files_to_send; k++)
               {
                  fsa->total_file_size += *tmp_ptr;
               }
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                          "Total file size for host %s overflowed. Correcting to %ld.",
# else
                          "Total file size for host %s overflowed. Correcting to %lld.",
# endif
                          fsa->host_dsp_name, (pri_off_t)fsa->total_file_size);
            }
            else if ((fsa->total_file_counter == 0) &&
                     (fsa->total_file_size > 0))
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "fc for host %s is zero but fs is not zero. Correcting.",
                               fsa->host_dsp_name);
                    fsa->total_file_size = 0;
                 }
#endif

            /* File counter done */
            fsa->file_counter_done += 1;

            /* Number of bytes send */
            fsa->bytes_send += *p_file_size_buffer;
            fsa->job_status[(int)db.job_no].bytes_send += *p_file_size_buffer;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
         }

#ifdef _WITH_TRANS_EXEC
         if (db.special_flag & TRANS_EXEC)
         {
            trans_exec(file_path, ff_name, p_file_name_buffer);
         }
#endif /* _WITH_TRANS_EXEC */


         /* Now archive file if necessary */
         if ((db.archive_time > 0) &&
             (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
         {
            /*
             * By telling the function archive_file() that this
             * is the first time to archive a file for this job
             * (in struct p_db) it does not always have to check
             * whether the directory has been created or not. And
             * we ensure that we do not create duplicate names
             * when adding db.archive_time to msg_name.
             */
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to archive file `%s'", file_name);

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if (unlink(source_file) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file `%s' after copying it successfully : %s",
                             source_file, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name + db.unl,
                                                                    "%s%c/%s",
                                                                    p_file_name_buffer,
                                                                    SEPARATOR_CHAR,
                                                                    ff_name) +
                                                                    db.unl;
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = 0;
                  ol_real_size = *ol_file_name_length + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Archived file `%s'.", file_name);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name + db.unl,
                                                                    "%s%c/%s",
                                                                    p_file_name_buffer,
                                                                    SEPARATOR_CHAR,
                                                                    ff_name) +
                                                                    db.unl;
                  }
                  else
                  {
                     (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                     ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                     ol_file_name[*ol_file_name_length + 1] = '\0';
                     (*ol_file_name_length)++;
                  }
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
                  *ol_unl = db.unl;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = (unsigned short)strlen(&ol_file_name[*ol_file_name_length + 1]);
                  ol_real_size = *ol_file_name_length +
                                 *ol_archive_name_length + 1 + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
         }
         else
         {
            /* Delete the file we just have copied */
            if (unlink(source_file) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file %s after copying it successfully : %s",
                          source_file, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               if (db.trans_rename_rule[0] != '\0')
               {
                  *ol_file_name_length = (unsigned short)sprintf(ol_file_name + db.unl,
                                                                 "%s%c/%s",
                                                                 p_file_name_buffer,
                                                                 SEPARATOR_CHAR,
                                                                 ff_name) +
                                                                 db.unl;
               }
               else
               {
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
               }
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa->job_status[(int)db.job_no].job_id;
               *ol_unl = db.unl;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = 0;
               ol_real_size = *ol_file_name_length + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
            }
#endif
         }

         /*
          * After each successful transfer set error
          * counter to zero, so that other jobs can be
          * started.
          */
         if (fsa->error_counter > 0)
         {
            int  fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                 readfd,
#endif
                 j;
            char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
            fsa->error_counter = 0;

            /*
             * Wake up FD!
             */
            (void)sprintf(fd_wake_up_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
            if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
            if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to open() FIFO %s : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
            else
            {
               if (write(fd, "", 1) != 1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to write() to FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (close(readfd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
#endif
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
            }

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa->allowed_transfers; j++)
            {
               if ((j != db.job_no) &&
                   (fsa->job_status[j].connect_status == NOT_WORKING))
               {
                  fsa->job_status[j].connect_status = DISCONNECT;
               }
            }
            fsa->error_history[0] = 0;
            fsa->error_history[1] = 0;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_EC);
#endif

            /*
             * Since we have successfully transmitted a file, no need to
             * have the queue stopped anymore.
             */
            if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
            {
               char *sign;

               fsa->host_status ^= AUTO_PAUSE_QUEUE_STAT;
               if (fsa->host_status & HOST_ERROR_EA_STATIC)
               {
                  fsa->host_status &= ~EVENT_STATUS_STATIC_FLAGS;
               }
               else
               {
                  fsa->host_status &= ~EVENT_STATUS_FLAGS;
               }
               error_action(fsa->host_alias, "stop");
               event_log(0L, EC_HOST, ET_EXT, EA_ERROR_END, "%s",
                         fsa->host_alias);
               if ((fsa->host_status & HOST_ERROR_OFFLINE_STATIC) ||
                   (fsa->host_status & HOST_ERROR_OFFLINE) ||
                   (fsa->host_status & HOST_ERROR_OFFLINE_T))
               {
                  sign = OFFLINE_SIGN;
               }
               else
               {
                  sign = INFO_SIGN;
               }
               system_log(sign, __FILE__, __LINE__,
                          "Starting input queue for %s that was stopped by init_afd.",
                          fsa->host_alias);
               event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                         fsa->host_alias);
            }
         } /* if (fsa->error_counter > 0) */
#ifdef WITH_ERROR_QUEUE
         if ((db.special_flag & IN_ERROR_QUEUE) &&
             (fsa->host_status & ERROR_QUEUE_SET))
         {
            remove_from_error_queue(db.job_id, fsa);
         }
#endif

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
      } /* for (i = 0; i < files_to_send; i++) */

      /* Do not forget to remove lock file if we have created one */
      if ((db.lock == LOCKFILE) && (fsa->active_transfers == 1))
      {
         if (unlink(db.lock_file_name) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to unlink() lock file `%s' : %s",
                      db.lock_file_name, strerror(errno));
            exit(REMOVE_LOCKFILE_ERROR);
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Removed lock file `%s'.", db.lock_file_name);
            }
         }
      }

      /*
       * Remove file directory.
       */
      if (rmdir(file_path) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to rmdir() `%s' : %s", file_path, strerror(errno));
         exit_status = STILL_FILES_TO_SEND;
      }

#ifdef _WITH_BURST_2
      burst_2_counter++;
   } while ((cb2_ret = check_burst_2(file_path, &files_to_send, NULL)) == YES);
   burst_2_counter--;

   if (cb2_ret == NEITHER)
   {
      exit_status = STILL_FILES_TO_SEND;
   }
#endif /* _WITH_BURST_2 */

   exitflag = 0;
   exit(exit_status);
}


/*++++++++++++++++++++++++++ copy_file_mkdir() ++++++++++++++++++++++++++*/
static int
copy_file_mkdir(char *from, char *to)
{
   int from_fd,
       ret = SUCCESS;

   /* Open source file. */
#ifdef O_LARGEFILE
   if ((from_fd = open(from, O_RDONLY | O_LARGEFILE)) == -1)
#else
   if ((from_fd = open(from, O_RDONLY)) == -1)
#endif
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Could not open `%s' for copying : %s", from, strerror(errno));
      ret = MOVE_ERROR;
   }
   else
   {
      struct stat stat_buf;

      /* Need size and permissions of input file. */
      if (fstat(from_fd, &stat_buf) == -1)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Could not fstat() on `%s' : %s", from, strerror(errno));
         (void)close(from_fd);
         ret = MOVE_ERROR;
      }
      else
      {
         int to_fd;

         /* Open destination file. */
#ifdef O_LARGEFILE
         if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE,
#else
         if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
#endif
                           stat_buf.st_mode)) == -1)
         {
            if ((errno == ENOENT) && (db.special_flag & CREATE_TARGET_DIR))
            {
               char *p_file = to;

               p_file += strlen(to);
               while ((*p_file != '/') && (p_file != to))
               {
                  p_file--;
               }
               if (*p_file == '/')
               {
                  char *error_ptr;

                  *p_file = '\0';
                  if (((ret = check_create_path(to, 0, &error_ptr,
                                                YES)) == CREATED_DIR) ||
                      (ret == CHOWN_ERROR))
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                               "Created path `%s'", to);
                     if (ret == CHOWN_ERROR)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                                  "Failed to chown() of directory `%s' : %s",
                                  to, strerror(errno));
                     }
                     *p_file = '/';
#ifdef O_LARGEFILE
                     if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE,
#else
                     if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
#endif
                                       stat_buf.st_mode)) == -1)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "Could not open `%s' for copying : %s",
                                  to, strerror(errno));
                        ret = MOVE_ERROR;
                     }
                     else
                     {
                        ret = SUCCESS;
                     }
                  }
                  else if (ret == MKDIR_ERROR)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to mkdir() `%s' error : %s",
                                    to, strerror(errno));
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '/';
                          }
                       }
                  else if (ret == STAT_ERROR)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to stat() `%s' error : %s",
                                    to, strerror(errno));
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '/';
                          }
                       }
                  else if (ret == NO_ACCESS)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Cannot access directory `%s' : %s",
                                    to, strerror(errno));
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '/';
                          }
                          ret = MOVE_ERROR;
                       }
                  else if (ret == ALLOC_ERROR)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                    "Failed to allocate memory : %s",
                                    strerror(errno));
                       }
                  else if (ret == SUCCESS)
                       {
                          trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL,
                                    "Hmmm, directory does seem to be ok, so why can we not open the file!?");
                          ret = MOVE_ERROR;
                       }
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Could not open `%s' for copying : %s",
                         to, strerror(errno));
               ret = MOVE_ERROR;
            }
         }
         if (to_fd != -1)
         {
            if (stat_buf.st_size > 0)
            {
               char *buffer;

               if ((buffer = malloc(stat_buf.st_blksize)) == NULL)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to allocate memory : %s", strerror(errno));
                  ret = MOVE_ERROR;
               }
               else
               {
                  int bytes_buffered;

                  do
                  {
                     if ((bytes_buffered = read(from_fd, buffer,
                                                stat_buf.st_blksize)) == -1)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  "Failed to read() `%s' : %s",
                                  from, strerror(errno));
                        ret = MOVE_ERROR;
                        break;
                     }
                     if (bytes_buffered > 0)
                     {
                        if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                     "Failed to write() `%s' : %s",
                                     to, strerror(errno));
                           ret = MOVE_ERROR;
                           break;
                        }
                     }
                  } while (bytes_buffered == stat_buf.st_blksize);
                  free(buffer);
               }
            }
            if (close(to_fd) == -1)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to close() `%s' : %s", to, strerror(errno));
            }
         }
      }
      if (close(from_fd) == -1)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                   "Failed to close() `%s' : %s", from, strerror(errno));
      }
   }

   return(ret);
}


/*+++++++++++++++++++++++++++++ sf_loc_exit() +++++++++++++++++++++++++++*/
static void
sf_loc_exit(void)
{
   int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
   int  readfd;
#endif
   char sf_fin_fifo[MAX_PATH_LENGTH];

   if ((fsa != NULL) && (db.fsa_pos >= 0))
   {
      if ((fsa->job_status[(int)db.job_no].file_size_done > 0) ||
          (fsa->job_status[(int)db.job_no].no_of_files_done > 0))
      {
#ifdef _WITH_BURST_2
# if SIZEOF_OFF_T == 4
         fd = sprintf(sf_fin_fifo, "%lu Bytes copied in %d file(s).",
# else
         fd = sprintf(sf_fin_fifo, "%llu Bytes copied in %d file(s).",
# endif
                      fsa->job_status[(int)db.job_no].file_size_done,
                      fsa->job_status[(int)db.job_no].no_of_files_done);
         if (burst_2_counter == 1)
         {
            (void)strcpy(&sf_fin_fifo[fd], " [BURST]");
         }
         else if (burst_2_counter > 1)
              {
                 (void)sprintf(sf_fin_fifo + fd, " [BURST * %u]",
                               burst_2_counter);
              }
         trans_log(INFO_SIGN, NULL, 0, NULL, "%s", sf_fin_fifo);
#else
# if SIZEOF_OFF_T == 4
         trans_log(INFO_SIGN, NULL, 0, NULL, "%lu Bytes copied in %d file(s).",
# else
         trans_log(INFO_SIGN, NULL, 0, NULL, "%llu Bytes copied in %d file(s).",
# endif
                   fsa->job_status[(int)db.job_no].file_size_done,
                   fsa->job_status[(int)db.job_no].no_of_files_done);
#endif /* _WITH_BURST_2 */
      }
      reset_fsa((struct job *)&db, exitflag);
   }

   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
   }
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
   }

   (void)strcpy(sf_fin_fifo, p_work_dir);
   (void)strcat(sf_fin_fifo, FIFO_DIR);
   (void)strcat(sf_fin_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(sf_fin_fifo, &readfd, &fd) == -1)
#else
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo `%s' : %s", sf_fin_fifo, strerror(errno));
   }
   else
   {
#ifdef _FIFO_DEBUG
      char  cmd[2];

      cmd[0] = ACKN; cmd[1] = '\0';
      show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
      /* Tell FD we are finished */
      if (write(fd, &db.my_pid, sizeof(pid_t)) != sizeof(pid_t))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
#ifdef WITHOUT_FIFO_RW_SUPPORT
      (void)close(readfd);
#endif
      (void)close(fd);
   }
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(INCORRECT);
}
