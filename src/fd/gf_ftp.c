/*
 *  gf_ftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gf_ftp <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]
 **
 **   options
 **      --version        Version Number
 **      -d               Distributed helper job.
 **      -o <retries>     Old/Error message and number of retries.
 **      -t               Temp toggle.
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
 **   11.04.2004 H.Kiehl Added TLS/SSL support.
 **   13.06.2004 H.Kiehl Added transfer rate limit.
 **   27.06.2006 H.Kiehl When downloading a file with leading dot
 **                      remove the dot when finish downloading.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>                     /* utime()                        */
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* close(), getpid()              */
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"
#include "version.h"

int                        event_log_fd = STDERR_FILENO,
                           exitflag = IS_FAULTY_VAR,
                           files_to_retrieve_shown = 0,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           *p_no_of_hosts = NULL,
                           *no_of_listed_files,
                           rl_fd = -1,
                           trans_db_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           trans_db_log_readfd,
                           transfer_log_readfd,
#endif
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag;
off_t                      file_size_to_retrieve_shown = 0;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
long                       transfer_timeout;
char                       msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct retrieve_list       *rl;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job                 db;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static int                 ftp_timeup(void);
static void                gf_ftp_exit(void),
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
                    more_files_in_list,
                    status;
   off_t            file_size_retrieved = 0,
                    file_size_to_retrieve;
   clock_t          clktck;
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

   /* Do some cleanups when we exit. */
   if (atexit(gf_ftp_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables. */
   init_gf(argc, argv, FTP_FLAG);
   msg_str[0] = '\0';
   if (fsa->trl_per_process > 0)
   {
      if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not get clock ticks per second : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
      if (fsa->trl_per_process < fsa->block_size)
      {
         blocksize = fsa->trl_per_process;
      }
      else
      {
         blocksize = fsa->block_size;
      }
   }
   else
   {
      blocksize = fsa->block_size;
   }

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
      if (fsa->protocol_options & FTP_IGNORE_BIN)
      {
         db.transfer_mode = 'N';
      }
      else
      {
         db.transfer_mode = 'I';
      }
   }

   /* Now determine the real hostname. */
   if (db.toggle_host == YES)
   {
      if (fsa->host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname, fsa->real_hostname[HOST_TWO - 1]);
      }
      else
      {
         (void)strcpy(db.hostname, fsa->real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
   }

   if (fsa->debug > NORMAL_MODE)
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   "Trying to do a %s connect to %s at port %d.",
                   db.mode_str, db.hostname, db.port);
   }

   /* Connect to remote FTP-server. */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   if (fsa->protocol_options & AFD_TCP_KEEPALIVE)
   {
      timeout_flag = transfer_timeout - 5;
      if (timeout_flag < MIN_KEEP_ALIVE_INTERVAL)
      {
         timeout_flag = MIN_KEEP_ALIVE_INTERVAL;
      }
   }
#else
   timeout_flag = OFF;
#endif
   if (((status = ftp_connect(db.hostname, db.port)) != SUCCESS) &&
       (status != 230))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "FTP connection to %s at port %d failed (%d).",
                db.hostname, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         if (status == 230)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "Connected. No user and password required, logged in.");
         }
         else
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Connected.");
         }
      }
   }

#ifdef WITH_SSL
   if ((db.auth == YES) || (db.auth == BOTH))
   {
      if (ftp_ssl_auth() == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "SSL/TSL connection to server `%s' failed.", db.hostname);
         exit(AUTH_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "Authentification successful.");
         }
      }
   }
