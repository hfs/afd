/*
 *  sf_ftp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Deutscher Wetterdienst (DWD),
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
 **   sf_ftp - send files via FTP
 **
 ** SYNOPSIS
 **   sf_ftp [options] -m <message-file>
 **
 **   options
 **       --version               - Version
 **       -w directory            - the working directory of the
 **                                 AFD
 **       -j <process number>     - the process number under which this
 **                                 job is to be displayed
 **       -f                      - error message
 **       -t                      - toggle host
 **       -m <message name>       - message name with the options
 **
 ** DESCRIPTION
 **   sf_ftp sends the given files to the defined recipient via FTP
 **   It does so by using it's own FTP-client.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <scheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 **       [options]
 **       <a list of FD options, terminated by a newline>
 **
 **   If the archive flag is set, each file will be archived after it
 **   has been send successful.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.1995 H.Kiehl Created
 **   22.01.1997 H.Kiehl Include support for output logging.
 **   03.02.1997 H.Kiehl Appending file when only send partly and an
 **                      error occurred.
 **   04.03.1997 H.Kiehl Ignoring error when closing remote file if
 **                      file size is not larger than zero.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   12.04.1998 H.Kiehl Added DOS binary mode.
 **   26.04.1999 H.Kiehl Added option "no login".
 **   29.01.2000 H.Kiehl Added ftp_size() command.
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   12.11.2000 H.Kiehl Added ftp_exec() command.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <ctype.h>                     /* isdigit()                      */
#if defined (_RADAR_CHECK) || defined (FTP_CTRL_KEEP_ALIVE_INTERVAL)
#include <time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), close()              */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "version.h"
#ifdef _WITH_EUMETSAT_HEADERS
#include "eumetsat_header_defs.h"
#endif

/* Global variables */
int                        counter_fd = -1,
                           exitflag = IS_FAULTY_VAR,
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           trans_rule_pos,
                           user_rule_pos, /* Not used here [init_sf]     */
                           fsa_id,
                           fsa_fd = -1,
#ifdef _BURST_MODE
                           burst_counter = 0,
#endif
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO,
                           timeout_flag;
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0,
                           total_append_count = 0;
#endif /* _WITH_BURST_2 */
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      append_offset = 0,
                           *file_size_buffer = NULL;
long                       transfer_timeout;
char                       *file_name_buffer = NULL,
                           host_deleted = NO,
                           initial_filename[MAX_FILENAME_LENGTH],
                           msg_str[MAX_RET_MSG_LENGTH],
                           *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct filetransfer_status *fsa;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

/* Local functions */
static void                sf_ftp_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);

/* #define _DEBUG_APPEND 1 */
/* #define _SIMULATE_SLOW_TRANSFER 2 */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _VERIFY_FSA
   unsigned int     ui_variable;
#endif
   int              exit_status = TRANSFER_SUCCESS,
                    j,
                    fd,
#ifdef _WITH_INTERRUPT_JOB
                    interrupt = NO,
#endif
                    status,
                    bytes_buffered,
                    append_file_number = -1,
                    files_to_send,
                    files_send = 0,
                    blocksize;
   off_t            lock_offset,
                    no_of_bytes;
#ifdef _WITH_BURST_2
   int              disconnect = NO;
   unsigned int     values_changed = 0;
#endif /* _WITH_BURST_2 */
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
   clock_t          end_time = 0,
                    start_time = 0,
                    *ol_transfer_time = NULL;
   struct tms       tmsdummy;
#endif /* _OUTPUT_LOG */
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   int              do_keep_alive_check = 1;
   time_t           keep_alive_time;
#endif
   off_t            *p_file_size_buffer;
   char             *ptr,
                    *ascii_buffer = NULL,
                    *p_file_name_buffer,
#ifdef _BURST_MODE
                    search_for_files = NO,
#endif
                    append_count = 0,
                    *buffer,
                    final_filename[MAX_FILENAME_LENGTH],
                    remote_filename[MAX_PATH_LENGTH],
                    fullname[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct job       *p_db;
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
   if (atexit(sf_ftp_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   files_to_send = init_sf(argc, argv, file_path, FTP_FLAG);
   p_db = &db;
   msg_str[0] = '\0';
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

   /*
    * In ASCII-mode an extra buffer is needed to convert LF's
    * to CRLF. By creating this buffer the function ftp_write()
    * knows it has to send the data in ASCII-mode.
    */
   if ((db.transfer_mode == 'A') || (db.transfer_mode == 'D'))
   {
      if (db.transfer_mode == 'D')
      {
         db.transfer_mode = 'I';
      }
      if ((ascii_buffer = malloc((blocksize * 2))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(ALLOC_ERROR);
      }
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      output_log_ptrs(&ol_fd,                /* File descriptor to fifo */
                      &ol_job_number,
                      &ol_data,              /* Pointer to buffer       */
                      &ol_file_name,
                      &ol_file_name_length,
                      &ol_archive_name_length,
                      &ol_file_size,
                      &ol_size,
                      &ol_transfer_time,
                      db.host_alias,
                      FTP);
   }
#endif

   timeout_flag = OFF;

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

   /* Connect to remote FTP-server. */
   if (((status = ftp_connect(db.hostname, db.port)) != SUCCESS) &&
       (status != 230))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__,
                "FTP %s connection to <%s> at port %d failed (%d).",
                (db.mode_flag == ACTIVE_MODE) ? "active" : "passive",
                db.hostname, db.port, status);
      exit((timeout_flag == ON) ? TIMEOUT_ERROR : CONNECT_ERROR);
   }
   else
   {
      if (fsa[db.fsa_pos].debug == YES)
      {
         if (status == 230)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Connected (%s). No user and password required, logged in.",
                         (db.mode_flag == ACTIVE_MODE) ? "active" : "passive");
         }
         else
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Connected (%s).",
                         (db.mode_flag == ACTIVE_MODE) ? "active" : "passive");
         }
      }
   }

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
#ifdef _BURST_MODE
         (void)memcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].unique_name,
                      db.msg_name, MAX_MSG_NAME_LENGTH);
         fsa[db.fsa_pos].job_status[(int)db.job_no].error_file = db.error_file;
         fsa[db.fsa_pos].job_status[(int)db.job_no].job_id = db.job_id;
         search_for_files = NO;
         burst_counter = 0;
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
#endif /* _BURST_MODE */
         if (values_changed & USER_CHANGED)
         {
            status = 0;
         }
         else
         {
            status = 230;
         }
#ifdef _OUTPUT_LOG
         if ((db.output_log == YES) && (ol_data == NULL))
         {
            output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
                            &ol_file_name_length, &ol_archive_name_length,
                            &ol_file_size, &ol_size, &ol_transfer_time,
                            db.host_alias, FTP);
         }
#endif /* _OUTPUT_LOG */
      }
