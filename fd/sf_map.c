/*
 *  sf_map.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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
 **   sf_map - sends files to MAP
 **
 ** SYNOPSIS
 **   sf_map [--version] [-w <directory>] -m <message-file>
 **
 ** DESCRIPTION
 **   sf_map is very similar to sf_ftp only that it sends files
 **   to the MAP system with special functions from that system.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.1997 H.Kiehl Created
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   25.08.1997 H.Kiehl Integrated map function store_blob().
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   10.09.1997 H.Kiehl Add FAX_E print service.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* getenv(), strtoul(), abort()   */
#include <ctype.h>                     /* isdigit()                      */
#include <setjmp.h>                    /* setjmp(), longjmp()            */
#include <signal.h>                    /* signal()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>                  /* struct timeval                 */
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <unistd.h>                    /* unlink(), alarm()              */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int                        counter_fd = -1,    /* NOT USED */
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           trans_rule_pos,/* NOT USED */
                           user_rule_pos, /* NOT USED */
                           timeout_flag = OFF,
                           fsa_id,
                           fsa_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO;
long                       transfer_timeout; /* Not used [init_sf()]    */
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer;
char                       host_deleted = NO,
                           *p_work_dir = NULL,
                           msg_str[1],
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           *del_file_name_buffer = NULL,
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa;
struct job                 db;
struct rule                *rule;      /* NOT USED */
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

#ifdef _WITH_MAP_SUPPORT
/* Local variables */
static jmp_buf             env_alrm;

/* Local functions */
static void sf_map_exit(void),
            sig_bus(int),
            sig_segv(int),
            sig_kill(int),
            sig_exit(int),
            sig_handler(int);

/* Remote function prototypes */
extern int  store_blob(unsigned long, char *, char *, char *, char *,
                       long, signed long *);
extern char *map_db_errafd(void);

#define DB_OKAY 100000L
#endif
#define MAP_TIMEOUT 1200    /* 20 Minutes */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_MAP_SUPPORT
#ifdef _VERIFY_FSA
   unsigned long    ul_variable;