#endif

   /* Login. */
   if (status != 230) /* We are not already logged in! */
   {
      if (fsa->proxy_name[0] == '\0')
      {
         /* Send user name. */
         if (((status = ftp_user(db.user)) != SUCCESS) && (status != 230))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                      "Failed to send user <%s> (%d).", db.user, status);
            (void)ftp_quit();
            exit(eval_timeout(USER_ERROR));
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               if (status != 230)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Entered user name <%s>.", db.user);
               }
               else
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Entered user name <%s>. No password required, logged in.",
                               db.user);
               }
            }
         }

         /* Send password (if required). */
         if (status != 230)
         {
            if ((status = ftp_pass(db.password)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to send password for user <%s> (%d).",
                         db.user, status);
               (void)ftp_quit();
               exit(eval_timeout(PASSWORD_ERROR));
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
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

#ifdef WITH_SSL
   if (db.auth > NO)
   {
      if (ftp_ssl_init(db.auth) == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "SSL/TSL initialisation failed.");
         (void)ftp_quit();                           
         exit(AUTH_ERROR);
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "SSL/TLS initialisation successful.");
         }
      }
   }
#endif

   if (db.transfer_mode != 'N')
   {
      /* Set transfer mode. */
      if ((status = ftp_type(db.transfer_mode)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "Failed to set transfer mode to %c (%d).",
                   db.transfer_mode, status);
         (void)ftp_quit();
         exit(eval_timeout(TYPE_ERROR));
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "Changed transfer mode to %c.", db.transfer_mode);
         }
      }
   }

   /* Change directory if necessary. */
   if (db.target_dir[0] != '\0')
   {
      char str_mode[5];

#ifdef NEW_FRA
      if ((fra[db.fra_pos].dir_flag & CREATE_R_SRC_DIR) &&
          (fra[db.fra_pos].dir_mode != 0))
      {
         status = sprintf(msg_str, "%04o", fra[db.fra_pos].dir_mode);
         (void)strcpy(str_mode, &msg_str[status - 4]);
      }
      else
      {
#endif
         str_mode[0] = '\0';
#ifdef NEW_FRA
      }
#endif
      if ((status = ftp_cd(db.target_dir,
                           (fra[db.fra_pos].dir_flag & CREATE_R_SRC_DIR) ? YES : NO,
                           str_mode, NULL)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "Failed to change directory to %s (%d).",
                   db.target_dir, status);
         (void)ftp_quit();
         exit(eval_timeout(CHDIR_ERROR));
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                         "Changed directory to %s.", db.target_dir);
         }
      }
   } /* if (db.target_dir[0] != '\0') */

   fsa->job_status[(int)db.job_no].connect_status = FTP_RETRIEVE_ACTIVE;
   if (db.special_flag & DISTRIBUTED_HELPER_JOB)
   {
      /*
       * If we are a helper job, lets NOT stay connected and do a
       * full directory scan.
       */
      db.keep_connected = 0;
   }
   more_files_in_list = NO;
   do
   {
      if ((files_to_retrieve = get_remote_file_names_ftp(&file_size_to_retrieve,
                                                         &more_files_in_list)) > 0)
      {
         int         fd,
                     i,
                     local_file_length;
         off_t       bytes_done;
         char        *buffer,
                     local_file[MAX_PATH_LENGTH],
                     local_tmp_file[MAX_PATH_LENGTH],
                     *p_local_file,
                     *p_local_tmp_file;
         struct stat stat_buf;

         if (more_files_in_list == YES)
         {
            /* Tell fd that he may start some more helper jobs that */
            /* help fetching files.                                 */
            send_proc_fin(YES);
         }

         /* Inform FSA that we have finished connecting and */
         /* will now start to retrieve data.                */
         if (gsf_check_fsa((struct job *)&db) != NEITHER)
         {
            fsa->job_status[(int)db.job_no].no_of_files += files_to_retrieve;
            fsa->job_status[(int)db.job_no].file_size += file_size_to_retrieve;

            /* Number of connections. */
            fsa->connections += 1;

            /* Total file counter. */
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
            fsa->total_file_counter += files_to_retrieve;
            fsa->total_file_size += file_size_to_retrieve;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
            files_to_retrieve_shown += files_to_retrieve;
            file_size_to_retrieve_shown += file_size_to_retrieve;
         }

         (void)gsf_check_fra();
         if (db.fra_pos == INCORRECT)
         {
            /* Looks as if this directory is no longer in our database. */
            (void)ftp_quit();
            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve,
                         (struct job *)&db);
            return(SUCCESS);
         }

         /* Get directory where files are to be stored and */
         /* prepare some pointers for the file names.      */
         if (create_remote_dir(fra[db.fra_pos].url, NULL, NULL, NULL,
                               local_file, &local_file_length) == INCORRECT)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to determine local incoming directory for <%s>.",
                       fra[db.fra_pos].dir_alias);
            (void)ftp_quit();
            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve,
                         (struct job *)&db);
            exit(INCORRECT);
         }
         else
         {
            local_file[local_file_length - 1] = '/';
            local_file[local_file_length] = '\0';
            (void)strcpy(local_tmp_file, local_file);
            p_local_file = &local_file[local_file_length];
            p_local_tmp_file = &local_tmp_file[local_file_length];
            *p_local_tmp_file = '.';
            p_local_tmp_file++;
         }

         /* Allocate buffer to read data from the source file. */
         if ((buffer = malloc(blocksize + 4)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            (void)ftp_quit();
            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve,
                         (struct job *)&db);
            exit(ALLOC_ERROR);
         }

         /* Retrieve all files. */
         for (i = 0; i < *no_of_listed_files; i++)
         {
            if ((rl[i].retrieved == NO) &&
                (rl[i].assigned == ((unsigned char)db.job_no + 1)))
            {
               off_t offset;

               if (rl[i].file_name[0] != '.')
               {
                  (void)strcpy(p_local_tmp_file, rl[i].file_name);
               }
               else
               {
                  (void)strcpy(p_local_file, rl[i].file_name);
               }
               if (fsa->file_size_offset != -1)
               {
                  if (stat(local_tmp_file, &stat_buf) == -1)
                  {
                     offset = 0;
                  }
                  else
                  {
                     offset = stat_buf.st_size;
                  }
               }
               else
               {
                  offset = 0;
               }
               if (((status = ftp_data(rl[i].file_name, offset, db.mode_flag,
                                       DATA_READ, db.rcvbuf_size)) != SUCCESS) &&
                   (status != 550))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to open remote file %s (%d).",
                            rl[i].file_name, status);
                  (void)ftp_quit();
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve,
                               (struct job *)&db);
                  exit(eval_timeout(OPEN_REMOTE_ERROR));
               }
               if (status == 550) /* ie. file has been deleted or is NOT a file. */
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                            "Failed to open remote file %s (%d).",
                            rl[i].file_name, status);

                  /*
                   * Mark this file as retrieved or else we will always
                   * fall over this file.
                   */
                  rl[i].retrieved = YES;
                  if (gsf_check_fsa((struct job *)&db) != NEITHER)
                  {
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                     fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
                     fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                     fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

                     /* Total file counter. */
                     fsa->total_file_counter -= 1;
                     files_to_retrieve_shown -= 1;
#ifdef _VERIFY_FSA
                     if (fsa->total_file_counter < 0)
                     {
                        int tmp_val;

                        tmp_val = files_to_retrieve - (files_retrieved + 1);
                        if (tmp_val < 0)
                        {
                           tmp_val = 0;
                        }
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Total file counter less then zero. Correcting to %d.",
                                  tmp_val);
                        fsa->total_file_counter = tmp_val;
                     }
