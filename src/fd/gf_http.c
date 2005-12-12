/*
 *  gf_http.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gf_http - gets files via HTTP
 **
 ** SYNOPSIS
 **   gf_http <work dir> <job no.> <FSA id> <FSA pos> <dir alias> [options]
 **
 **   options
 **      --version        Version Number
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
 **   18.11.2003 H.Kiehl Created
 **   13.06.2004 H.Kiehl Added transfer rate limit.
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
#include "httpdefs.h"
#include "fddefs.h"
#include "version.h"

int                        exitflag = IS_FAULTY_VAR,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           *p_no_of_hosts = NULL,
                           rl_fd = -1,
                           trans_db_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
long                       transfer_timeout;
char                       msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job                 db;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions */
static void                gf_http_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              blocksize,
                    chunksize,
                    fd,
                    files_retrieved = 0,
                    files_to_retrieve = 0,
                    i, j,
                    nfg,
                    status;
   off_t            bytes_done,
                    content_length,
                    file_size_retrieved = 0,
                    file_size_to_retrieve = 0,
                    offset;
   clock_t          clktck;
   char             *buffer,
                    *chunkbuffer = NULL,
                    local_file[MAX_PATH_LENGTH],
                    local_tmp_file[MAX_PATH_LENGTH],
                    *p_file_name,
                    *p_local_file,
                    *p_local_tmp_file,
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct file_mask *fml;
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
   if (atexit(gf_http_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   init_gf(argc, argv, HTTP_FLAG);
   msg_str[0] = '\0';
   timeout_flag = OFF;
   if (fsa->transfer_rate_limit > 0)
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
                   "Trying to connect to %s at port %d.",
                   db.hostname, db.port);
   }

   /* Connect to remote HTTP-server */
   if ((status = http_connect(db.hostname, db.port,
#ifdef WITH_SSL
                              db.user, db.password, db.auth)) != SUCCESS)
#else
                              db.user, db.password)) != SUCCESS)