#endif /* _WITH_BURST_2 */

   /* Login */
   if (status != 230) /* We are not already logged in! */
   {
      if (fsa[db.fsa_pos].proxy_name[0] == '\0')
      {
#ifdef _WITH_BURST_2
         /* Send user name. */
         if ((disconnect == YES) ||
             (((status = ftp_user(db.user)) != SUCCESS) && (status != 230)))
         {
            if ((disconnect == YES) ||
                ((burst_2_counter > 0) && ((status == 500) || (status == 530))))
            {
               /*
                * Aaargghh..., we need to logout again! The server is
                * not able to handle more than one USER request.
                * We should use the REIN (REINITIALIZE) command here,
                * however it seems most FTP-servers have this not
                * implemented.
                */
               if ((status = ftp_quit()) != SUCCESS)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to disconnect from remote host (%d).", status);
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : QUIT_ERROR);
               }
               else
               {
                  if (fsa[db.fsa_pos].debug == YES)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Logged out. Needed for burst.");
                  }
               }

               /* Connect to remote FTP-server. */
               if (((status = ftp_connect(db.hostname, db.port)) != SUCCESS) &&
                   (status != 230))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FTP connection to <%s> at port %d failed (%d).",
                            db.hostname, db.port, status);
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : CONNECT_ERROR);
               }
               else
               {
                  if (fsa[db.fsa_pos].debug == YES)
                  {
                     if (status == 230)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Connected. No user and password required, logged in.");
                     }
                     else
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Connected.");
                     }
                  }
               }

               if (status != 230) /* We are not already logged in! */
               {
                  /* Send user name. */
                  if (((status = ftp_user(db.user)) != SUCCESS) &&
                      (status != 230))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to send user <%s> (%d).",
                               db.user, status);
                     (void)ftp_quit();
                     exit((timeout_flag == ON) ? TIMEOUT_ERROR : USER_ERROR);
                  }
               }
               else
               {
                  if (fsa[db.fsa_pos].debug == YES)
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

               /*
                * Since we did a new connect we must set the transfer type
                * again. Or else we will transfer files in ASCII mode.
                */
               values_changed |= TYPE_CHANGED;
               disconnect = YES;
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send user <%s> (%d).", db.user, status);
               (void)ftp_quit();
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : USER_ERROR);
            }
         }
#else
         /* Send user name. */
         if (((status = ftp_user(db.user)) != SUCCESS) && (status != 230))
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to send user <%s> (%d).", db.user, status);
            (void)ftp_quit();
            exit(USER_ERROR);
         }
#endif
         else
         {
            if (fsa[db.fsa_pos].debug == YES)
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

         /* Send password (if required). */
         if (status != 230)
         {
            if ((status = ftp_pass(db.password)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to send password for user <%s> (%d).",
                         db.user, status);
               (void)ftp_quit();
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : PASSWORD_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
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

   /* Check if we need to set the idle time for remote FTP-server. */
#ifdef _WITH_BURST_2
   if ((fsa[db.fsa_pos].protocol & SET_IDLE_TIME) &&
       (burst_2_counter == 0))
#else
   if (fsa[db.fsa_pos].protocol & SET_IDLE_TIME)
#endif
   {
      if ((status = ftp_idle(transfer_timeout)) != SUCCESS)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to set IDLE time to <%d> (%d).",
                   transfer_timeout, status);
      }
      else
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Changed IDLE time to %d.", transfer_timeout);
         }
      }
   }

#ifdef _WITH_BURST_2
   if ((burst_2_counter != 0) && (db.transfer_mode == 'I') &&
       (ascii_buffer != NULL))
   {
      free(ascii_buffer);
      ascii_buffer = NULL;
   }
   if ((burst_2_counter == 0) || (values_changed & TYPE_CHANGED))
   {
#endif /* _WITH_BURST_2 */
      /*
       * In ASCII-mode an extra buffer is needed to convert LF's
       * to CRLF. By creating this buffer the function ftp_write()
       * knows it has to send the data in ASCII-mode.
       */
      if ((db.transfer_mode == 'A') || (db.transfer_mode == 'D'))
      {
         if (db.transfer_mode == 'D')
         {
            db.transfer_mode = 'I';
         }
         if (ascii_buffer == NULL)
         {
            if ((ascii_buffer = malloc((blocksize * 2))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s", strerror(errno));
               exit(ALLOC_ERROR);
            }
         }
      }

      /* Set transfer mode. */
      if ((status = ftp_type(db.transfer_mode)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to set transfer mode to <%c> (%d).",
                   db.transfer_mode, status);
         (void)ftp_quit();
         exit((timeout_flag == ON) ? TIMEOUT_ERROR : TYPE_ERROR);
      }
      else
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Changed transfer mode to %c.", db.transfer_mode);
         }
      }
#ifdef _WITH_BURST_2
   } /* ((burst_2_counter == 0) || (values_changed & TYPE_CHANGED)) */
#endif /* _WITH_BURST_2 */

#ifdef _WITH_BURST_2
   if ((burst_2_counter == 0) || (values_changed & TARGET_DIR_CHANGED))
   {
      /*
       * We must go to the home directory of the user when the target
       * directory is not the absolute path.
       */
      if ((burst_2_counter > 0) && (db.target_dir[0] != '/') &&
          (disconnect == NO))
      {
         if ((status = ftp_cd("")) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to change to home directory (%d).", status);
            (void)ftp_quit();
            exit((timeout_flag == ON) ? TIMEOUT_ERROR : CHDIR_ERROR);
         }
         else
         {
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Changed to home directory.");
            }
         }
      }
#endif /* _WITH_BURST_2 */
      /* Change directory if necessary. */
      if (db.target_dir[0] != '\0')
      {
         if ((status = ftp_cd(db.target_dir)) != SUCCESS)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to change directory to <%s> (%d).",
                      db.target_dir, status);
            (void)ftp_quit();
            exit((timeout_flag == ON) ? TIMEOUT_ERROR : CHDIR_ERROR);
         }
         else
         {
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Changed directory to %s.", db.target_dir);
            }
         }
      } /* if (db.target_dir[0] != '\0') */
#ifdef _WITH_BURST_2
   } /* ((burst_2_counter == 0) || (values_changed & TARGET_DIR_CHANGED)) */
#endif /* _WITH_BURST_2 */

   /* Inform FSA that we have finished connecting and */
   /* will now start to transfer data.                */
#ifdef _WITH_BURST_2
   if ((host_deleted == NO) && (burst_2_counter == 0))
#else
   if (host_deleted == NO)
#endif
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
         fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = FTP_ACTIVE;
         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = files_to_send;

         /* Number of connections. */
         lock_region_w(fsa_fd,
                       (char *)&fsa[db.fsa_pos].connections - (char *)fsa);
         fsa[db.fsa_pos].connections += 1;
         unlock_region(fsa_fd,
                       (char *)&fsa[db.fsa_pos].connections - (char *)fsa);
         unlock_region(fsa_fd, lock_offset);
      }
   }

   /* If we send a lock file, do it now. */
   if (db.lock == LOCKFILE)
   {
      /* Create lock file on remote host */
      if ((status = ftp_data(LOCK_FILENAME, 0, db.mode_flag,
                             DATA_WRITE)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to send lock file <%s> (%d).", LOCK_FILENAME, status);
         (void)ftp_quit();
         exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_LOCK_ERROR);
      }
      else
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Created lock file %s.", LOCK_FILENAME);
         }
      }

      /* Close remote lock file */
      if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to close lock file <%s> (%d).",
                   LOCK_FILENAME, status);
         (void)ftp_quit();
         exit((timeout_flag == ON) ? TIMEOUT_ERROR : CLOSE_REMOTE_ERROR);
      }
      else
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Closed data connection for remote lock file <%s>.",
                         LOCK_FILENAME);
         }
      }
   }