#endif

                     /* Total file size. */
                     fsa->total_file_size -= rl[i].size;
                     file_size_to_retrieve_shown -= rl[i].size;
#ifdef _VERIFY_FSA
                     if (fsa->total_file_size < 0)
                     {
                        off_t new_size = file_size_to_retrieve - file_size_retrieved;

                        if (new_size < 0)
                        {
                           new_size = 0;
                        }
                        fsa->total_file_size = new_size;
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                  "Total file size overflowed. Correcting to %ld.",
# else
                                  "Total file size overflowed. Correcting to %lld.",
# endif
                                  (pri_off_t)fsa->total_file_size);
                     }
                     else if ((fsa->total_file_counter == 0) &&
                              (fsa->total_file_size > 0))
                          {
                             trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
# if SIZEOF_OFF_T == 4
                                       "fc is zero but fs is not zero (%ld). Correcting.",
# else
                                       "fc is zero but fs is not zero (%lld). Correcting.",
# endif
                                       (pri_off_t)fsa->total_file_size);
                             fsa->total_file_size = 0;
                          }
#endif
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                  }
                  files_retrieved++;
                  file_size_retrieved += rl[i].size;
               }
               else /* status == SUCCESS */
               {
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                  "Opened data connection for file %s.",
                                  rl[i].file_name);
                  }