#endif
   int              i,
                    files_to_send = 0;
   signed long      map_errno;
   off_t            lock_offset,
                    *p_file_size_buffer;
   char             *p_source_file,
                    *p_file_name_buffer,
                    file_name[MAX_FILENAME_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    source_file[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
   unsigned int     *ol_job_number = NULL;
   char             *ol_data = NULL,
                    *ol_file_name = NULL;
   unsigned short   *ol_archive_name_length,
                    *ol_file_name_length;
   off_t            *ol_file_size = NULL;
   size_t           ol_size,
                    ol_real_size;
   clock_t          end_time,
                    start_time = 0,
                    *ol_transfer_time = NULL;
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
   if (atexit(sf_map_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   msg_str[0] = '\0';
   files_to_send = init_sf(argc, argv, file_path, MAP_FLAG);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGALRM, sig_handler) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not set signal handler to catch SIGINT : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Inform FSA that we have are ready to copy the files. */
   if (host_deleted == NO)
   {
      if (check_fsa() == YES)
      {
         if ((db.fsa_pos = get_host_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
      }
      if (host_deleted == NO)
      {
         fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = MAP_ACTIVE;
         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = files_to_send;
      }
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      output_log_ptrs(&ol_fd,
                      &ol_job_number,
                      &ol_data,
                      &ol_file_name,
                      &ol_file_name_length,
                      &ol_archive_name_length,
                      &ol_file_size,
                      &ol_size,
                      &ol_transfer_time,
                      db.host_alias,
                      MAP);
   }
#endif

   /* Prepare pointers and directory name */
   (void)strcpy(source_file, file_path);
   p_source_file = source_file + strlen(source_file);
   *p_source_file++ = '/';

   /* Now determine the real hostname. */
   if (db.toggle_host == YES)
   {
      if (fsa[db.fsa_pos].host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname,
                      fsa[db.fsa_pos].real_hostname[HOST_TWO - 1]);
      }
      else
      {
         (void)strcpy(db.hostname,
                      fsa[db.fsa_pos].real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa[db.fsa_pos].real_hostname[(int)(fsa[db.fsa_pos].host_toggle - 1)]);
   }

   /* Send all files */
   p_file_name_buffer = file_name_buffer;
   p_file_size_buffer = file_size_buffer;
   for (i = 0; i < files_to_send; i++)
   {
      /* Get the the name of the file we want to send next */
      (void)strcpy(p_source_file, p_file_name_buffer);
      (void)strcpy(file_name, p_file_name_buffer);

      /* Write status to FSA? */
      if (host_deleted == NO)
      {
         if (check_fsa() == YES)
         {
            if ((db.fsa_pos = get_host_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
            }
         }
         if (host_deleted == NO)
         {
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
            (void)strcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use,
                         file_name);
         }
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         start_time = times(&tmsdummy);
      }
#endif

      if (*p_file_size_buffer > 0)
      {
         /*
          * Due to constant hang ups of the map functions its necessary
          * to timeout these functions. :-((((
          */
         if (setjmp(env_alrm) != 0)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Map function timed out!");
            exit(MAP_FUNCTION_ERROR);
         }
         (void)alarm(MAP_TIMEOUT);

         /*
          * Determine if we want to print a job or send it to the
          * MAP database. We determine this by checking if db.user
          * is numeric or not. If it is numeric send it to the
          * MAP database, otherwise it is a print job.
          */
         if (isdigit(db.user[0]) == 0)
         {
            int  fax_error;
            char fax_print_error_str[80];

            if ((fax_error = faxe_print(source_file,       /* FSS name 8-O */
                                        file_name,         /* FAX_E name   */
                                        db.hostname,       /* Remote host  */
                                        db.user,           /* Print format */
                                        fax_print_error_str)) < 0)
            {
               (void)alarm(0);
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to print file <%s> to %s [FAX PRINT ERROR %d].",
                               file_name, db.hostname, fax_error);
                  trans_db_log(ERROR_SIGN, NULL, 0, "%s", fax_print_error_str);
               }
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to print file <%s> to %s [FAX PRINT ERROR %d].",
                         file_name, db.hostname, fax_error);
               trans_log(ERROR_SIGN, NULL, 0, "%s", fax_print_error_str);
               trans_log(INFO_SIGN, NULL, 0, "%d Bytes printed in %d file(s).",
                         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
               exit(MAP_FUNCTION_ERROR);
            }
            else
            {
               (void)alarm(0);
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Printed file <%s> to %s.",
                               file_name, db.hostname);
               }
            }
         }
         else /* Send to MAP database */
         {
            store_blob(strtoul(db.user, NULL, 10),  /* Datentyp des MFS-Objekts */
                       file_path,                   /* source directory         */
                       file_name,
                       file_name,                   /* Objektname das gespeichert werden soll */
                       db.hostname,                 /* Remote hostname          */
                       db.port,                     /* Schluessel der Empfangskette des MFS   */
                       &map_errno);
            (void)alarm(0);
            if (map_errno != DB_OKAY)
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to send file <%s> to %s:%d [MAP ERROR %ld].",
                               file_name, db.hostname, db.port, map_errno);
                  trans_db_log(ERROR_SIGN, NULL, 0, "%s", map_db_errafd());
               }
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send file <%s> to %s:%d [MAP ERROR %ld].",
                         file_name, db.hostname, db.port, map_errno);
               trans_log(ERROR_SIGN, NULL, 0, "%s", map_db_errafd());
               trans_log(INFO_SIGN, NULL, 0, "%d Bytes send in %d file(s).",
                         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
               exit(MAP_FUNCTION_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Send file <%s> to %s:%d [%ld].",
                               file_name, db.hostname, db.port, map_errno);
               }
            }
         }
      }
      else
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Ignoring file <%s>, since MAP can't handle files with %d Bytes length.",
                   file_name, *p_file_size_buffer);
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         end_time = times(&tmsdummy);
      }
#endif

      /* Tell FSA we have send a file !!!! */
      if (host_deleted == NO)
      {
         lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);

         /* Before we read from the FSA lets make */
         /* sure that it is NOT stale!            */
         if (check_fsa() == YES)
         {
            if ((db.fsa_pos = get_host_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
            }
            else
            {
               lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
            }
         }
         if (host_deleted == NO)
         {
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] = '\0';
            fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done = i + 1;
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += *p_file_size_buffer;
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = 0;
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = 0;

            /* Total file counter */
            lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
            fsa[db.fsa_pos].total_file_counter -= 1;
#ifdef _VERIFY_FSA
            if (fsa[db.fsa_pos].total_file_counter < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file counter for host %s less then zero. Correcting to %d.",
                          fsa[db.fsa_pos].host_dsp_name,
                          files_to_send - (i + 1));
               fsa[db.fsa_pos].total_file_counter = files_to_send - (i + 1);
            }
