/*
 *  gf_ftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000, 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gf_ftp - gets files via FTP
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* close(), getpid()              */
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"
#include "version.h"

int                        exitflag = IS_FAULTY_VAR,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           host_deleted = NO,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           *no_of_listed_files,
                           rl_fd = -1,
                           trans_db_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag;
#ifndef _NO_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
long                       transfer_timeout;
char                       msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct retrieve_list       *rl;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job                 db;

/* Local functions */
static void                gf_ftp_exit(void),
                           reset_values(int, off_t, int, off_t),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              blocksize,
                    files_retrieved = 0,
                    files_to_retrieve,
                    status;
   off_t            file_size_retrieved = 0,
                    file_size_to_retrieve,
                    lock_offset;
   char             work_dir[MAX_PATH_LENGTH];
#ifdef SA_FULLDUMP
   struct sigaction sact;
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
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(gf_ftp_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   init_gf(argc, argv, FTP_FLAG);
   msg_str[0] = '\0';
   timeout_flag = OFF;
   blocksize = fsa[db.fsa_pos].block_size;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_kill) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   if (db.transfer_mode == 'D')
   {
      db.transfer_mode = 'I';
   }

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

   /* Connect to remote FTP-server */
   if (((status = ftp_connect(db.hostname, db.port)) != SUCCESS) &&
       (status != 230))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "FTP connection to %s at port %d failed (%d).",
                db.hostname, db.port, status);
      exit(CONNECT_ERROR);
   }
   else
   {
      if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
      {
         if (status == 230)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Connected. No user and password required, logged in.");
         }
         else
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Connected.");
         }
      }
   }

   /* Login */
   if (status != 230) /* We are not already logged in! */
   {
      if (fsa[db.fsa_pos].proxy_name[0] == '\0')
      {
         /* Send user name */
         if (((status = ftp_user(db.user)) != SUCCESS) && (status != 230))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to send user <%s> (%d).", db.user, status);
            (void)ftp_quit();
            exit(USER_ERROR);
         }
         else
         {
            if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
            {
               if (status != 230)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Entered user name <%s>.", db.user);
               }
               else
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Entered user name <%s>. No password required, logged in.",
                               db.user);
               }
            }
         }

         /* Send password (if required) */
         if (status != 230)
         {
            if ((status = ftp_pass(db.password)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send password for user <%s> (%d).",
                         db.user, status);
               (void)ftp_quit();
               exit(PASSWORD_ERROR);
            }
            else
            {
               if ((fsa[db.fsa_pos].debug == YES) &&
                   (trans_db_log_fd != -1))
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Entered password, logged in as %s.", db.user);
               }
            }
         } /* if (status != 230) */
      }
      else /* Go through the proxy procedure. */
      {
         handle_proxy();
      }
   } /* if (status != 230) */

   /* Set transfer mode */
   if ((status = ftp_type(db.transfer_mode)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "Failed to set transfer mode to %c (%d).",
                db.transfer_mode, status);
      (void)ftp_quit();
      exit(TYPE_ERROR);
   }
   else
   {
      if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                      "Changed transfer mode to %c.", db.transfer_mode);
      }
   }

   /* Change directory if necessary */
   if (db.target_dir[0] != '\0')
   {
      if ((status = ftp_cd(db.target_dir)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to change directory to %s (%d).",
                   db.target_dir, status);
         (void)ftp_quit();
         exit(CHDIR_ERROR);
      }
      else
      {
         if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Changed directory to %s.", db.target_dir);
         }
      }
   } /* if (db.target_dir[0] != '\0') */

   if ((files_to_retrieve = get_remote_file_names(&file_size_to_retrieve)) > 0)
   {
      int         fd,
                  i;
      off_t       bytes_done;
      char        *buffer,
                  local_file[MAX_PATH_LENGTH],
                  local_tmp_file[MAX_PATH_LENGTH],
                  *p_local_file,
                  *p_local_tmp_file;
      struct stat stat_buf;

      /* Inform FSA that we have finished connecting and */
      /* will now start to retrieve data.                */
      if (host_deleted == NO)
      {
         lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);
         if (check_fsa() == YES)
         {
            if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                 no_of_hosts)) == INCORRECT)
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
            fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = RETRIEVE_ACTIVE;
            fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = files_to_retrieve;
            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size = file_size_to_retrieve;

            /* Number of connections */
            lock_region_w(fsa_fd,
                          (char *)&fsa[db.fsa_pos].connections - (char *)fsa);
            fsa[db.fsa_pos].connections += 1;
            unlock_region(fsa_fd,
                          (char *)&fsa[db.fsa_pos].connections - (char *)fsa);

            /* Total file counter */
            lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
            fsa[db.fsa_pos].total_file_counter += files_to_retrieve;

            /* Total file size */
            fsa[db.fsa_pos].total_file_size += file_size_to_retrieve;
            unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
            unlock_region(fsa_fd, lock_offset);
         }
      }

      /* Allocate buffer to read data from the source file. */
      if ((buffer = malloc(blocksize + 4)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         (void)ftp_quit();
         reset_values(files_retrieved, file_size_retrieved,
                      files_to_retrieve, file_size_to_retrieve);
         exit(ALLOC_ERROR);
      }

      /* Get directory where files are to be stored and */
      /* prepare some pointers for the file names.      */
      if (create_remote_dir(fra[db.fra_pos].url, local_file) == INCORRECT)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to determine local incoming directory for <%s>.",
                    fra[db.fra_pos].dir_alias);
         (void)ftp_quit();
         reset_values(files_retrieved, file_size_retrieved,
                      files_to_retrieve, file_size_to_retrieve);
         exit(INCORRECT);
      }
      else
      {
         int length = strlen(local_file);

         local_file[length++] = '/';
         local_file[length] = '\0';
         (void)strcpy(local_tmp_file, local_file);
         p_local_file = &local_file[length];
         p_local_tmp_file = &local_tmp_file[length];
         *p_local_tmp_file = '.';
         p_local_tmp_file++;
      }

      /* Retrieve all files. */
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (rl[i].retrieved == NO)
         {
            off_t offset;

            (void)strcpy(p_local_tmp_file, rl[i].file_name);
            if (stat(local_tmp_file, &stat_buf) == -1)
            {
               offset = 0;
            }
            else
            {
               offset = stat_buf.st_size;
            }
            if (((status = ftp_data(rl[i].file_name, offset, db.mode_flag,
                                    DATA_READ)) != SUCCESS) &&
                (status != -550))
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open remote file %s (%d).",
                         rl[i].file_name, status);
               (void)ftp_quit();
               reset_values(files_retrieved, file_size_retrieved,
                            files_to_retrieve, file_size_to_retrieve);
               exit(OPEN_REMOTE_ERROR);
            }
            if (status == -550) /* ie. file has been deleted or is NOT a file. */
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to open remote file %s (%d).",
                         rl[i].file_name, status);

               /*
                * Mark this file as retrieved or else we will always
                * fall over this file.
                */
               rl[i].retrieved = YES;
            }
            else /* status == SUCCESS */
            {
               if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Opened data connection for file %s.",
                               rl[i].file_name);
               }

               if (offset > 0)
               {
                  fd = open(local_tmp_file, O_WRONLY | O_APPEND);
               }
               else
               {
                  fd = open(local_tmp_file, O_WRONLY | O_CREAT, FILE_MODE);
               }
               if (fd == -1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to open local file %s : %s",
                            local_tmp_file, strerror(errno));
                  (void)ftp_quit();
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve);
                  exit(OPEN_LOCAL_ERROR);
               }
               else
               {
                  if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                  {
                     msg_str[0] = '\0';
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Opened local file %s.", local_tmp_file);
                  }
               }

               if (host_deleted == NO)
               {
                  if (check_fsa() == YES)
                  {
                     if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                         no_of_hosts)) == INCORRECT)
                     {
                        host_deleted = YES;
                     }
                  }
                  if (host_deleted == NO)
                  {
                     if (rl[i].size == -1)
                     {
                        fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = 0;
                     }
                     else
                     {
                        fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = rl[i].size;
                     }
                     (void)strcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use,
                                  rl[i].file_name);
                  }
               }
               bytes_done = 0;
               do
               {
                  if ((status = ftp_read(buffer, blocksize)) == INCORRECT)
                  {
                     status = errno;
                     msg_str[0] = '\0';
                     if (status == EPIPE)
                     {
                        (void)ftp_get_reply();
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,                 
                               "Failed to read from remote file %s",
                               rl[i].file_name);
                     reset_values(files_retrieved, file_size_retrieved,
                                  files_to_retrieve, file_size_to_retrieve);
                     if (status == EPIPE)
                     {
                        (void)rec(transfer_log_fd, DEBUG_SIGN,
                                  "%-*s[%d]: Hmm. Pipe is broken. Will NOT send a QUIT. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit(READ_REMOTE_ERROR);
                  }
                  else if (status > 0)
                       {
                          if (write(fd, buffer, status) != status)
                          {
                             trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                       "Failed to write() to file %s : %s",
                                       local_tmp_file, strerror(errno));
                             (void)ftp_quit();
                             reset_values(files_retrieved, file_size_retrieved,
                                          files_to_retrieve, file_size_to_retrieve);
                             exit(WRITE_LOCAL_ERROR);
                          }
                          bytes_done += status;
                       }

                  if (host_deleted == NO)
                  {
                     if (check_fsa() == YES)
                     {
                        if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                            no_of_hosts)) == INCORRECT)
                        {
                           host_deleted = YES;
                        }
                     }
                     if (host_deleted == NO)
                     {
                        fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                        fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += status;
                        fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += status;
                     }
                  }
               } while (status != 0);

               /* Close the FTP data connection. */
               if ((status = ftp_close_data(DATA_READ)) != SUCCESS)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to close data connection (%d).", status);
                  (void)ftp_quit();
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve);
                  exit(CLOSE_REMOTE_ERROR);
               }
               else
               {
                  if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Closed data connection for file %s.",
                                  rl[i].file_name);
                  }
               }

               /* Close the local file. */
               if (close(fd) == -1)
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to close() local file %s.", local_tmp_file);
               }
               else
               {
                  if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                  {
                     msg_str[0] = '\0';
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Closed local file %s.", local_tmp_file);
                  }
               }

               /* Check if remote file is to be deleted. */
               if (fra[db.fra_pos].remove == YES)
               {
                  if ((status = ftp_dele(rl[i].file_name)) != SUCCESS)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__,
                               "Failed to delete remote file %s (%d).",
                               rl[i].file_name, status);
                  }
                  else
                  {
                     if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Deleted remote file %s.",
                                     rl[i].file_name);
                     }
                  }
               }

               if (host_deleted == NO)
               {
                  lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
                  rlock_region(fsa_fd, lock_offset);

                  if (check_fsa() == YES)
                  {
                     if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                         no_of_hosts)) == INCORRECT)
                     {
                        host_deleted = YES;
                     }
                  }
                  if (host_deleted == NO)
                  {
#ifdef _VERIFY_FSA
                     unsigned long ul_variable;
#endif

                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] = '\0';
                     fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done++;
                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = 0;
                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = 0;

                     /* Total file counter */
                     lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
                     fsa[db.fsa_pos].total_file_counter -= 1;