#ifdef WITH_SSL
                  if (db.auth == BOTH)
                  {
                     if (ftp_auth_data() == INCORRECT)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                  "TSL/SSL data connection to server `%s' failed.",
                                  db.hostname);
                        (void)ftp_quit();
                        exit(eval_timeout(AUTH_ERROR));
                     }
                     else
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                        "Authentification successful.");
                        }
                     }
                  }
#endif

                  if (offset > 0)
                  {
#ifdef O_LARGEFILE
                     fd = open(local_tmp_file, O_WRONLY | O_APPEND | O_LARGEFILE);
#else
                     fd = open(local_tmp_file, O_WRONLY | O_APPEND);
#endif
                  }
                  else
                  {
#ifdef O_LARGEFILE
                     fd = open(local_tmp_file, O_WRONLY | O_CREAT | O_LARGEFILE,
                               FILE_MODE);
#else
                     fd = open(local_tmp_file, O_WRONLY | O_CREAT, FILE_MODE);
#endif
                  }
                  if (fd == -1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to open local file %s : %s",
                               local_tmp_file, strerror(errno));
                     (void)ftp_quit();
                     reset_values(files_retrieved, file_size_retrieved,
                                  files_to_retrieve, file_size_to_retrieve,
                                  (struct job *)&db);
                     exit(OPEN_LOCAL_ERROR);
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Opened local file %s.", local_tmp_file);
                     }
                  }

                  if (gsf_check_fsa((struct job *)&db) != NEITHER)
                  {
                     if (rl[i].size == -1)
                     {
                        fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                     }
                     else
                     {
                        fsa->job_status[(int)db.job_no].file_size_in_use = rl[i].size;
                     }
                     (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                                  rl[i].file_name);
                  }

                  bytes_done = 0;
                  if (fsa->trl_per_process > 0)
                  {
                     init_limit_transfer_rate();
                  }
                  do
                  {
                     if ((status = ftp_read(buffer, blocksize)) == INCORRECT)
                     {
                        status = errno;
                        if (status == EPIPE)
                        {
                           (void)ftp_get_reply();
                        }
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                  (status == EPIPE) ? msg_str : NULL,
                                  "Failed to read from remote file %s",
                                  rl[i].file_name);
                        reset_values(files_retrieved, file_size_retrieved,
                                     files_to_retrieve, file_size_to_retrieve,
                                     (struct job *)&db);
                        if (status == EPIPE)
                        {
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Hmm. Pipe is broken. Will NOT send a QUIT.");
                        }
                        else
                        {
                           (void)ftp_quit();
                        }
                        exit(eval_timeout(READ_REMOTE_ERROR));
                     }

                     if (fsa->trl_per_process > 0)
                     {
                        limit_transfer_rate(status, fsa->trl_per_process,
                                            clktck);
                     }
                     if (status > 0)
                     {
                        if (write(fd, buffer, status) != status)
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
                                     "Failed to write() to file %s : %s",
                                     local_tmp_file, strerror(errno));
                           (void)ftp_quit();
                           reset_values(files_retrieved, file_size_retrieved,
                                        files_to_retrieve,
                                        file_size_to_retrieve,
                                        (struct job *)&db);
                           exit(eval_timeout(WRITE_LOCAL_ERROR));
                        }
                        bytes_done += status;
                     }

                     if (gsf_check_fsa((struct job *)&db) != NEITHER)
                     {
                        fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                        fsa->job_status[(int)db.job_no].file_size_done += status;
                        fsa->job_status[(int)db.job_no].bytes_send += status;
                     }
                  } while (status != 0);

                  /* Close the FTP data connection. */
                  if ((status = ftp_close_data()) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                               "Failed to close data connection (%d).", status);
                     (void)ftp_quit();
                     reset_values(files_retrieved, file_size_retrieved,
                                  files_to_retrieve, file_size_to_retrieve,
                                  (struct job *)&db);
                     exit(eval_timeout(CLOSE_REMOTE_ERROR));
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                     "Closed data connection for file %s.",
                                     rl[i].file_name);
                     }
                  }

                  if (fsa->protocol_options & KEEP_TIME_STAMP)
                  {
                     struct utimbuf old_time;

                     old_time.actime = time(NULL);
                     if (rl[i].got_date != YES)
                     {
                        (void)ftp_date(rl[i].file_name, &rl[i].file_mtime);
                     }
                     old_time.modtime = rl[i].file_mtime;
                     if (utime(local_tmp_file, &old_time) == -1)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Failed to set time of file %s : %s",
                                  local_tmp_file, strerror(errno));
                     }
                  }

                  /* Close the local file. */
                  if (close(fd) == -1)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to close() local file %s : %s",
                               local_tmp_file, strerror(errno));
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Closed local file %s.", local_tmp_file);
                     }
                  }

                  if (fsa->host_status & HOST_ACTION_SUCCESS)
                  {
                     error_action(fsa->host_alias, "start",
                                  HOST_SUCCESS_ACTION);
                  }

                  /* Check if remote file is to be deleted. */
                  if (fra[db.fra_pos].remove == YES)
                  {
                     if ((status = ftp_dele(rl[i].file_name)) != SUCCESS)
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                                  "Failed to delete remote file %s (%d).",
                                  rl[i].file_name, status);
                     }
                     else
                     {
                        if (fsa->debug > NORMAL_MODE)
                        {
                           trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                                        "Deleted remote file %s.",
                                        rl[i].file_name);
                        }
                     }
                  }

                  if (gsf_check_fsa((struct job *)&db) != NEITHER)
                  {
#ifdef LOCK_DEBUG
                     lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                     lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
                     fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
                     fsa->job_status[(int)db.job_no].no_of_files_done++;
                     fsa->job_status[(int)db.job_no].file_size_in_use = 0;
                     fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

                     /* Total file counter. */
                     fsa->total_file_counter -= 1;
                     files_to_retrieve_shown -= 1;
#ifdef _VERIFY_FSA
                     if (fsa->total_file_counter < 0)
                     {
                        int tmp_val;

                        tmp_val = files_to_retrieve - (files_retrieved + 1);
                        if (tmp_val < 0)
                        {
                           tmp_val = 0;
                        }
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "Total file counter less then zero. Correcting to %d.",
                                   tmp_val);
                        fsa->total_file_counter = tmp_val;
                     }