#ifdef _WITH_BURST_2
   if (burst_2_counter == 0)
   {
#endif /* _WITH_BURST_2 */
      /* Allocate buffer to read data from the source file. */
      if ((buffer = malloc(blocksize + 4)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         (void)ftp_quit();
         exit(ALLOC_ERROR);
      }
#ifdef _WITH_BURST_2
   } /* (burst_2_counter == 0) */
#endif /* _WITH_BURST_2 */

#ifdef _BURST_MODE
   do
   {
      if (search_for_files == YES)
      {
         off_t file_size_to_send = 0;

         lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);
         if (check_fsa() == YES)
         {
            if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
               lock_offset = -1;
            }
            else
            {
               lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
               lock_region_w(fsa_fd,
                             (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
            }
         }
         if ((files_to_send = get_file_names(file_path,
                                             &file_size_to_send)) < 1)
         {
            /*
             * With age limit it can happen that files_to_send is zero.
             * If that is the case, ie files_to_send is -1, do not
             * indicate an error.
             */
            if (files_to_send == 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm. Burst counter = %d and files_to_send = %d [%s]. How is this possible? [PID = %d] [job_no = %d] AAarrgghhhhh....",
                          fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter,
                          files_to_send, file_path, db.my_pid, (int)db.job_no);
            }
            fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter = 0;
            if (lock_offset != -1)
            {
               unlock_region(fsa_fd, lock_offset);
            }
            break;
         }
         burst_counter = fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter;
         unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);

         /* Tell user we are bursting. */
         if (host_deleted == NO)
         {
            if (check_fsa() == YES)
            {
               if ((db.fsa_pos = get_host_position(fsa,
                                                   db.host_alias,
                                                   no_of_hosts)) == INCORRECT)
               {
                  host_deleted = YES;
                  lock_offset = -1;
               }
               else
               {
                  lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
                  rlock_region(fsa_fd, lock_offset);
               }
            }
            if (host_deleted == NO)
            {
               fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = FTP_BURST_TRANSFER_ACTIVE;
               fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done + files_to_send;
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size = fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done + file_size_to_send;
            }
            if (fsa[db.fsa_pos].debug == YES)
            {
               msg_str[0] = '\0';
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Bursting.");
            }
         }
         if (lock_offset != -1)
         {
            unlock_region(fsa_fd, lock_offset);
         }
      }
#endif

      /* Send all files. */
#ifdef _WITH_INTERRUPT_JOB
      interrupt = NO;
#endif
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
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
               if ((fsa[db.fsa_pos].active_transfers > 1) &&
                   (*p_file_size_buffer > blocksize))
               {
                  int file_is_duplicate = NO;

                  lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] - (char *)fsa);

                  /*
                   * Check if this file is not currently being transfered!
                   */
                  for (j = 0; j < fsa[db.fsa_pos].allowed_transfers; j++)
                  {
                     if ((j != db.job_no) &&
                         (fsa[db.fsa_pos].job_status[j].job_id == fsa[db.fsa_pos].job_status[(int)db.job_no].job_id) &&
                         (CHECK_STRCMP(fsa[db.fsa_pos].job_status[j].file_name_in_use, p_file_name_buffer) == 0))
                     {
#ifdef _DELETE_LOG
                        int    prog_name_length;
                        size_t dl_real_size;

                        if (dl.fd == -1)
                        {
                           delete_log_ptrs(&dl);
                        }
                        (void)strcpy(dl.file_name, p_file_name_buffer);
                        (void)sprintf(dl.host_name, "%-*s %x",
                                      MAX_HOSTNAME_LENGTH,
                                      fsa[db.fsa_pos].host_dsp_name,
                                      OTHER_DEL);
                        *dl.file_size = *p_file_size_buffer;
                        *dl.job_number = db.job_id;
                        *dl.file_name_length = strlen(p_file_name_buffer);
                        prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                                   "%s Duplicate file",
                                                   SEND_FILE_FTP);
                        dl_real_size = *dl.file_name_length + dl.size +
                                       prog_name_length;
                        if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "write() error : %s", strerror(errno));
                        }
#endif /* _DELETE_LOG */
                        (void)sprintf(fullname, "%s/%s",
                                      file_path, p_file_name_buffer);
                        if (unlink(fullname) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to unlink() duplicate file <%s> : %s",
                                      fullname, strerror(errno));
                        }
                        msg_str[0] = '\0';
                        trans_log(WARN_SIGN, __FILE__, __LINE__,
                                  "File <%s> is currently transmitted by job %d. Will NOT send file again!",
                                  p_file_name_buffer, j);

                        fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done++;

                        /* Total file counter */
                        lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
                        fsa[db.fsa_pos].total_file_counter -= 1;
#ifdef _VERIFY_FSA
                        if (fsa[db.fsa_pos].total_file_counter < 0)
                        {
                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      "Total file counter for host %s less then zero. Correcting.",
                                      fsa[db.fsa_pos].host_dsp_name);
                           fsa[db.fsa_pos].total_file_counter = 0;
                        }
#endif

                        /* Total file size */
#ifdef _VERIFY_FSA
                        ui_variable = fsa[db.fsa_pos].total_file_size;
#endif
                        fsa[db.fsa_pos].total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
                        if (fsa[db.fsa_pos].total_file_size > ui_variable)
                        {
                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      "Total file size for host %s overflowed. Correcting.",
                                      fsa[db.fsa_pos].host_dsp_name);
                           fsa[db.fsa_pos].total_file_size = 0;
                        }
                        else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
                                 (fsa[db.fsa_pos].total_file_size > 0))
                             {
                                system_log(INFO_SIGN, __FILE__, __LINE__,
                                           "fc for host %s is zero but fs is not zero. Correcting.",
                                           fsa[db.fsa_pos].host_dsp_name);
                                fsa[db.fsa_pos].total_file_size = 0;
                             }