#ifdef _VERIFY_FSA
                     if (fsa[db.fsa_pos].total_file_counter < 0)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Total file counter for host <%s> less then zero. Correcting to %d.",
                                   fsa[db.fsa_pos].host_dsp_name,
                                   files_to_retrieve - (files_retrieved + 1));
                        fsa[db.fsa_pos].total_file_counter = files_to_retrieve - (files_retrieved + 1);
                     }
#endif

                     /* Total file size */
                     if (rl[i].size != -1)
                     {
#ifdef _VERIFY_FSA
                        ul_variable = fsa[db.fsa_pos].total_file_size;
#endif
                        fsa[db.fsa_pos].total_file_size -= rl[i].size;
#ifdef _VERIFY_FSA
                        if (fsa[db.fsa_pos].total_file_size > ul_variable)
                        {
                           off_t new_size = file_size_to_retrieve - file_size_retrieved;

                           if (new_size < 0)
                           {
                              new_size = 0;
                           }
                           fsa[db.fsa_pos].total_file_size = new_size;
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Total file size for host <%s> overflowed. Correcting to %lu.",
                                      fsa[db.fsa_pos].host_dsp_name,
                                      fsa[db.fsa_pos].total_file_size);
                        }
                        else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
                                 (fsa[db.fsa_pos].total_file_size > 0))
                             {
                                system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                           "fc for host <%s> is zero but fs is not zero. Correcting.",
                                           fsa[db.fsa_pos].host_dsp_name);
                                fsa[db.fsa_pos].total_file_size = 0;
                             }