#endif

                     /* Total file size. */
                     if ((rl[i].size != -1) && (bytes_done > 0))
                     {
                        /*
                         * If the file size is not the same as the one when we
                         * did the remote ls command, give a warning in the
                         * transfer log so some action can be taken against
                         * the originator.
                         */
                        if ((bytes_done + offset) != rl[i].size)
                        {
                           trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                     "File size of file %s changed from %ld to %ld when it was retrieved.",
#else
                                     "File size of file %s changed from %lld to %lld when it was retrieved.",
#endif
                                     rl[i].file_name, (pri_off_t)rl[i].size,
                                     (pri_off_t)(bytes_done + offset));
                           fsa->total_file_size += (bytes_done + offset - rl[i].size);
                           rl[i].size = bytes_done + offset;
                        }
                        fsa->total_file_size -= rl[i].size;
                        file_size_to_retrieve_shown -= rl[i].size;
#ifdef _VERIFY_FSA
                        if (fsa->total_file_size < 0)
                        {
                           off_t new_size = file_size_to_retrieve - file_size_retrieved;

                           if (new_size < 0)
                           {
                              new_size = 0;
                           }
                           fsa->total_file_size = new_size;
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                     "Total file size overflowed. Correcting to %ld.",
#else
                                     "Total file size overflowed. Correcting to %lld.",
#endif
                                     (pri_off_t)fsa->total_file_size);
                        }
                        else if ((fsa->total_file_counter == 0) &&
                                 (fsa->total_file_size > 0))
                             {
                                trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                          "fc is zero but fs is not zero (%ld). Correcting.",
#else
                                          "fc is zero but fs is not zero (%lld). Correcting.",
#endif
                                          (pri_off_t)fsa->total_file_size);
                                fsa->total_file_size = 0;
                             }