#endif
                        unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);

                        file_is_duplicate = YES;
                        p_file_name_buffer += MAX_FILENAME_LENGTH;
                        p_file_size_buffer++;
                        break;
                     }
                  } /* for (j = 0; j < allowed_transfers; j++) */

                  if (file_is_duplicate == NO)
                  {
                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
                     (void)strcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use,
                                  p_file_name_buffer);
                     unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] - (char *)fsa);
                  }
                  else
                  {
                     unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] - (char *)fsa);
                     continue;
                  }
               }
               else
               {
                  fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
                  (void)strcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use,
                               p_file_name_buffer);
               }
            }
         }

         (void)strcpy(final_filename, p_file_name_buffer);
         (void)sprintf(fullname, "%s/%s", file_path, final_filename);

         /* Send file in dot notation? */
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            (void)strcpy(initial_filename, db.lock_notation);
            (void)strcat(initial_filename, final_filename);
         }
         else
         {
            (void)strcpy(initial_filename, final_filename);
            if (db.lock == POSTFIX)
            {
               (void)strcat(initial_filename, db.lock_notation);
            }
         }

         /*
          * Check if the file has not already been partly
          * transmitted. If so, lets first get the size of the
          * remote file, to append it.
          */
         append_offset = 0;
         append_file_number = -1;
         if ((fsa[db.fsa_pos].file_size_offset != -1) &&
             (db.no_of_restart_files > 0))
         {
            int ii;

            for (ii = 0; ii < db.no_of_restart_files; ii++)
            {
               if ((CHECK_STRCMP(db.restart_file[ii], initial_filename) == 0) &&
                   (append_compare(db.restart_file[ii], fullname) == YES))
               {
                  append_file_number = ii;
                  break;
               }
            }
            if (append_file_number != -1)
            {
               if (fsa[db.fsa_pos].file_size_offset == AUTO_SIZE_DETECT)
               {
                  off_t remote_size;

                  if ((status = ftp_size(initial_filename, &remote_size)) != SUCCESS)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Failed to send SIZE command for file <%s> (%d).",
                               initial_filename, status);
                     if (timeout_flag == ON)
                     {
                        timeout_flag = OFF;
                     }
                  }
                  else
                  {
                     append_offset = remote_size;
                     if (fsa[db.fsa_pos].debug == YES)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Remote size of <%s> is %d.",
                                     initial_filename, remote_size);
                     }
                  }
               }
               else
               {
                  char line_buffer[MAX_RET_MSG_LENGTH];

                  if ((status = ftp_list(db.mode_flag, LIST_CMD,
                                         initial_filename,
                                         line_buffer)) != SUCCESS)
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Failed to send LIST command for file <%s> (%d).",
                               initial_filename, status);
                     if (timeout_flag == ON)
                     {
                        timeout_flag = OFF;
                     }
                  }
                  else if (line_buffer[0] != '\0')
                       {
                          int  space_count = 0;
                          char *p_end_line,
                               *ptr = line_buffer;

                          /*
                           * Cut out remote file size, from ls command.
                           */
                          p_end_line = line_buffer + strlen(line_buffer);
                          do
                          {
                             while ((*ptr != ' ') && (*ptr != '\t') &&
                                    (ptr < p_end_line))
                             {
                                ptr++;
                             }
                             if ((*ptr == ' ') || (*ptr == '\t'))
                             {
                                space_count++;
                                while (((*ptr == ' ') || (*ptr == '\t')) &&
                                       (ptr < p_end_line))
                                {
                                   ptr++;
                                }
                             }
                             else
                             {
                                if (*(p_end_line - 1) == '\n')
                                {
                                   *(p_end_line - 1) = '\0';
                                }
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Assuming <file size offset> for host %s is to large! [%s]",
                                           tr_hostname, line_buffer);
                                space_count = -1;
                                break;
                             }
                          } while (space_count != fsa[db.fsa_pos].file_size_offset);

                          if ((space_count > -1) &&
                              (space_count == fsa[db.fsa_pos].file_size_offset))
                          {
                             char *p_end = ptr;

                             while ((isdigit(*p_end) != 0) &&
                                    (p_end < p_end_line))
                             {
                                p_end++;
                             }
                             *p_end = '\0';
                             append_offset = atoi(ptr);
                             if (fsa[db.fsa_pos].debug == YES)
                             {
                                trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                             "Remote size of <%s> is %d.",
                                             initial_filename, append_offset);
                             }
                          }
                       }
               }
               if (append_offset > 0)
               {
                  fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += append_offset;
                  fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = append_offset;
               }
            } /* if (append_file_number != -1) */
         }

         no_of_bytes = 0;
         if ((append_offset < *p_file_size_buffer) ||
             (*p_file_size_buffer == 0))
         {
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
            int keep_alive_check_interval = -1;
#endif

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               start_time = times(&tmsdummy);
            }
#endif

            /* Open file on remote site */
            if ((status = ftp_data(initial_filename, append_offset,
                                   db.mode_flag, DATA_WRITE)) != SUCCESS)
            {
               if ((db.rename_file_busy != '\0') && (timeout_flag != ON) &&
                   (posi(msg_str, "Cannot open or remove a file containing a running program.") != NULL))
               {
                  size_t length = strlen(initial_filename);

                  initial_filename[length] = db.rename_file_busy;
                  initial_filename[length + 1] = '\0';
                  if ((status = ftp_data(initial_filename, 0,
                                         db.mode_flag, DATA_WRITE)) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to open remote file <%s> (%d).",
                               initial_filename, status);
                     (void)ftp_quit();
                     exit((timeout_flag == ON) ? TIMEOUT_ERROR : OPEN_REMOTE_ERROR);
                  }
                  else
                  {
                     trans_log(WARN_SIGN, __FILE__, __LINE__,
                               "Internal rename to `%s' due to remote error.",
                               initial_filename);
                     if (fsa[db.fsa_pos].debug == YES)
                     {
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Open remote file <%s>.",
                                     initial_filename);
                     }
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to open remote file <%s> (%d).",
                            initial_filename, status);
                  (void)ftp_quit();
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : OPEN_REMOTE_ERROR);
               }
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Open remote file <%s>.", initial_filename);
               }
            }

#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
            if ((fsa[db.fsa_pos].protocol & STAT_KEEPALIVE) &&
                (do_keep_alive_check))
            {
               keep_alive_time = time(NULL);
            }
#endif

            /* Open local file */
            if ((fd = open(fullname, O_RDONLY)) == -1)
            {
               msg_str[0] = '\0';
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open local file <%s> : %s",
                         fullname, strerror(errno));
               (void)ftp_quit();
               exit(OPEN_LOCAL_ERROR);
            }
            if (fsa[db.fsa_pos].debug == YES)
            {
               msg_str[0] = '\0';
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Open local file <%s>", fullname);
            }
            if (append_offset > 0)
            {
               if ((*p_file_size_buffer - append_offset) > 0)
               {
                  if (lseek(fd, append_offset, SEEK_SET) < 0)
                  {
                     append_offset = 0;
                     msg_str[0] = '\0';
                     trans_log(WARN_SIGN, __FILE__, __LINE__,
                               "Failed to seek() in <%s> (Ignoring append): %s",
                               fullname, strerror(errno));
                  }
                  else
                  {
                     append_count++;
                     if (fsa[db.fsa_pos].debug == YES)
                     {
                        msg_str[0] = '\0';
                        trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                     "Appending file <%s> at %d.",
                                     fullname, append_offset);
                     }
                  }
               }
               else
               {
                  append_offset = 0;
               }
            }