#endif

            if (*p_file_size_buffer > 0)
            {
               /* Total file size */
#ifdef _VERIFY_FSA
               ul_variable = fsa[db.fsa_pos].total_file_size;
#endif
               fsa[db.fsa_pos].total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
               if (fsa[db.fsa_pos].total_file_size > ul_variable)
               {
                  int   k;
                  off_t *tmp_ptr = p_file_size_buffer;

                  tmp_ptr++;
                  fsa[db.fsa_pos].total_file_size = 0;
                  for (k = (i + 1); k < files_to_send; k++)
                  {     
                     fsa[db.fsa_pos].total_file_size += *tmp_ptr;
                  }

                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Total file size for host %s overflowed. Correcting to %lu.",
                             fsa[db.fsa_pos].host_dsp_name,
                             fsa[db.fsa_pos].total_file_size);
               }
               else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
                        (fsa[db.fsa_pos].total_file_size > 0))
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "fc for host %s is zero but fs is not zero. Correcting.",
                                  fsa[db.fsa_pos].host_dsp_name);
                       fsa[db.fsa_pos].total_file_size = 0;
                    }
#endif
               /* Number of bytes send */
               fsa[db.fsa_pos].bytes_send += *p_file_size_buffer;
               fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += *p_file_size_buffer;
            }

            /* File counter done */
            fsa[db.fsa_pos].file_counter_done += 1;
            unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
            unlock_region(fsa_fd, lock_offset);
         }
      }

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
          * when adding ARCHIVE_UNIT * db.archive_time to
          * msg_name.
          */
         if (archive_file(file_path, file_name, p_db) < 0)
         {
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to archive file <%s>", file_name);
            }

            /*
             * NOTE: We _MUST_ delete the file we just send,
             *       else the file directory will run full!
             */
            if (unlink(source_file) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file <%s> after copying it successfully : %s",
                          source_file, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)strcpy(ol_file_name, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
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
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Archived file <%s>.", file_name);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)strcpy(ol_file_name, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
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
         if (unlink(source_file) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not unlink() local file <%s> after copying it successfully : %s",
                       source_file, strerror(errno));
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            (void)strcpy(ol_file_name, p_file_name_buffer);
            *ol_file_name_length = (unsigned short)strlen(ol_file_name);
            *ol_file_size = *p_file_size_buffer;
            *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
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
      if (fsa[db.fsa_pos].error_counter > 0)
      {
         int  fd,
              j;
         char fd_wake_up_fifo[MAX_PATH_LENGTH];

         lock_region_w(fsa_fd,
                       (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);
         fsa[db.fsa_pos].error_counter = 0;

         /*
          * Wake up FD!
          */
         (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
         if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to open() FIFO <%s> : %s",
                       fd_wake_up_fifo, strerror(errno));
         }
         else
         {
            if (write(fd, "", 1) != 1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to write() to FIFO <%s> : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Failed to close() FIFO <%s> : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
         }

         /*
          * Remove the error condition (NOT_WORKING) from all jobs
          * of this host.
          */
         for (j = 0; j < fsa[db.fsa_pos].allowed_transfers; j++)
         {
            if ((j != db.job_no) &&
                (fsa[db.fsa_pos].job_status[j].connect_status == NOT_WORKING))
            {
               fsa[db.fsa_pos].job_status[j].connect_status = DISCONNECT;
            }
         }
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);

         /*
          * Since we have successfully transmitted a file, no need to
          * have the queue stopped anymore.
          */
         if (fsa[db.fsa_pos].host_status & AUTO_PAUSE_QUEUE_STAT)
         {
            fsa[db.fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_STAT;
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       "Starting input queue for %s that was stopped by init_afd.",
                       fsa[db.fsa_pos].host_alias);
         }
      } /* if (fsa[db.fsa_pos].error_counter > 0) */

      p_file_name_buffer += MAX_FILENAME_LENGTH;
      p_file_size_buffer++;
   } /* for (i = 0; i < files_to_send; i++) */

   trans_log(INFO_SIGN, NULL, 0, "%d Bytes send in %d file(s).",
             fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done, i);

   /*
    * Remove file directory.
    */
   if (rmdir(file_path) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to remove directory <%s> : %s",
                 file_path, strerror(errno));
   }
#endif /* _WITH_MAP_SUPPORT */

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


#ifdef _WITH_MAP_SUPPORT
/*+++++++++++++++++++++++++++++ sf_map_exit() +++++++++++++++++++++++++++*/
static void
sf_map_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   reset_fsa((struct job *)&db, exitflag);

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
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo <%s> : %s", sf_fin_fifo, strerror(errno));
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


/*++++++++++++++++++++++++++++++ sig_handler() ++++++++++++++++++++++++++*/
static void
sig_handler(int signo)
{
   longjmp(env_alrm, 1);
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
#endif /* _WITH_MAP_SUPPORT */