#endif
                     }

                     /* File counter done. */
                     fsa->file_counter_done += 1;

                     /* Number of bytes send. */
                     fsa->bytes_send += bytes_done;
#ifdef LOCK_DEBUG
                     unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                     unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif

                     (void)gsf_check_fra();
                     if (db.fra_pos != INCORRECT)
                     {
                        if (fra[db.fra_pos].error_counter > 0)
                        {
                           lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                         (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                         (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                           fra[db.fra_pos].error_counter = 0;
                           if (fra[db.fra_pos].dir_flag & DIR_ERROR_SET)
                           {
                              fra[db.fra_pos].dir_flag &= ~DIR_ERROR_SET;
                              SET_DIR_STATUS(fra[db.fra_pos].dir_flag,
                                             time(NULL),
                                             fra[db.fra_pos].start_event_handle,
                                             fra[db.fra_pos].end_event_handle,
                                             fra[db.fra_pos].dir_status);
                              error_action(fra[db.fra_pos].dir_alias, "stop",
                                           DIR_ERROR_ACTION);
                              event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s",
                                        fra[db.fra_pos].dir_alias);
                           }
                           unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                         (char *)&fra[db.fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                         (char *)&fra[db.fra_pos].error_counter - (char *)fra);
#endif
                        }
                     }

                     if (fsa->error_counter > 0)
                     {
                        int  fd, j;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        int  readfd;
#endif
                        char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
                        lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
                        fsa->error_counter = 0;

                        /* Wake up FD! */
                        (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                                      FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
                        if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to open() FIFO `%s' : %s",
                                      fd_wake_up_fifo, strerror(errno));
                        }
                        else
                        {
                           char dummy;

                           if (write(fd, &dummy, 1) != 1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to write() to FIFO `%s' : %s",
                                         fd_wake_up_fifo, strerror(errno));
                           }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           if (close(readfd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO `%s' (read) : %s",
                                         fd_wake_up_fifo, strerror(errno));
                           }
#endif
                           if (close(fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO `%s' : %s",
                                         fd_wake_up_fifo, strerror(errno));
                           }
                        }

                        /*
                         * Remove the error condition (NOT_WORKING) from all
                         * jobs of this host.
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
                         * Since we have successfully retrieved a file, no
                         * need to have the queue stopped anymore.
                         */
                        if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
                        {
                           char *sign;

#ifdef LOCK_DEBUG
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                           lock_region_w(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                           fsa->host_status &= ~AUTO_PAUSE_QUEUE_STAT;
                           if (fsa->host_status & HOST_ERROR_EA_STATIC)
                           {
                              fsa->host_status &= ~EVENT_STATUS_STATIC_FLAGS;
                           }
                           else
                           {
                              fsa->host_status &= ~EVENT_STATUS_FLAGS;
                           }
                           fsa->host_status &= ~PENDING_ERRORS;
#ifdef LOCK_DEBUG
                           unlock_region(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                           unlock_region(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                           error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION);
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
                           trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                                     "Starting input queue that was stopped by init_afd.");
                           event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                                     fsa->host_alias);
                        }
                     }
                  }
                  else
                  {
                     /*
                      * If the file size is not the same as the one when we did
                      * the remote ls command, give a warning in the transfer
                      * log so some action can be taken against the originator.
                      */
                     if ((rl[i].size != -1) &&
                         ((bytes_done + offset) != rl[i].size))
                     {
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                  "File size of file %s changed from %ld to %ld when it was retrieved.",
#else
                                  "File size of file %s changed from %lld to %lld when it was retrieved.",
#endif
                                  rl[i].file_name, (pri_off_t)rl[i].size,
                                  (pri_off_t)bytes_done + offset);
                        rl[i].size = bytes_done + offset;
                     }
                  }

                  /* Rename the file so AMG can grab it. */
                  if (rl[i].file_name[0] == '.')
                  {
                     (void)strcpy(p_local_file, &rl[i].file_name[1]);
                  }
                  else
                  {
                     (void)strcpy(p_local_file, rl[i].file_name);
                  }
                  if (rename(local_tmp_file, local_file) == -1)
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Failed to rename() %s to %s : %s",
                               local_tmp_file, local_file, strerror(errno));
                  }
                  else
                  {
                     if (fsa->debug > NORMAL_MODE)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                     "Renamed local file %s to %s.",
                                     local_tmp_file, local_file);
                     }
                     rl[i].retrieved = YES;
                  }
               }
               files_retrieved++;
               file_size_retrieved += bytes_done;
            } /* if (rl[i].retrieved == NO) */
         } /* for (i = 0; i < *no_of_listed_files; i++) */

         reset_values(files_retrieved, file_size_retrieved,
                      files_to_retrieve, file_size_to_retrieve,
                      (struct job *)&db);

         /* Free memory for the read buffer. */
         free(buffer);
      }
      else if ((files_to_retrieve == 0) && (fsa->error_counter > 0))
           {
              int  fd, j;
#ifdef WITHOUT_FIFO_RW_SUPPORT
              int  readfd;
#endif
              char fd_wake_up_fifo[MAX_PATH_LENGTH];

#ifdef LOCK_DEBUG
              lock_region_w(fsa_fd, db.lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
              lock_region_w(fsa_fd, db.lock_offset + LOCK_EC);
#endif
              fsa->error_counter = 0;

              /* Wake up FD! */
              (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                            FIFO_DIR, FD_WAKE_UP_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
              if (open_fifo_rw(fd_wake_up_fifo, &readfd, &fd) == -1)
#else
              if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to open() FIFO `%s' : %s",
                            fd_wake_up_fifo, strerror(errno));
              }
              else
              {
                 char dummy;

                 if (write(fd, &dummy, 1) != 1)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Failed to write() to FIFO `%s' : %s",
                               fd_wake_up_fifo, strerror(errno));
                 }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                 if (close(readfd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Failed to close() FIFO `%s' (read) : %s",
                               fd_wake_up_fifo, strerror(errno));
                 }
#endif
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Failed to close() FIFO `%s' : %s",
                               fd_wake_up_fifo, strerror(errno));
                 }
              }

              /*
               * Remove the error condition (NOT_WORKING) from all
               * jobs of this host.
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
               * Since we have successfully retrieved a file, no
               * need to have the queue stopped anymore.
               */
              if (fsa->host_status & AUTO_PAUSE_QUEUE_STAT)
              {
                 char *sign;

#ifdef LOCK_DEBUG
                 lock_region_w(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                 lock_region_w(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                 fsa->host_status &= ~AUTO_PAUSE_QUEUE_STAT;
                 if (fsa->host_status & HOST_ERROR_EA_STATIC)
                 {
                    fsa->host_status &= ~EVENT_STATUS_STATIC_FLAGS;
                 }
                 else
                 {
                    fsa->host_status &= ~EVENT_STATUS_FLAGS;
                 }
                 fsa->host_status &= ~PENDING_ERRORS;
#ifdef LOCK_DEBUG
                 unlock_region(fsa_fd, db.lock_offset + LOCK_HS, __FILE__, __LINE__);
#else
                 unlock_region(fsa_fd, db.lock_offset + LOCK_HS);
#endif
                 error_action(fsa->host_alias, "stop", HOST_ERROR_ACTION);
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
                 trans_log(sign, __FILE__, __LINE__, NULL, NULL,
                           "Starting input queue that was stopped by init_afd.");
                 event_log(0L, EC_HOST, ET_AUTO, EA_START_QUEUE, "%s",
                           fsa->host_alias);
              }
           }
   } while (((*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & DISABLE_RETRIEVE) == 0) &&
            ((more_files_in_list == YES) ||
             ((db.keep_connected > 0) && (ftp_timeup() == SUCCESS))));

   fsa->job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
   if ((status = ftp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, msg_str,
                "Failed to disconnect from remote host (%d).", status);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str, "Logged out.");
      }
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*+++++++++++++++++++++++++++++ gf_ftp_exit() +++++++++++++++++++++++++++*/
static void
gf_ftp_exit(void)
{
   if ((fsa != NULL) && (db.fsa_pos >= 0))
   {
      WHAT_DONE("retrieved", fsa->job_status[(int)db.job_no].file_size_done,
                fsa->job_status[(int)db.job_no].no_of_files_done);
      reset_fsa((struct job *)&db, exitflag, files_to_retrieve_shown,
                file_size_to_retrieve_shown);
   }

   send_proc_fin(NO);
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++ ftp_timeup() +++++++++++++++++++++++++++++*/
static int
ftp_timeup(void)
{
   time_t now,
          timeup;

   (void)gsf_check_fra();
   if (db.fra_pos == INCORRECT)
   {
      return(INCORRECT);
   }
   if (fra[db.fra_pos].keep_connected > 0)
   {
      db.keep_connected = fra[db.fra_pos].keep_connected;
   }
   else if ((fsa->keep_connected > 0) &&
            ((fsa->special_flag & KEEP_CON_NO_FETCH) == 0))
        {
           db.keep_connected = fsa->keep_connected;
        }
        else
        {
           db.keep_connected = 0;
           return(INCORRECT);
        }
   now = time(NULL);
   timeup = now + db.keep_connected;
   if (db.no_of_time_entries == 0)
   {
      fra[db.fra_pos].next_check_time = now + db.remote_file_check_interval;
   }
   else
   {
      fra[db.fra_pos].next_check_time = calc_next_time_array(db.no_of_time_entries,
                                                             db.te, now,
                                                             __FILE__, __LINE__);
   }
   if (fra[db.fra_pos].next_check_time > timeup)
   {
      return(INCORRECT);
   }
   else
   {
      if (fra[db.fra_pos].next_check_time < now)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_TIME_T == 4
                    "BUG in calc_next_time(): next_check_time (%ld) < now (%ld)",
#else
                    "BUG in calc_next_time(): next_check_time (%lld) < now (%lld)",
#endif
                    (pri_time_t)fra[db.fra_pos].next_check_time,
                    (pri_time_t)now);
         return(INCORRECT);
      }
      else
      {
         timeup = fra[db.fra_pos].next_check_time;
      }
   }
   if (gsf_check_fsa((struct job *)&db) != NEITHER)
   {
      int    status;
      time_t sleeptime = 0;

      if (fsa->protocol_options & STAT_KEEPALIVE)
      {
         sleeptime = fsa->transfer_timeout - 5;
      }
      if (sleeptime < 1)
      {
         sleeptime = DEFAULT_NOOP_INTERVAL;
      }
      if ((now + sleeptime) > timeup)
      {
         sleeptime = timeup - now;
      }
      fsa->job_status[(int)db.job_no].unique_name[2] = 5;
      do
      {
         (void)sleep(sleeptime);
         (void)gsf_check_fra();
         if (db.fra_pos == INCORRECT)
         {
            return(INCORRECT);
         }
         if (gsf_check_fsa((struct job *)&db) == NEITHER)
         {
            break;
         }
         if (fsa->job_status[(int)db.job_no].unique_name[2] == 6)
         {
            fsa->job_status[(int)db.job_no].unique_name[2] = '\0';
            return(INCORRECT);
         }
         now = time(NULL);
         if (now < timeup)
         {
            if ((status = ftp_noop()) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                         "Failed to send NOOP command (%d).", status);
               return(INCORRECT);
            }
            now = time(NULL);
         }
         if ((now + sleeptime) > timeup)
         {
            sleeptime = timeup - now;
         }
      } while (timeup > now);
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, files_to_retrieve_shown,
             file_size_to_retrieve_shown);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR, files_to_retrieve_shown,
             file_size_to_retrieve_shown);
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