#ifdef _WITH_EUMETSAT_HEADERS
            if ((db.special_flag & ADD_EUMETSAT_HEADER) &&
                (append_offset == 0) &&
                (db.special_ptr != NULL))
            {
               struct stat stat_buf;

               if (fstat(fd, &stat_buf) == -1)
               {
                  msg_str[0] = '\0';
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Hmmm. Failed to stat() <%s> : %s",
                            fullname, strerror(errno));
               }
               else
               {
                  size_t header_length;
                  char   *p_header;

                  if ((p_header = create_eumetsat_header(db.special_ptr,
                                                         (unsigned char)db.special_ptr[4],
                                                         *p_file_size_buffer,
                                                         stat_buf.st_mtime,
                                                         &header_length)) != NULL)
                  {
                     if ((status = ftp_write(p_header, NULL, header_length)) != SUCCESS)
                     {
                        /*
                         * It could be that we have received a SIGPIPE
                         * signal. If this is the case there might be data
                         * from the remote site on the control connection.
                         * Try to read this data into the global variable
                         * 'msg_str'.
                         */
                        msg_str[0] = '\0';
                        if (status == EPIPE)
                        {
                           (void)ftp_get_reply();
                        }
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to write EUMETSAT header to remote file <%s>",
                                  initial_filename);
                        if (status == EPIPE)
                        {
                           /*
                            * When pipe is broken no nead to send a QUIT
                            * to the remote side since the connection has
                            * already been closed by the remote side.
                            */
                           msg_str[0] = '\0';
                           trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Hmm. Pipe is broken. Will NOT send a QUIT.");
                        }
                        else
                        {
                           (void)ftp_quit();
                        }
                        exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_REMOTE_ERROR);
                     }
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
                           fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += header_length;
                           fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += header_length;
                        }
                     }
                     free(p_header);
                  }
               }
            }
#endif
            if ((db.special_flag & FILE_NAME_IS_HEADER) &&
                (append_offset == 0))
            {
               int  header_length;
               char *ptr = p_file_name_buffer;

               buffer[0] = 1; /* SOH */
               buffer[1] = '\015'; /* CR */
               buffer[2] = '\015'; /* CR */
               buffer[3] = '\012'; /* LF */
               header_length = 4;

               for (;;)
               {
                  while ((*ptr != '_') && (*ptr != '-') &&
                         (*ptr != ' ') && (*ptr != '\0') && (*ptr != ';'))
                  {
                     buffer[header_length] = *ptr;
                     header_length++; ptr++;
                  }
                  if ((*ptr == '\0') || (*ptr == ';'))
                  {
                     break;
                  }
                  else
                  {
                     buffer[header_length] = ' ';
                     header_length++; ptr++;
                  }
               }
               buffer[header_length] = '\015'; /* CR */
               buffer[header_length + 1] = '\015'; /* CR */
               buffer[header_length + 2] = '\012'; /* LF */
               header_length += 3;

               if ((status = ftp_write(buffer, ascii_buffer, header_length)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  msg_str[0] = '\0';
                  if (status == EPIPE)
                  {
                     (void)ftp_get_reply();
                  }
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to write WMO header to remote file <%s>",
                            initial_filename);
                  if (status == EPIPE)
                  {
                     /*
                      * When pipe is broken no nead to send a QUIT
                      * to the remote side since the connection has
                      * already been closed by the remote side.
                      */
                     msg_str[0] = '\0';
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Hmm. Pipe is broken. Will NOT send a QUIT.");
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_REMOTE_ERROR);
               }
               if (host_deleted == NO)
               {
                  if (check_fsa() == YES)
                  {
                     if ((db.fsa_pos = get_host_position(fsa,
                                                         db.host_alias,
                                                         no_of_hosts)) == INCORRECT)
                     {
                        host_deleted = YES;
                     }
                  }
                  if (host_deleted == NO)
                  {
                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += header_length;
                     fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += header_length;
                  }
               }
            }

            /* Read (local) and write (remote) file. */
            do
            {
#ifdef _SIMULATE_SLOW_TRANSFER
               (void)sleep(_SIMULATE_SLOW_TRANSFER);
#endif
               if ((bytes_buffered = read(fd, buffer, blocksize)) < 0)
               {
                  msg_str[0] = '\0';
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Could not read() local file <%s> [%d] : %s",
                            fullname, bytes_buffered, strerror(errno));
                  (void)ftp_quit();
                  exit(READ_LOCAL_ERROR);
               }
               if (bytes_buffered > 0)
               {
#ifdef _DEBUG_APPEND
                  if (((status = ftp_write(buffer, ascii_buffer, bytes_buffered)) != SUCCESS) ||
                      (fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done > MAX_SEND_BEFORE_APPEND))
#else
                  if ((status = ftp_write(buffer, ascii_buffer,
                                          bytes_buffered)) != SUCCESS)
#endif
                  {
                     /*
                      * It could be that we have received a SIGPIPE
                      * signal. If this is the case there might be data
                      * from the remote site on the control connection.
                      * Try to read this data into the global variable
                      * 'msg_str'.
                      */
                     msg_str[0] = '\0';
                     if (status == EPIPE)
                     {
                        (void)ftp_get_reply();
                     }
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to write %d Bytes to remote file <%s>",
                               bytes_buffered, initial_filename);
                     if (status == EPIPE)
                     {
                        /*
                         * When pipe is broken no nead to send a QUIT
                         * to the remote side since the connection has
                         * already been closed by the remote side.
                         */
                        msg_str[0] = '\0';
                        trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Hmm. Pipe is broken. Will NOT send a QUIT.");
                     }
                     else
                     {
                        (void)ftp_quit();
                     }
                     exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_REMOTE_ERROR);
                  }

                  no_of_bytes += bytes_buffered;
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
                           fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes + append_offset;
                           fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += bytes_buffered;
                           fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += bytes_buffered;
                     }
                  }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                  if ((fsa[db.fsa_pos].protocol & STAT_KEEPALIVE) &&
                      (do_keep_alive_check))
                  {
                     if (keep_alive_check_interval > 40)
                     {
                        time_t tmp_time = time(NULL);

                        keep_alive_check_interval = -1;
                        if ((tmp_time - keep_alive_time) >= FTP_CTRL_KEEP_ALIVE_INTERVAL)
                        {
                           keep_alive_time = tmp_time;
                           if ((status = ftp_keepalive()) != SUCCESS)
                           {
                              trans_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to send STAT command (%d).",
                                        status);
                              if (timeout_flag == ON)
                              {
                                 timeout_flag = OFF;
                              }
                              do_keep_alive_check = 0;
                           }
                           else if (fsa[db.fsa_pos].debug == YES)
                                {
                                   trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                                "Send STAT command.");
                                }
                        }
                     }
                     keep_alive_check_interval++;
                  }