#endif
                     }

                     /* File counter done */
                     fsa[db.fsa_pos].file_counter_done += 1;

                     /* Number of bytes send */
                     fsa[db.fsa_pos].bytes_send += bytes_done;
                     unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
                     unlock_region(fsa_fd, lock_offset);

                     if (fsa[db.fsa_pos].error_counter > 0)
                     {
                        int  fd, j;
                        char fd_wake_up_fifo[MAX_PATH_LENGTH];

                        lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);
                        fsa[db.fsa_pos].error_counter = 0;

                        /* Wake up FD! */
                        (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                                      FIFO_DIR, FD_WAKE_UP_FIFO);
                        if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to open() FIFO <%s> : %s",
                                      fd_wake_up_fifo, strerror(errno));
                        }
                        else
                        {
                           char dummy;

                           if (write(fd, &dummy, 1) != 1)
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
                                      "Starting input queue for <%s> that was stopped by init_afd.",
                                      fsa[db.fsa_pos].host_alias);
                        }
                     }
                  }
               }

               /*
                * If the file size is not the same as the one when we did
                * the remote ls command, give a warning in the transfer log
                * so some action can be taken against the originator.
                */
               if ((rl[i].size != -1) && ((bytes_done + offset) != rl[i].size))
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "File size of file %s changed from %u to %u when it was retrieved.",
                            rl[i].file_name, rl[i].size, bytes_done + offset);
                  rl[i].size = bytes_done;
               }

               /* Rename the file so AMG can grab it. */
               (void)strcpy(p_local_file, rl[i].file_name);
               if (rename(local_tmp_file, local_file) == -1)
               {
                  msg_str[0] = '\0';
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to rename() %s to %s : %s",
                            local_tmp_file, local_file, strerror(errno));
               }
               else
               {
                  rl[i].retrieved = YES;
                  files_retrieved++;
                  file_size_retrieved += bytes_done;
               }
            }
         } /* if (rl[i].retrieved == NO) */
      } /* for (i = 0; i < *no_of_listed_files; i++) */

      reset_values(files_retrieved, file_size_retrieved,
                   files_to_retrieve, file_size_to_retrieve);

      /* Free memory for the read buffer. */
      free(buffer);
   } /* files_to_retrieve > 0 */

   fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
   msg_str[0] = '\0';
   trans_log(INFO_SIGN, NULL, 0, "%lu Bytes retrieved in %d file(s).",
             file_size_retrieved, files_retrieved);
   if ((status = ftp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "Failed to disconnect from remote host (%d).", status);
   }
   else
   {
      if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Logged out.");
      }
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*++++++++++++++++++++++++++++ reset_values() +++++++++++++++++++++++++++*/
static void
reset_values(int files_retrieved,
             off_t file_size_retrieved,
             int   files_to_retrieve,
             off_t file_size_to_retrieve)
{
#ifdef _VERIFY_FSA
   unsigned long ul_variable;
#endif

   if (((files_retrieved < files_to_retrieve) ||
        (file_size_retrieved < file_size_to_retrieve)) &&
       (host_deleted == NO))
   {
      off_t lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;

      rlock_region(fsa_fd, lock_offset);

      if (check_fsa() == YES)
      {
         if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                             no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
      }
      if (host_deleted == NO)
      {
         lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
         if (files_retrieved < files_to_retrieve)
         {
            fsa[db.fsa_pos].total_file_counter -= (files_to_retrieve - files_retrieved);
#ifdef _VERIFY_FSA
            if (fsa[db.fsa_pos].total_file_counter < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file counter for host <%s> less then zero. Correcting to 0.",
                          fsa[db.fsa_pos].host_dsp_name);
               fsa[db.fsa_pos].total_file_counter = 0;
            }
#endif
         }

         if (file_size_retrieved < file_size_to_retrieve)
         {
#ifdef _VERIFY_FSA
            ul_variable = fsa[db.fsa_pos].total_file_size;
#endif
            fsa[db.fsa_pos].total_file_size -= (file_size_to_retrieve - file_size_retrieved);
#ifdef _VERIFY_FSA
            if (fsa[db.fsa_pos].total_file_size > ul_variable)
            {
               fsa[db.fsa_pos].total_file_size = 0;
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file size for host <%s> overflowed. Correcting to 0.",
                          fsa[db.fsa_pos].host_dsp_name);
            }
            else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
                     (fsa[db.fsa_pos].total_file_size > 0))
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "fc for host <%s> is zero but fs is not zero. Correcting.",
                               fsa[db.fsa_pos].host_dsp_name);
                    fsa[db.fsa_pos].total_file_size = 0;
                 }
#endif
         }
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
      }
      unlock_region(fsa_fd, lock_offset);
   }
   return;
}


/*+++++++++++++++++++++++++++++ gf_ftp_exit() +++++++++++++++++++++++++++*/
static void
gf_ftp_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   reset_fsa((struct job *)&db, exitflag);

   (void)strcpy(sf_fin_fifo, p_work_dir);
   (void)strcat(sf_fin_fifo, FIFO_DIR);
   (void)strcat(sf_fin_fifo, SF_FIN_FIFO);
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo <%s> : %s",
                 sf_fin_fifo, strerror(errno));
   }
   else
   {
      pid_t pid = getpid();
#ifdef _FIFO_DEBUG
      char  cmd[2];
#endif
      /* Tell FD we are finished */
#ifdef _FIFO_DEBUG
      cmd[0] = ACKN; cmd[1] = '\0';
      show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
      if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
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


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   exitflag = 0;
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