#endif
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                "HTTP connection to %s at port %d failed (%d).",
                db.hostname, db.port, status);
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
#ifdef WITH_SSL
         char *p_msg_str;

         if ((db.auth == YES) || (db.auth == BOTH))
         {
            p_msg_str = msg_str;
         }
         else
         {
            p_msg_str = NULL;
         }
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, p_msg_str, "Connected.");
#else
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Connected.");
#endif
      }
   }

   /* Inform FSA that we have finished connecting and */
   /* will now start to retrieve data.                */
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
      fsa->job_status[(int)db.job_no].connect_status = HTTP_RETRIEVE_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = 0;
      fsa->job_status[(int)db.job_no].file_size = 0;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif

      /* Number of connections */
      fsa->connections += 1;
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 4)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes error : %s",
                 blocksize + 4, strerror(errno));
      http_quit();
      exit(ALLOC_ERROR);
   }

   /* Get directory where files are to be stored and */
   /* prepare some pointers for the file names.      */
   if (create_remote_dir(fra[db.fra_pos].url, local_file) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to determine local incoming directory for <%s>.",
                 fra[db.fra_pos].dir_alias);
      http_quit();
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

   /* Get all file masks for this directory. */
   if (read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to get the file masks.");
      if (fml != NULL)
      {
         free(fml);
      }
      http_quit();
      exit(INCORRECT);
   }

   /* Retrieve all files. */
   for (i = 0; i < nfg; i++)
   {
      p_file_name = fml[i].file_list;
      for (j = 0; j < fml[i].fc; j++)
      {
         (void)strcpy(p_local_tmp_file, p_file_name);
         if (!((i == 0) && (j == 0)))
         {
            /* The server may close the connection at any time. */
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Checking if connection is ok.");
            }
            if ((status = http_check_connection()) == CONNECTION_REOPENED)
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Reconnected.");
               }
            }
            else if (status == SUCCESS)
                 {
                    if (fsa->debug > NORMAL_MODE)
                    {
                       trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                    "Good! Connection still open.");
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Failed to check connection (%d)", status);
                    exit(eval_timeout(CONNECT_ERROR));
                 }
         }
         if ((status = http_get(db.hostname, db.target_dir, p_file_name,
                                &content_length)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                      "Failed to open remote file %s (%d).",
                      p_file_name, status);
            (void)http_quit();
            exit(eval_timeout(OPEN_REMOTE_ERROR));
         }
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Opened HTTP connection for file %s.",
                         p_file_name);
         }
         files_to_retrieve = 1;
         file_size_to_retrieve = content_length;
         (void)gsf_check_fsa();
         if (db.fsa_pos != INCORRECT)
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
            fsa->job_status[(int)db.job_no].no_of_files = files_to_retrieve;
            fsa->job_status[(int)db.job_no].file_size = file_size_to_retrieve;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif

            /* Total file counter */
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
            fsa->total_file_counter += files_to_retrieve;

            /* Total file size */
            fsa->total_file_size += file_size_to_retrieve;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
         }
         if ((content_length == 0) || (stat(local_tmp_file, &stat_buf) == -1))
         {
            offset = 0;
         }
         else
         {
            offset = stat_buf.st_size;
         }
         if ((offset > 0) && (content_length > 0))
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
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to open local file %s : %s",
                      local_tmp_file, strerror(errno));
            http_quit();
            reset_values(files_retrieved, file_size_retrieved,
                         files_to_retrieve, file_size_to_retrieve);
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

         (void)gsf_check_fsa();
         if (db.fsa_pos != INCORRECT)
         {
            fsa->job_status[(int)db.job_no].file_size_in_use = content_length;
            (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                         p_file_name);
         }
         bytes_done = 0;
         if (fsa->transfer_rate_limit > 0)
         {
            init_limit_transfer_rate();
         }
         if (content_length > 0)
         {
            do
            {
               if ((status = http_read(buffer, blocksize)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                            "Failed to read from remote file %s", p_file_name);
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve);
                  http_quit();
                  exit(eval_timeout(READ_REMOTE_ERROR));
               }
               if (fsa->transfer_rate_limit > 0)
               {
                  limit_transfer_rate(status, fsa->trl_per_process, clktck);
               }
               if (status > 0)
               {
                  if (write(fd, buffer, status) != status)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write() to file %s : %s",
                               local_tmp_file, strerror(errno));
                     http_quit();
                     reset_values(files_retrieved, file_size_retrieved,
                                  files_to_retrieve, file_size_to_retrieve);
                     exit(WRITE_LOCAL_ERROR);
                  }
                  bytes_done += status;
               }

               (void)gsf_check_fsa();
               if (db.fsa_pos != INCORRECT)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                  fsa->job_status[(int)db.job_no].file_size_done += status;
                  fsa->job_status[(int)db.job_no].bytes_send += status;
               }
            } while (status != 0);
         }
         else /* We need to read data in chunks dictated by the server. */
         {
            if (chunkbuffer == NULL)
            {
               if ((chunkbuffer = malloc(blocksize + 4)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to malloc() %d bytes error : %s",
                             blocksize + 4, strerror(errno));
                  http_quit();
                  (void)unlink(local_tmp_file);
                  exit(ALLOC_ERROR);
               }
               chunksize = blocksize + 4;
            }
            do
            {
               if ((status = http_chunk_read(&chunkbuffer,
                                             &chunksize)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                            "Failed to read from remote file %s", p_file_name);
                  reset_values(files_retrieved, file_size_retrieved,
                               files_to_retrieve, file_size_to_retrieve);
                  free(chunkbuffer);
                  http_quit();
                  (void)unlink(local_tmp_file);
                  exit(eval_timeout(READ_REMOTE_ERROR));
               }
               if (fsa->transfer_rate_limit > 0)
               {
                  limit_transfer_rate(status, fsa->trl_per_process, clktck);
               }
               if (status > 0)
               {
                  if (write(fd, chunkbuffer, status) != status)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write() to file %s : %s",
                               local_tmp_file, strerror(errno));
                     free(chunkbuffer);
                     http_quit();
                     (void)unlink(local_tmp_file);
                     reset_values(files_retrieved, file_size_retrieved,
                                  files_to_retrieve, file_size_to_retrieve);
                     exit(WRITE_LOCAL_ERROR);
                  }
                  bytes_done += status;
               }

               (void)gsf_check_fsa();
               if (db.fsa_pos != INCORRECT)
               {
                  fsa->job_status[(int)db.job_no].file_size_in_use_done = bytes_done;
                  fsa->job_status[(int)db.job_no].file_size_done += status;
                  fsa->job_status[(int)db.job_no].bytes_send += status;
               }
            } while (status != 0);
         }

         /* Close the local file. */
         if (close(fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to close() local file %s.", local_tmp_file);
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Closed local file %s.", local_tmp_file);
            }
         }

         /* Check if remote file is to be deleted. */
         if (fra[db.fra_pos].remove == YES)
         {
            /* The server may close the connection at any time. */
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                            "Checking if connection is ok.");
            }
            if ((status = http_check_connection()) == CONNECTION_REOPENED)
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Reconnected.");
               }
            }
            else if (status == SUCCESS)
                 {
                    if (fsa->debug > NORMAL_MODE)
                    {
                       trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                                    "Good! Connection still open.");
                    }
                 }
                 else
                 {
                    trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                              "Failed to check connection (%d)", status);
                    exit(eval_timeout(CONNECT_ERROR));
                 }

            if ((status = http_del(db.hostname, db.target_dir,
                                   p_file_name)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, msg_str,
                         "Failed to delete remote file %s (%d).",
                         p_file_name, status);
            }
            else
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
                               "Deleted remote file %s.",
                               p_file_name);
               }
            }
         }

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
            fsa->job_status[(int)db.job_no].file_size_in_use = 0;
            fsa->job_status[(int)db.job_no].file_size_in_use_done = 0;

            /* Total file counter */
            fsa->total_file_counter -= 1;