#endif /* FTP_CTRL_KEEP_ALIVE_INTERVAL */
               } /* if (bytes_buffered > 0) */
            } while (bytes_buffered == blocksize);

            /*
             * Since there are always some users sending files to the
             * AFD not in dot notation, lets check here if the file size
             * has changed.
             */
            if ((no_of_bytes + append_offset) != *p_file_size_buffer)
            {
               /*
                * Give a warning in the system log, so some action
                * can be taken against the originator.
                */
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "File <%s> for host %s was DEFINITELY NOT send in dot notation. Size changed from %d to %d.",
                          final_filename, fsa[db.fsa_pos].host_dsp_name,
                          *p_file_size_buffer, no_of_bytes + append_offset);
            }

            /* Close local file */
            if (close(fd) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to close() local file <%s> : %s",
                          final_filename, strerror(errno));
               /*
                * Since we usually do not send more then 100 files and
                * sf_ftp() will exit(), there is no point in stopping
                * the transmission.
                */
            }

            if (db.special_flag & FILE_NAME_IS_HEADER)
            {
               buffer[0] = '\015';
               buffer[1] = '\015';
               buffer[2] = '\012';
               buffer[3] = 3;  /* ETX */
               if ((status = ftp_write(buffer, ascii_buffer, 4)) != SUCCESS)
               {
                  /*
                   * It could be that we have received a SIGPIPE
                   * signal. If this is the case there might be data
                   * from the remote site on the control connection.
                   * Try to read this data into the global variable
                   * 'msg_str'.
                   */
                  msg_str[0] = '\0';
                  if (status == EPIPE)
                  {
                     (void)ftp_get_reply();
                  }
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to write <CR><CR><LF><ETX> to remote file <%s>",
                            initial_filename);
                  if (status == EPIPE)
                  {
                     /*
                      * When pipe is broken no nead to send a QUIT
                      * to the remote side since the connection has
                      * already been closed by the remote side.
                      */
                     msg_str[0] = '\0';
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Hmm. Pipe is broken. Will NOT send a QUIT.");
                  }
                  else
                  {
                     (void)ftp_quit();
                  }
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_REMOTE_ERROR);
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
                     fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += 4;
                     fsa[db.fsa_pos].job_status[(int)db.job_no].bytes_send += 4;
                  }
               }
            }

            /* Close remote file */
            if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
            {
               /*
                * Closing files that have zero length is not possible
                * on some systems. So if this is the case lets not count
                * this as an error. Just ignore it, but send a message in
                * the transfer log, so the user sees that he is trying
                * to send files with zero length.
                */
               if ((*p_file_size_buffer > 0) || (timeout_flag == ON))
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to close remote file <%s>",
                            initial_filename);
                  (void)ftp_quit();
                  exit((timeout_flag == ON) ? TIMEOUT_ERROR : CLOSE_REMOTE_ERROR);
               }
               else
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to close remote file <%s> (%d). Ignoring since file size is %d.",
                            initial_filename, status, *p_file_size_buffer);
               }
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Closed data connection for file <%s>.",
                               initial_filename);
               }
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               end_time = times(&tmsdummy);
            }
#endif
            if (db.chmod_str[0] != '\0')
            {
               if ((status = ftp_chmod(initial_filename,
                                       db.chmod_str)) != SUCCESS)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to chmod remote file <%s> to %s (%d).",
                            initial_filename, db.chmod_str, status);
                  if (timeout_flag == ON)
                  {
                     timeout_flag = OFF;
                  }
               }
               else if (fsa[db.fsa_pos].debug == YES)
                    {
                       trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                    "Changed mode of remote file <%s> to %s",
                                    initial_filename, db.chmod_str);
                    }
            }

            if (fsa[db.fsa_pos].debug == YES)
            {
               char line_buffer[MAX_RET_MSG_LENGTH];

               if ((status = ftp_list(db.mode_flag, LIST_CMD, initial_filename,
                                      line_buffer)) != SUCCESS)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to list remote file <%s> (%d).",
                            initial_filename, status);
                  if (timeout_flag == ON)
                  {
                     timeout_flag = OFF;
                  }
               }
               else
               {
                  msg_str[0] = '\0';
                  trans_db_log(INFO_SIGN, NULL, 0, "%s", line_buffer);
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Local file size of <%s> is %d",
                               final_filename, no_of_bytes + append_offset);
               }
            }
         } /* if (append_offset < p_file_size_buffer) */

         /* If we used dot notation, don't forget to rename */
         if ((db.lock == DOT) || (db.lock == POSTFIX) || (db.lock == DOT_VMS) ||
             (db.trans_rename_rule[0] != '\0'))
         {
            remote_filename[0] = '\0';
            if (db.lock == DOT_VMS)
            {
               (void)strcat(final_filename, DOT_NOTATION);
            }
            if (db.trans_rename_rule[0] != '\0')
            {
               register int k;

               for (k = 0; k < rule[trans_rule_pos].no_of_rules; k++)
               {
                  if (pmatch(rule[trans_rule_pos].filter[k],
                             final_filename) == 0)
                  {
                     change_name(final_filename,
                                 rule[trans_rule_pos].filter[k],
                                 rule[trans_rule_pos].rename_to[k],
                                 remote_filename);
                     break;
                  }
               }
            }
            if (remote_filename[0] == '\0')
            {
               (void)strcpy(remote_filename, final_filename);
            }
            if ((status = ftp_move(initial_filename,
                                   remote_filename)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to move remote file <%s> to <%s> (%d)",
                         initial_filename, remote_filename, status);
               (void)ftp_quit();
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : MOVE_REMOTE_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Renamed remote file <%s> to <%s>",
                               initial_filename, remote_filename);
               }
            }
            if (db.lock == DOT_VMS)
            {
               /* Remove dot at end of name */
               ptr = final_filename + strlen(final_filename) - 1;
               *ptr = '\0';
            }
         }

#ifdef _WITH_READY_FILES
         if ((db.lock == READY_A_FILE) || (db.lock == READY_B_FILE))
         {
            int  rdy_length;
            char file_type,
                 ready_file_name[MAX_FILENAME_LENGTH],
                 ready_file_buffer[MAX_PATH_LENGTH + 25];

            /* Generate the name for the ready file */
            (void)sprintf(ready_file_name, "%s_rdy", final_filename);

            /* Open ready file on remote site */
            if ((status = ftp_data(ready_file_name, append_offset,
                                   db.mode_flag, DATA_WRITE)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to open remote ready file <%s> (%d).",
                         ready_file_name, status);
               (void)ftp_quit();
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : OPEN_REMOTE_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Open remote ready file <%s>", ready_file_name);
               }
            }

            /* Create contents for ready file */
            if (db.lock == READY_A_FILE)
            {
               file_type = 'A';
            }
            else
            {
               file_type = 'B';
            }
            rdy_length = sprintf(ready_file_buffer,
                                 "%s %c U\n$$end_of_ready_file\n",
                                 initial_filename, file_type);

            /* Write remote ready file in one go. */
            if ((status = ftp_write(ready_file_buffer, NULL,
                                    rdy_length)) != SUCCESS)
            {
               /*
                * It could be that we have received a SIGPIPE
                * signal. If this is the case there might be data
                * from the remote site on the control connection.
                * Try to read this data into the global variable
                * 'msg_str'.
                */
               msg_str[0] = '\0';
               if (status == EPIPE)
               {
                  (void)ftp_get_reply();
               }
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to write to remote ready file <%s> (%d).",
                         ready_file_name, status);
               if (status == EPIPE)
               {
                  /*
                   * When pipe is broken no nead to send a QUIT
                   * to the remote side since the connection has
                   * already been closed by the remote side.
                   */
                  msg_str[0] = '\0';
                  trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Hmm. Pipe is broken. Will NOT send a QUIT.");
               }
               else
               {
                  (void)ftp_quit();
               }
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : WRITE_REMOTE_ERROR);
            }

            /* Close remote ready file */
            if ((status = ftp_close_data(DATA_WRITE)) != SUCCESS)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to close remote ready file <%s> (%d).",
                         ready_file_name, status);
               (void)ftp_quit();
               exit((timeout_flag == ON) ? TIMEOUT_ERROR : CLOSE_REMOTE_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Closed remote ready file <%s>", ready_file_name);
               }
            }
         }
#endif /* _WITH_READY_FILES */

         if (db.special_flag & EXEC_FTP)
         {
            char *p_name;

            if (db.trans_rename_rule[0] != '\0')
            {
               p_name = remote_filename;
            }
            else
            {
               p_name = final_filename;
            }
            if ((status = ftp_exec(db.special_ptr, p_name)) != SUCCESS)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to send SITE %s %s (%d).",
                         db.special_ptr, p_name, status);
               if (timeout_flag == ON)
               {
                  timeout_flag = OFF;
               }
            }
            else if (fsa[db.fsa_pos].debug == YES)
                 {
                    trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                 "Send SITE %s %s", db.special_ptr, p_name);
                 }
         }

         /* Update FSA, one file transmitted. */
         if (host_deleted == NO)
         {
            lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);

            /* Before we read from the FSA lets make */
            /* sure that it is NOT stale!            */
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
                             "Total file counter for host %s less then zero. Correcting to %d.",
                             fsa[db.fsa_pos].host_dsp_name,
                             files_to_send - (files_send + 1));
                  fsa[db.fsa_pos].total_file_counter = files_to_send - (files_send + 1);
               }
#endif

               /* Total file size */
#ifdef _VERIFY_FSA
               ui_variable = fsa[db.fsa_pos].total_file_size;
#endif
               fsa[db.fsa_pos].total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
               if (fsa[db.fsa_pos].total_file_size > ui_variable)
               {
                  int   k;
                  off_t *tmp_ptr = p_file_size_buffer;

                  tmp_ptr++;
                  fsa[db.fsa_pos].total_file_size = 0;
                  for (k = (files_send + 1); k < files_to_send; k++)
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
                                  "fc for host %s is zero but fs is not zero (%d). Correcting.",
                                  fsa[db.fsa_pos].host_dsp_name,
                                  fsa[db.fsa_pos].total_file_size);
                       fsa[db.fsa_pos].total_file_size = 0;
                    }
#endif

               /* File counter done */
               fsa[db.fsa_pos].file_counter_done += 1;

               /* Number of bytes send */
               fsa[db.fsa_pos].bytes_send += no_of_bytes;
               unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
               unlock_region(fsa_fd, lock_offset);
            }
         }

         if (append_file_number != -1)
         {
            /* This file was appended, so lets remove it */
            /* from the append list in the message file. */
            remove_append(db.job_id, db.restart_file[append_file_number]);
         }

#ifdef _WITH_TRANS_EXEC
         if (db.special_flag & TRANS_EXEC)
         {
            trans_exec(file_path, fullname, p_file_name_buffer);
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
             * when adding ARCHIVE_UNIT * db.archive_time to
             * msg_name.
             */
            if (archive_file(file_path, final_filename, p_db) < 0)
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  msg_str[0] = '\0';
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to archive file <%s>", final_filename);
               }

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if ((unlink(fullname) == -1) && (errno != ENOENT))
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file <%s> after sending it successfully : %s",
                             fullname, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  if (db.trans_rename_rule[0] == '\0')
                  {
                     (void)strcpy(ol_file_name, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  }
                  else
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                    "%s /%s",
                                                                    p_file_name_buffer,
                                                                    remote_filename);
                  }
                  *ol_file_size = no_of_bytes + append_offset;
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
#endif /* _OUTPUT_LOG */
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  msg_str[0] = '\0';
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Archived file <%s>", final_filename);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  if (db.trans_rename_rule[0] == '\0')
                  {
                     (void)strcpy(ol_file_name, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  }
                  else
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                    "%s /%s",
                                                                    p_file_name_buffer,
                                                                    remote_filename);
                  }
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1],
                               &db.archive_dir[db.archive_offset]);
                  *ol_file_size = no_of_bytes + append_offset;
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
#endif /* _OUTPUT_LOG */
            }
         }
         else
         {
            /* Delete the file we just have send */
            if (unlink(fullname) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file <%s> after sending it successfully : %s",
                          fullname, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               if (db.trans_rename_rule[0] == '\0')
               {
                  (void)strcpy(ol_file_name, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               }
               else
               {
                  *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                 "%s /%s",
                                                                 p_file_name_buffer,
                                                                 remote_filename);
               }
               *ol_file_size = no_of_bytes + append_offset;
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
#endif /* _OUTPUT_LOG */
         }

#ifdef _RADAR_CHECK
         if ((db.special_flag & RADAR_CHECK_FLAG) &&
             (p_file_name_buffer[0] == 'r') &&
             (p_file_name_buffer[1] == 'a') &&
             (p_file_name_buffer[2] == 'a') &&
             (p_file_name_buffer[3] == '0') &&
             (p_file_name_buffer[4] == '0') &&
             (p_file_name_buffer[5] == '-') &&
             (p_file_name_buffer[6] == 'p') &&
             (p_file_name_buffer[7] == 'l') &&
             (p_file_name_buffer[8] == '_') &&
             (isdigit(p_file_name_buffer[15])) &&
             (isdigit(p_file_name_buffer[16])) &&
             (isdigit(p_file_name_buffer[17])) &&
             (isdigit(p_file_name_buffer[18])) &&
             (isdigit(p_file_name_buffer[19])) &&
             (isdigit(p_file_name_buffer[20])) &&
             (isdigit(p_file_name_buffer[21])) &&
             (isdigit(p_file_name_buffer[22])) &&
             (isdigit(p_file_name_buffer[23])) &&
             (isdigit(p_file_name_buffer[24])))
         {
            char      str[3];
            time_t    diff_time,
                      time_val;
            struct tm *bd_time;

            time_val = time(NULL);
            bd_time = localtime(&time_val);
            bd_time->tm_sec  = 0;
            str[0] = p_file_name_buffer[23];
            str[1] = p_file_name_buffer[24];
            str[2] = '\0';
            bd_time->tm_min  = atoi(str);
            str[0] = p_file_name_buffer[21];
            str[1] = p_file_name_buffer[22];
            bd_time->tm_hour = atoi(str);
            str[0] = p_file_name_buffer[19];
            str[1] = p_file_name_buffer[20];
            bd_time->tm_mday = atoi(str);
            str[0] = p_file_name_buffer[17];
            str[1] = p_file_name_buffer[18];
            bd_time->tm_mon  = atoi(str) - 1;
            diff_time = time_val - (mktime(bd_time) - timezone);
            msg_str[0] = '\0';
            if (diff_time > 540) /* 9 minutes */
            {
               trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                         "=====> %s %ld seconds late.",
                         p_file_name_buffer, diff_time);
            }
            else if (diff_time > 420) /* 7 minutes */
                 {
                    trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                              "====> %s %ld seconds late.",
                              p_file_name_buffer, diff_time);
                 }
            else if (diff_time > 300) /* 5 minutes */
                 {
                    trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                              "===> %s %ld seconds late.",
                              p_file_name_buffer, diff_time);
                 }
            else if (diff_time > 180) /* 3 minutes */
                 {
                    trans_log(DEBUG_SIGN, __FILE__, __LINE__,
                              "==> %s %ld seconds late.",
                              p_file_name_buffer, diff_time);
                 }
         }