#ifdef _VERIFY_FSA
            if (fsa->total_file_counter < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file counter for host <%s> less then zero. Correcting to 0.",
                          fsa->host_dsp_name);
               fsa->total_file_counter = 0;
            }
#endif

            /* Total file size */
            if (content_length > 0)
            {
               fsa->total_file_size -= content_length;
#ifdef _VERIFY_FSA
               if (fsa->total_file_size < 0)
               {
                  off_t new_size = file_size_to_retrieve - file_size_retrieved;

                  if (new_size < 0)
                  {
                     new_size = 0;
                  }
                  fsa->total_file_size = new_size;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                             "Total file size for host <%s> overflowed. Correcting to %ld.",
#else
                             "Total file size for host <%s> overflowed. Correcting to %lld.",
#endif
                             fsa->host_dsp_name, fsa->total_file_size);
               }
               else if ((fsa->total_file_counter == 0) &&
                        (fsa->total_file_size > 0))
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "fc for host <%s> is zero but fs is not zero. Correcting.",
                                  fsa->host_dsp_name);
                       fsa->total_file_size = 0;
                    }
#endif
            }

            /* File counter done */
            fsa->file_counter_done += 1;

            /* Number of bytes send */
            fsa->bytes_send += bytes_done;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif

            if (fsa->error_counter > 0)
            {
               int  fd, j;
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
               if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
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
                  if (close(fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to close() FIFO `%s' : %s",
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
                  fsa->host_status ^= AUTO_PAUSE_QUEUE_STAT;
                  error_action(fsa->host_alias, "stop");
                  system_log(INFO_SIGN, __FILE__, __LINE__,
                             "Starting input queue for <%s> that was stopped by init_afd.",
                             fsa->host_alias);
               }
            }
         }

         /*
          * If the file size is not the same as the one when we did
          * the remote ls command, give a warning in the transfer log
          * so some action can be taken against the originator.
          */
         if ((content_length > 0) &&
             ((bytes_done + offset) != content_length))
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_OFF_T == 4
                      "File size of file %s changed from %ld to %u when it was retrieved.",
#else
                      "File size of file %s changed from %lld to %u when it was retrieved.",
#endif
                      p_file_name, content_length, bytes_done + offset);
         }

         /* Rename the file so AMG can grab it. */
         (void)strcpy(p_local_file, p_file_name);
         if (rename(local_tmp_file, local_file) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL,
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
            files_retrieved++;
            file_size_retrieved += bytes_done;
         }
         NEXT(p_file_name);
      }
   }

   reset_values(files_retrieved, file_size_retrieved, 0, 0);

   /* Free memory for the read buffer. */
   free(buffer);
   if (fml != NULL)
   {
      free(fml);
   }
   if (chunkbuffer != NULL)
   {
      free(chunkbuffer);
   }

   fsa->job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
#if SIZEOF_OFF_T == 4
   trans_log(INFO_SIGN, NULL, 0, NULL, "%ld Bytes retrieved in %d file(s).",
#else
   trans_log(INFO_SIGN, NULL, 0, NULL, "%lld Bytes retrieved in %d file(s).",
#endif
             file_size_retrieved, files_retrieved);
   http_quit();
   if (fsa->debug > NORMAL_MODE)
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "Logged out.");
   }

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*++++++++++++++++++++++++++++ gf_http_exit() +++++++++++++++++++++++++++*/
static void
gf_http_exit(void)
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
                 "Could not open fifo `%s' : %s",
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