#endif /* _RADAR_CHECK */

         /*
          * After each successful transfer set error counter to zero,
          * so that other jobs can be started.
          */
         if (fsa[db.fsa_pos].error_counter > 0)
         {
            int  fd,
                 j;
            char fd_wake_up_fifo[MAX_PATH_LENGTH];

            lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);
            fsa[db.fsa_pos].error_counter = 0;

            /*
             * Wake up FD!
             */
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

#ifdef _WITH_INTERRUPT_JOB
         if ((fsa[db.fsa_pos].job_status[(int)db.job_no].special_flag & INTERRUPT_JOB) &&
             ((files_send + 1) < files_to_send))
         {
            interrupt = YES;
            break;
         }
#endif

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

#ifdef _BURST_MODE
      search_for_files = YES;
      lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].job_status[(int)db.job_no].job_id - (char *)fsa);
#ifdef _WITH_INTERRUPT_JOB
   } while ((fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter != burst_counter) &&
            (interrupt == NO));
#else
   } while (fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter != burst_counter);
#endif
   fsa[db.fsa_pos].job_status[(int)db.job_no].burst_counter = 0;
#endif /* _BURST_MODE */

   /* Do not forget to remove lock file if we have created one */
   if ((db.lock == LOCKFILE) && (fsa[db.fsa_pos].active_transfers == 1))
   {
      if ((status = ftp_dele(LOCK_FILENAME)) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to remove remote lock file <%s> (%d)",
                   LOCK_FILENAME, status);
         (void)ftp_quit();
         exit((timeout_flag == ON) ? TIMEOUT_ERROR : REMOVE_LOCKFILE_ERROR);
      }
      else
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Removed lock file <%s>.", LOCK_FILENAME);
         }
      }
   }

   /*
    * If a file that is to be appended, removed (eg. by disabling
    * the host), the name of the append file will be left in the
    * message (in the worst case forever). So lets do a check
    * here if all files are transmitted only then remove all append
    * files from the message.
    */
   if ((db.no_of_restart_files > 0) &&
       (append_count != db.no_of_restart_files) &&
       (fsa[db.fsa_pos].total_file_counter == 0))
   {
      remove_all_appends(db.job_id);
   }

#ifdef _WITH_INTERRUPT_JOB
   if (interrupt == NO)
   {
#endif
      /*
       * Remove file directory.
       */
      if (rmdir(file_path) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to remove directory <%s> : %s [PID = %d] [job_no = %d]",
                    file_path, strerror(errno), db.my_pid, (int)db.job_no);
         exit_status = STILL_FILES_TO_SEND;
      }
#ifdef _WITH_INTERRUPT_JOB
   }
#endif

#ifdef _WITH_BURST_2
      burst_2_counter++;
      total_append_count += append_count;
      append_count = 0;
#ifdef _BURST_MODE
      burst_2_counter += burst_counter;
#endif /* _BURST_MODE */
#ifdef _WITH_INTERRUPT_JOB
   } while (check_burst_2(file_path, &files_to_send, interrupt, &values_changed) == YES);
#else
   } while (check_burst_2(file_path, &files_to_send, &values_changed) == YES);
#endif
   burst_2_counter--;
#endif /* _WITH_BURST_2 */

   fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = CLOSING_CONNECTION;
   free(buffer);

#ifdef _CHECK_BEFORE_EXIT
#endif

   /* Logout again */
   if ((status = ftp_quit()) != SUCCESS)
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__,
                "Failed to disconnect from remote host (%d).", status);

      /*
       * Since all files have been transfered successful it is
       * not necessary to indicate an error in the status display.
       * It's enough when we say in the Transfer log that we failed
       * to log out.
       */
   }
   else
   {
      if (fsa[db.fsa_pos].debug == YES)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Logged out.");
      }
   }

   /* Don't need the ASCII buffer */
   if (ascii_buffer != NULL)
   {
      free(ascii_buffer);
   }

   exitflag = 0;
   exit(exit_status);
}


/*+++++++++++++++++++++++++++++ sf_ftp_exit() +++++++++++++++++++++++++++*/
static void
sf_ftp_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   if ((fsa != NULL) && (host_deleted == NO) && (db.fsa_pos >= 0))
   {
      if ((fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done > 0) ||
          (fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done > 0))
      {
         (void)sprintf(sf_fin_fifo, "%lu Bytes send in %d file(s).",
                       fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                       fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);

#ifdef _WITH_BURST_2
         if (total_append_count == 1)
         {
            (void)strcat(sf_fin_fifo, " [APPEND]");
         }
         else if (total_append_count > 1)
              {
                 char tmp_buffer[13 + MAX_INT_LENGTH];

                 (void)sprintf(tmp_buffer, " [APPEND * %d]", total_append_count);
                 (void)strcat(sf_fin_fifo, tmp_buffer);
              }
         if (burst_2_counter == 1)
         {
            (void)strcat(sf_fin_fifo, " [BURST]");
         }
         else if (burst_2_counter > 1)
              {
                 char tmp_buffer[12 + MAX_INT_LENGTH];

                 (void)sprintf(tmp_buffer, " [BURST * %u]", burst_2_counter);
                 (void)strcat(sf_fin_fifo, tmp_buffer);
              }
#else
         if (append_count == 1)
         {
            (void)strcat(sf_fin_fifo, " [APPEND]");
         }
         else if (append_count > 1)
              {
                 char tmp_buffer[13 + MAX_INT_LENGTH];

                 (void)sprintf(tmp_buffer, " [APPEND * %d]", append_count);
                 (void)strcat(sf_fin_fifo, tmp_buffer);
              }
#ifdef _BURST_MODE
         if (burst_counter == 1)
         {
            (void)strcat(sf_fin_fifo, " [BURST]");
         }
         else if (burst_counter > 1)
              {
                 char tmp_buffer[12 + MAX_INT_LENGTH];

                 (void)sprintf(tmp_buffer, " [BURST * %d]", burst_counter);
                 (void)strcat(sf_fin_fifo, tmp_buffer);
              }
#endif /* _BURST_MODE */
#endif
         msg_str[0] = '\0';
         trans_log(INFO_SIGN, NULL, 0, "%s", sf_fin_fifo);
      }

      if ((fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] != '\0') &&
          (fsa[db.fsa_pos].file_size_offset != -1) &&
          (append_offset == 0) &&
          (fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done > MAX_SEND_BEFORE_APPEND))
      {
         log_append(&db, initial_filename,
                    fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use);
      }
      reset_fsa((struct job *)&db, exitflag);
   }

   if (db.trans_rename_rule[0] != '\0')
   {
      (void)close(counter_fd);
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
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo <%s> : %s",
                 sf_fin_fifo, strerror(errno));
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
