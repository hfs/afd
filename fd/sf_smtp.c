/*
 *  sf_smtp.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_smtp - send data via SMTP
 **
 ** SYNOPSIS
 **   sf_smtp [options] -m <message-file>
 **
 **   options
 **       --version               - Version
 **       -w directory            - the working directory of the
 **                                 AFD
 **       -j <process number>     - the process number under which this
 **                                 job is to be displayed
 **       -f                      - error message
 **       -t                      - toggle host
 **
 ** DESCRIPTION
 **   sf_smtp sends the given files to the defined recipient via SMTP
 **   It does so by using it's own SMTP-client.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <sheme>://<user>:<password>@<host>:<port>/<url-path>
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
 **   14.12.1996 H.Kiehl Created
 **   27.01.1997 H.Kiehl Include support for output logging.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   09.08.1997 H.Kiehl Write some more detailed logging when
 **                      an error has occurred.
 **   29.08.1997 H.Kiehl Support for 'real' host names.
 **   03.06.1998 H.Kiehl Added subject option.
 **   28.07.1998 H.Kiehl Added 'file name is user' option.
 **   27.11.1998 H.Kiehl Added attaching file.
 **   29.03.1999 H.Kiehl Local user name is LOGNAME.
 **   24.08.1999 H.Kiehl Enhanced option "attach file" to support trans-
 **                      renaming.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* remove(), close()              */
#include <errno.h>
#include "fddefs.h"
#include "smtpdefs.h"
#include "version.h"

/* Global variables */
int                        counter_fd,
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           rule_pos,
                           fsa_id,
                           fsa_fd = -1,
                           line_length = 0, /* encode_base64()           */
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO,
                           timeout_flag;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer;
long                       smtp_timeout;
char                       host_deleted = NO,
                           err_msg_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           msg_str[MAX_RET_MSG_LENGTH],
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           *file_name_buffer;
struct filetransfer_status *fsa;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG                                   
struct delete_log          dl;
#endif

/* Local functions */
static void                sf_smtp_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           sig_kill(int),
                           sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _VERIFY_FSA
   unsigned int     ui_variable;
#endif
   int              j,
                    fd,
                    status,
                    no_of_bytes,
                    loops,
                    rest,
                    mail_header_size = 0,
                    files_to_send = 0,
                    files_send,
                    blocksize,
                    write_size;
   off_t            lock_offset,
                    *p_file_size_buffer;
   char             *smtp_buffer = NULL,
                    *p_file_name_buffer,
                    *ptr,
                    host_name[256],
                    local_user[MAX_FILENAME_LENGTH],
                    multipart_boundary[MAX_FILENAME_LENGTH],
                    remote_user[MAX_FILENAME_LENGTH],
                    *buffer,
                    *buffer_ptr,
                    *encode_buffer = NULL,
                    final_filename[MAX_FILENAME_LENGTH],
                    fullname[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    *extra_mail_header_buffer = NULL,
                    *mail_header_buffer = NULL,
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
   unsigned int     *ol_job_number = NULL;
   char             *ol_data = NULL,
                    *ol_file_name = NULL;
   unsigned short   *ol_file_name_length;
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
      (void)rec(sys_log_fd, FATAL_SIGN, "sigaction() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_smtp_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit function : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   init_sf(argc, argv, file_path, &blocksize, &files_to_send, SMTP);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))  /* Ignore SIGPIPE! */
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Set SMTP timeout value */
   smtp_timeout = fsa[(int)db.position].transfer_timeout;

   /*
    * The extra buffer is needed to convert LF's to CRLF.
    */
   if ((smtp_buffer = (char *)malloc((blocksize * 2))) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      output_log_ptrs(&ol_fd,
                      &ol_job_number,
                      &ol_data,
                      &ol_file_name,
                      &ol_file_name_length,
                      &ol_file_size,
                      &ol_size,
                      &ol_transfer_time,
                      db.host_alias,
                      SMTP);
   }
#endif

   timeout_flag = OFF;

   if (db.smtp_server[0] == '\0')
   {
      (void)strcpy(db.smtp_server, SMTP_HOST_NAME);
   }

   /* Connect to remote SMTP-server */
   if ((status = smtp_connect(db.smtp_server, db.port)) != SUCCESS)
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Could not connect to %s (%d). (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   SMTP_HOST_NAME, status, __FILE__, __LINE__);
      }
      reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);

      if (timeout_flag == OFF)
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to connect to %s (%d). #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   SMTP_HOST_NAME, status,
                   db.job_id, __FILE__, __LINE__);
         if (status != INCORRECT)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, msg_str);
         }
      }
      else
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to connect to %s due to timeout. #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   SMTP_HOST_NAME, db.job_id, __FILE__, __LINE__);
      }
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, INFO_SIGN,
                   "%-*s[%d]: Connected to %s. (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, db.smtp_server,
                   __FILE__, __LINE__);
         (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, msg_str);
      }
   }

   /* Now send HELO */
   if (gethostname(host_name, 255) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "gethostname() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((status = smtp_helo(host_name)) != SUCCESS)
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to send HELO to %s (%d). (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, host_name, status, __FILE__, __LINE__);
      }
      if (timeout_flag == OFF)
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to send HELO to %s (%d). #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, host_name, status,
                   db.job_id, __FILE__, __LINE__);
         if (status != INCORRECT)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, msg_str);
         }
      }
      else
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to send HELO to %s due to timeout. #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, host_name,
                   db.job_id, __FILE__, __LINE__);
      }
      reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, INFO_SIGN,
                   "%-*s[%d]: Have send HELO. (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, __FILE__, __LINE__);
         (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, msg_str);
      }
   }

   /* Inform FSA that we have finished connecting */
   if (host_deleted == NO)
   {
      lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
      rlock_region(fsa_fd, lock_offset);

      if (check_fsa() == YES)
      {
         if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
         else
         {
            lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);
         }
      }
      if (host_deleted == NO)
      {
         fsa[(int)db.position].job_status[(int)db.job_no].connect_status = EMAIL_ACTIVE;
         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files = files_to_send;

         /* Number of connections */
         lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].connections - (char *)fsa);
         fsa[(int)db.position].connections += 1;
         unlock_region(fsa_fd, (char *)&fsa[(int)db.position].connections - (char *)fsa);
         unlock_region(fsa_fd, lock_offset);
      }
   }

   /* Prepare local and remote user name */
   if ((ptr = getenv("LOGNAME")) != NULL)
   {
      (void)sprintf(local_user, "%s@%s", ptr, host_name);
   }
   else
   {
      (void)sprintf(local_user, "%s@%s", AFD_USER_NAME, host_name);
   }
   if (check_fsa() == YES)
   {
      if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Terminating. Host %s is not in FSA. (%s %d)\n",
                   db.host_alias, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if (db.toggle_host == YES)
   {
      if (fsa[(int)db.position].host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname,
                      fsa[(int)db.position].real_hostname[HOST_TWO - 1]);
      }
      else
      {
         (void)strcpy(db.hostname,
                      fsa[(int)db.position].real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa[(int)db.position].real_hostname[(int)(fsa[(int)db.position].host_toggle - 1)]);
   }
   if ((db.special_flag & FILE_NAME_IS_USER) == 0)
   {
      (void)sprintf(remote_user, "%s@%s", db.user, db.hostname);
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 1)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }
   if (db.special_flag & ATTACH_FILE)
   {
      if ((encode_buffer = malloc(2 * (blocksize + 1))) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(ALLOC_ERROR);
      }

      /*
       * When encoding in base64 is done the blocksize must be
       * divideable by three!!!!
       */
      blocksize = blocksize - (blocksize % 3);
   }

   /* Read mail header file. */
   multipart_boundary[0] = '\0';
   if (db.special_flag & ADD_MAIL_HEADER)
   {
      int  mail_fd;
      char mail_header_file[MAX_PATH_LENGTH];

      if (db.special_ptr == NULL)
      {
         /*
          * Try read default mail header file for this host.
          */
         (void)sprintf(mail_header_file, "%s%s/%s%s",
                       p_work_dir, ETC_DIR, MAIL_HEADER_IDENTIFIER,
                       fsa[(int)db.position].host_alias);
      }
      else
      {
         /*
          * Try read user specified mail header file for this host.
          */
         (void)strcpy(mail_header_file, db.special_ptr);
      }

      if ((mail_fd = open(mail_header_file, O_RDONLY)) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to open() mail header file %s : %s (%s %d)\n",
                   mail_header_file, strerror(errno),
                   __FILE__, __LINE__);
      }
      else
      {
         struct stat stat_buf;

         if (fstat(mail_fd, &stat_buf) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to fstat() mail header file %s : %s (%s %d)\n",
                      mail_header_file, strerror(errno),
                      __FILE__, __LINE__);
         }
         else
         {
            if (stat_buf.st_size <= 204800)
            {
               if ((mail_header_buffer = malloc(stat_buf.st_size)) == NULL)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to malloc() buffer for mail header file : %s (%s %d)\n",
                            strerror(errno),
                            __FILE__, __LINE__);
               }
               else
               {
                  if ((extra_mail_header_buffer = malloc((2 * stat_buf.st_size))) == NULL)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to malloc() buffer for mail header file : %s (%s %d)\n",
                               strerror(errno),
                               __FILE__, __LINE__);
                     free(mail_header_buffer);
                     mail_header_buffer = NULL;
                  }
                  else
                  {
                     mail_header_size = stat_buf.st_size;
                     if (read(mail_fd, mail_header_buffer, mail_header_size) != stat_buf.st_size)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to read() mail header file %s : %s (%s %d)\n",
                                  mail_header_file, strerror(errno),
                                  __FILE__, __LINE__);
                        free(mail_header_buffer);
                        mail_header_buffer = NULL;
                     }
                     else
                     {
                        mail_header_buffer[mail_header_size] = '\0';

                        /* If we are attaching a file we have to */
                        /* do a multipart mail.                  */
                        if (db.special_flag & ATTACH_FILE)
                        {
                           (void)sprintf(multipart_boundary, "----%s", db.msg_name);
                        }
                     }
                  }
               }
            }
            else
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Mail header file %s to large (%d Bytes). Allowed are %d Bytes. (%s %d)\n",
                         mail_header_file, stat_buf.st_size,
                         __FILE__, __LINE__);
            }
         }
         if (close(mail_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }
   } /* if (db.special_flag & ADD_MAIL_HEADER) */

   /* Send all files */
   p_file_name_buffer = file_name_buffer;
   p_file_size_buffer = file_size_buffer;
   for (files_send = 0; files_send < files_to_send; files_send++)
   {
      /* Send local user name */
      if ((status = smtp_user(local_user)) != SUCCESS)
      {
         if (fsa[(int)db.position].debug == YES)
         {
            if (timeout_flag == OFF)
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to send local user %s (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         local_user, status, __FILE__, __LINE__);
               (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%c]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to send local user %s due to timeout. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         local_user, __FILE__, __LINE__);
            }
         }
         (void)smtp_quit();
         reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);
         if (timeout_flag == OFF)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to send local user %s (%d). #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      local_user, status,
                      db.job_id, __FILE__, __LINE__);
            (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
         else
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to send local user %s due to timeout. #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      local_user, db.job_id, __FILE__, __LINE__);
         }
         exit(USER_ERROR);
      }
      else
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Entered local user name %s. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      local_user, __FILE__, __LINE__);
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
      }

      if (db.special_flag & FILE_NAME_IS_USER)
      {
         if (db.user_rename_rule[0] != '\0')
         {
            register int k;

            for (k = 0; k < rule[rule_pos].no_of_rules; k++)
            {
               if (filter(rule[rule_pos].filter[k], p_file_name_buffer) == 0)
               {
                  change_name(p_file_name_buffer,
                              rule[rule_pos].filter[k],
                              rule[rule_pos].rename_to[k],
                              db.user);
                  break;
               }
            }
         }
         else
         {
            (void)strcpy(db.user, p_file_name_buffer);
         }
         (void)sprintf(remote_user, "%s@%s", db.user, db.hostname);
      } /* if (db.special_flag & FILE_NAME_IS_USER) */
   
      /* Send remote user name */
      if ((status = smtp_rcpt(remote_user)) != SUCCESS)
      {
         if (fsa[(int)db.position].debug == YES)
         {
            if (timeout_flag == OFF)
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to send remote user %s (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         remote_user, status, __FILE__, __LINE__);
               (void)rec(trans_db_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to send remote user %s due to timeout. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         remote_user, __FILE__, __LINE__);
            }
         }
         (void)smtp_quit();
         reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);
         if (timeout_flag == OFF)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to send remote user %s (%d). #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      remote_user, status, db.job_id,
                      __FILE__, __LINE__);
            (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
         else
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to send remote user %s due to timeout. #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      remote_user, db.job_id, __FILE__, __LINE__);
         }
         exit(REMOTE_USER_ERROR);
      }
      else
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Remote address %s accepted by SMTP-server. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      remote_user, __FILE__, __LINE__);
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
      }

      /* Enter data mode */
      if ((status = smtp_open()) != SUCCESS)
      {
         if (fsa[(int)db.position].debug == YES)
         {
            if (timeout_flag == OFF)
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to set DATA mode (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         status, __FILE__, __LINE__);
               (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to set DATA mode due to timeout. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         __FILE__, __LINE__);
            }
         }
         (void)smtp_quit();
         reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);
         if (timeout_flag == OFF)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to set DATA mode (%d). #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      status, db.job_id, __FILE__, __LINE__);
            (void)rec(transfer_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
         else
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to set DATA mode due to timeout. #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      db.job_id, __FILE__, __LINE__);
         }
         exit(DATA_ERROR);
      }
      else
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Set DATA mode. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      __FILE__, __LINE__);
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      msg_str);
         }
      }

      /* Get the the name of the file we want to send next */
      (void)strcpy(final_filename, p_file_name_buffer);
      (void)sprintf(fullname, "%s/%s", file_path, final_filename);

      /* Open local file */
      if ((fd = open(fullname, O_RDONLY)) < 0)
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Failed to open local file %s (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      fullname, __FILE__, __LINE__);
         }
         (void)smtp_quit();
         (void)rec(transfer_log_fd, INFO_SIGN,
                   "%-*s[%d]: %d Bytes send in %d file(s).\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                   fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
         reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                              NO_OF_FILES_VAR |
                              NO_OF_FILES_DONE_VAR |
                              FILE_SIZE_DONE_VAR |
                              FILE_SIZE_IN_USE_VAR |
                              FILE_SIZE_IN_USE_DONE_VAR));
         exit(OPEN_LOCAL_ERROR);
      }
      else
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Open local file %s (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      fullname, __FILE__, __LINE__);
         }
      }

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         start_time = times(&tmsdummy);
      }
#endif

      /* Write status to FSA? */
      if (host_deleted == NO)
      {
         if (check_fsa() == YES)
         {
            if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
            }
         }
         if (host_deleted == NO)
         {
            fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
            (void)strcpy(fsa[(int)db.position].job_status[(int)db.job_no].file_name_in_use,
                         final_filename);
         }
      }

      /* Read (local) and write (remote) file */
      no_of_bytes = 0;
      loops = *p_file_size_buffer / blocksize;
      rest = *p_file_size_buffer % blocksize;

      /* Send file name as subject if wanted. */
      if (db.special_flag & MAIL_SUBJECT)
      {
         size_t length;

         if (db.special_flag & ATTACH_FILE)
         {
            length = sprintf(buffer, "Subject : %s", db.subject);
         }
         else
         {
            length = sprintf(buffer, "Subject : %s\n", db.subject);
         }

         if (smtp_write(buffer, smtp_buffer, length) < 0)
         {
            if (fsa[(int)db.position].debug == YES)
            {
               if (timeout_flag == OFF)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
                  (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, msg_str);
               }
               else
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
               }
            }
            (void)smtp_quit();
            (void)rec(transfer_log_fd, INFO_SIGN,
                      "%-*s[%d]: %d Bytes send in %d file(s).\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                      fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
            reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                 NO_OF_FILES_VAR |
                                 NO_OF_FILES_DONE_VAR |
                                 FILE_SIZE_DONE_VAR |
                                 FILE_SIZE_IN_USE_VAR |
                                 FILE_SIZE_IN_USE_DONE_VAR |
                                 FILE_NAME_IN_USE_VAR));
            if (timeout_flag == OFF)
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
               (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
            }
            exit(WRITE_REMOTE_ERROR);
         }
         no_of_bytes = length;
      }
      else if (db.special_flag & FILE_NAME_IS_SUBJECT)
           {
              size_t length;

              if (db.special_flag & ATTACH_FILE)
              {
                 length = sprintf(buffer, "Subject : %s", final_filename);
              }
              else
              {
                 length = sprintf(buffer, "Subject : %s\n", final_filename);
              }

              if (smtp_write(buffer, smtp_buffer, length) < 0)
              {
                 if (fsa[(int)db.position].debug == YES)
                 {
                    if (timeout_flag == OFF)
                    {
                       (void)rec(trans_db_log_fd, ERROR_SIGN,
                                 "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                                 MAX_HOSTNAME_LENGTH, tr_hostname,
                                 (int)db.job_no, __FILE__, __LINE__);
                       (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                                 MAX_HOSTNAME_LENGTH, tr_hostname,
                                 (int)db.job_no, msg_str);
                    }
                    else
                    {
                       (void)rec(trans_db_log_fd, ERROR_SIGN,
                                 "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                                 MAX_HOSTNAME_LENGTH, tr_hostname,
                                 (int)db.job_no, __FILE__, __LINE__);
                    }
                 }
                 (void)smtp_quit();
                 (void)rec(transfer_log_fd, INFO_SIGN,
                           "%-*s[%d]: %d Bytes send in %d file(s).\n",
                           MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                           fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                           fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                 reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                      NO_OF_FILES_VAR |
                                      NO_OF_FILES_DONE_VAR |
                                      FILE_SIZE_DONE_VAR |
                                      FILE_SIZE_IN_USE_VAR |
                                      FILE_SIZE_IN_USE_DONE_VAR |
                                      FILE_NAME_IN_USE_VAR));
                 if (timeout_flag == OFF)
                 {
                    (void)rec(transfer_log_fd, ERROR_SIGN,
                              "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                              MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                              db.job_id, __FILE__, __LINE__);
                    (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                              MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                              msg_str);
                 }
                 else
                 {
                    (void)rec(transfer_log_fd, ERROR_SIGN,
                              "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                              MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                              db.job_id, __FILE__, __LINE__);
                 }
                 exit(WRITE_REMOTE_ERROR);
              }

              no_of_bytes = length;
           } /* if (db.special_flag & FILE_NAME_IS_SUBJECT) */

      /* Send MIME information. */
      if (db.special_flag & ATTACH_FILE)
      {
         size_t length;

         if (multipart_boundary[0] != '\0')
         {
#ifdef PRE_RELEASE
            length = sprintf(buffer,
                             "MIME-Version: 1.0 (produced by AFD %d.%d.%d-%d)\nContent-Type: MULTIPART/MIXED; BOUNDARY=\"%s\"\n",
                             MAJOR, MINOR, BUG_FIX, PRE_RELEASE,
                             multipart_boundary);
#else
            length = sprintf(buffer,
                             "MIME-Version: 1.0 (produced by AFD %d.%d.%d)\nContent-Type: MULTIPART/MIXED; BOUNDARY=\"%s\"\n",
                             MAJOR, MINOR, BUG_FIX, multipart_boundary);
#endif
            buffer_ptr = buffer;
         }
         else
         {
            length = sprintf(encode_buffer,
                             "MIME-Version: 1.0 (produced by AFD %d.%d.%d)\nContent-Type: APPLICATION/octet-stream; name=\"%s\"\nContent-Transfer-Encoding: BASE64\n\n",
                             MAJOR, MINOR, BUG_FIX, final_filename);
            buffer_ptr = encode_buffer;
         }

         if (smtp_write(buffer_ptr, smtp_buffer, length) < 0)
         {
            if (fsa[(int)db.position].debug == YES)
            {
               if (timeout_flag == OFF)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
                  (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, msg_str);
               }
               else
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
               }
            }
            (void)smtp_quit();
            (void)rec(transfer_log_fd, INFO_SIGN,
                      "%-*s[%d]: %d Bytes send in %d file(s).\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                      fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
            reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                 NO_OF_FILES_VAR |
                                 NO_OF_FILES_DONE_VAR |
                                 FILE_SIZE_DONE_VAR |
                                 FILE_SIZE_IN_USE_VAR |
                                 FILE_SIZE_IN_USE_DONE_VAR |
                                 FILE_NAME_IN_USE_VAR));
            if (timeout_flag == OFF)
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
               (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
            }
            exit(WRITE_REMOTE_ERROR);
         }

         no_of_bytes += length;
      } /* if (db.special_flag & ATTACH_FILE) */

      /* Write the mail header. */
      if (mail_header_buffer != NULL)
      {
         int length = 0;

         if (db.special_flag & ATTACH_FILE)
         {
            /* Write boundary */
            length = sprintf(encode_buffer,
                             "\n--%s\nContent-Type: TEXT/PLAIN; charset=US-ASCII\n\n",
                             multipart_boundary);

            if (smtp_write(encode_buffer, smtp_buffer, length) < 0)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  if (timeout_flag == OFF)
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                     (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                  }
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               if (timeout_flag == OFF)
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
                  (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            msg_str);
               }
               else
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
               }
               exit(WRITE_REMOTE_ERROR);
            }

            no_of_bytes += length;
         } /* if (db.special_flag & ATTACH_FILE) */

         /* Now lets write the message. */
         if (db.special_flag & ENCODE_ANSI)
         {
            if (smtp_write_iso8859(mail_header_buffer, extra_mail_header_buffer, mail_header_size) < 0)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  if (timeout_flag == OFF)
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                     (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                  }
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               if (timeout_flag == OFF)
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
                  (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            msg_str);
               }
               else
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
               }
               exit(WRITE_REMOTE_ERROR);
            }
         }
         else
         {
            if (smtp_write(mail_header_buffer, extra_mail_header_buffer, mail_header_size) < 0)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  if (timeout_flag == OFF)
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                     (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                  }
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               if (timeout_flag == OFF)
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
                  (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            msg_str);
               }
               else
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
               }
               exit(WRITE_REMOTE_ERROR);
            }
         }
         no_of_bytes += length;

         if (db.special_flag & ATTACH_FILE)
         {
            /* Write boundary */
            if (db.trans_rename_rule[0] != '\0')
            {
               int  k;
               char new_filename[MAX_FILENAME_LENGTH];

               new_filename[0] = '\0';
               for (k = 0; k < rule[rule_pos].no_of_rules; k++)
               {
                  if (filter(rule[rule_pos].filter[k], final_filename) == 0)
                  {
                     change_name(final_filename,
                                 rule[rule_pos].filter[k],
                                 rule[rule_pos].rename_to[k],
                                 new_filename);
                     break;
                  }
               }
               if (new_filename[0] == '\0')
               {
                  (void)strcpy(new_filename, final_filename);
               }
               length = sprintf(encode_buffer,
                                "\n--%s\nContent-Type: APPLICATION/octet-stream; name=\"%s\"\nContent-Transfer-Encoding: BASE64\n\n",
                                multipart_boundary, new_filename);
            }
            else
            {
               length = sprintf(encode_buffer,
                                "\n--%s\nContent-Type: APPLICATION/octet-stream; name=\"%s\"\nContent-Transfer-Encoding: BASE64\n\n",
                                multipart_boundary, final_filename);
            }

            if (smtp_write(encode_buffer, smtp_buffer, length) < 0)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  if (timeout_flag == OFF)
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                     (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(trans_db_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, __FILE__, __LINE__);
                  }
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               if (timeout_flag == OFF)
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
                  (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            msg_str);
               }
               else
               {
                  (void)rec(transfer_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            db.job_id, __FILE__, __LINE__);
               }
               exit(WRITE_REMOTE_ERROR);
            }

            no_of_bytes += length;
         }
      } /* if (mail_header_buffer != NULL) */

      for (;;)
      {
         for (j = 0; j < loops; j++)
         {
            if (read(fd, buffer, blocksize) != blocksize)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Could not read local file %s : %s (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fullname, strerror(errno), __FILE__, __LINE__);
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               exit(READ_LOCAL_ERROR);
            }
            if (db.special_flag & ATTACH_FILE)
            {
               write_size = encode_base64((unsigned char *)buffer, blocksize,
                                          (unsigned char *)encode_buffer);
               buffer_ptr = encode_buffer;
            }
            else
            {
               buffer_ptr = buffer;
               write_size = blocksize;
            }
            if (db.special_flag & ENCODE_ANSI)
            {
               if (smtp_write_iso8859(buffer_ptr, smtp_buffer, write_size) < 0)
               {
                  if (fsa[(int)db.position].debug == YES)
                  {
                     if (timeout_flag == OFF)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, msg_str);
                     }
                     else
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                     }
                  }
                  (void)smtp_quit();
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s[%d]: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                  reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                       NO_OF_FILES_VAR |
                                       NO_OF_FILES_DONE_VAR |
                                       FILE_SIZE_DONE_VAR |
                                       FILE_SIZE_IN_USE_VAR |
                                       FILE_SIZE_IN_USE_DONE_VAR |
                                       FILE_NAME_IN_USE_VAR));
                  if (timeout_flag == OFF)
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               msg_str);
                  }
                  else
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                  }
                  exit(WRITE_REMOTE_ERROR);
               }
            }
            else
            {
               if (smtp_write(buffer_ptr, smtp_buffer, write_size) < 0)
               {
                  if (fsa[(int)db.position].debug == YES)
                  {
                     if (timeout_flag == OFF)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, msg_str);
                     }
                     else
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                     }
                  }
                  (void)smtp_quit();
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s[%d]: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                  reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                       NO_OF_FILES_VAR |
                                       NO_OF_FILES_DONE_VAR |
                                       FILE_SIZE_DONE_VAR |
                                       FILE_SIZE_IN_USE_VAR |
                                       FILE_SIZE_IN_USE_DONE_VAR |
                                       FILE_NAME_IN_USE_VAR));
                  if (timeout_flag == OFF)
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               msg_str);
                  }
                  else
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                  }
                  exit(WRITE_REMOTE_ERROR);
               }
            }

            no_of_bytes += write_size;

            if (host_deleted == NO)
            {
               if (check_fsa() == YES)
               {
                  if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
               }
               if (host_deleted == NO)
               {
                  fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa[(int)db.position].job_status[(int)db.job_no].file_size_done += write_size;
                  fsa[(int)db.position].job_status[(int)db.job_no].bytes_send += write_size;
               }
            }
         }
         if (rest > 0)
         {
            if (read(fd, buffer, rest) != rest)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Could not read local file %s : %s (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fullname, strerror(errno), __FILE__, __LINE__);
               }
               (void)smtp_quit();
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               exit(READ_LOCAL_ERROR);
            }
            if (db.special_flag & ATTACH_FILE)
            {
               write_size = encode_base64((unsigned char *)buffer, rest,
                                          (unsigned char *)encode_buffer);
               buffer_ptr = encode_buffer;
            }
            else
            {
               write_size = rest;
               buffer_ptr = buffer;
            }
            if (db.special_flag & ENCODE_ANSI)
            {
               if (smtp_write_iso8859(buffer_ptr, smtp_buffer, write_size) < 0)
               {
                  if (fsa[(int)db.position].debug == YES)
                  {
                     if (timeout_flag == OFF)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, msg_str);
                     }
                     else
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                     }
                  }
                  (void)smtp_quit();
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s[%d]: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                  reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                       NO_OF_FILES_VAR |
                                       NO_OF_FILES_DONE_VAR |
                                       FILE_SIZE_DONE_VAR |
                                       FILE_SIZE_IN_USE_VAR |
                                       FILE_SIZE_IN_USE_DONE_VAR |
                                       FILE_NAME_IN_USE_VAR));
                  if (timeout_flag == OFF)
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, db.job_id, __FILE__, __LINE__);
                  }
                  exit(WRITE_REMOTE_ERROR);
               }
            }
            else
            {
               if (smtp_write(buffer_ptr, smtp_buffer, write_size) < 0)
               {
                  if (fsa[(int)db.position].debug == YES)
                  {
                     if (timeout_flag == OFF)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, msg_str);
                     }
                     else
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, __FILE__, __LINE__);
                     }
                  }
                  (void)smtp_quit();
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s[%d]: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                  reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                       NO_OF_FILES_VAR |
                                       NO_OF_FILES_DONE_VAR |
                                       FILE_SIZE_DONE_VAR |
                                       FILE_SIZE_IN_USE_VAR |
                                       FILE_SIZE_IN_USE_DONE_VAR |
                                       FILE_NAME_IN_USE_VAR));
                  if (timeout_flag == OFF)
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               db.job_id, __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, msg_str);
                  }
                  else
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, db.job_id, __FILE__, __LINE__);
                  }
                  exit(WRITE_REMOTE_ERROR);
               }
            }

            no_of_bytes += write_size;

            if (host_deleted == NO)
            {
               if (check_fsa() == YES)
               {
                  if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
               }
               if (host_deleted == NO)
               {
                  fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                  fsa[(int)db.position].job_status[(int)db.job_no].file_size_done += write_size;
                  fsa[(int)db.position].job_status[(int)db.job_no].bytes_send += write_size;
               }
            }
         }

         /*
          * Since there are always some users sending files to the
          * AFD not in dot notation, lets check here if this is really
          * the EOF.
          * If not lets continue so long until we hopefully have reached
          * the EOF.
          * NOTE: This is NOT a fool proof way. There must be a better
          *       way!
          */
         if (fstat(fd, &stat_buf) == -1)
         {
            (void)rec(transfer_log_fd, DEBUG_SIGN,
                      "Hmmm. Failed to stat() %s : %s (%s %d)\n",
                      fullname, strerror(errno),
                      __FILE__, __LINE__);
            break;
         }
         else
         {
            if (stat_buf.st_size > *p_file_size_buffer)
            {
               loops = (stat_buf.st_size - *p_file_size_buffer) / blocksize;
               rest = (stat_buf.st_size - *p_file_size_buffer) % blocksize;
               *p_file_size_buffer = stat_buf.st_size;

               /*
                * Give a warning in the system log, so some action
                * can be taken against the originator.
                */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "File %s for host %s was DEFINITELY NOT send in dot notation. (%s %d)\n",
                         final_filename, fsa[(int)db.position].host_dsp_name,
                         __FILE__, __LINE__);
            }
            else
            {
               break;
            }
         }
      } /* for (;;) */

      /* Write boundary end if neccessary. */
      if ((db.special_flag & ATTACH_FILE) && (mail_header_buffer != NULL))
      {
         int length;

         /* Write boundary */
         length = sprintf(buffer, "\n--%s--\n", multipart_boundary);

         if (smtp_write(buffer, smtp_buffer, length) < 0)
         {
            if (fsa[(int)db.position].debug == YES)
            {
               if (timeout_flag == OFF)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write data to SMTP-server (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
                  (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, msg_str);
               }
               else
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to write to SMTP-server due to timeout. (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, __FILE__, __LINE__);
               }
            }
            (void)smtp_quit();
            (void)rec(transfer_log_fd, INFO_SIGN,
                      "%-*s[%d]: %d Bytes send in %d file(s).\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                      fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
            reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                 NO_OF_FILES_VAR |
                                 NO_OF_FILES_DONE_VAR |
                                 FILE_SIZE_DONE_VAR |
                                 FILE_SIZE_IN_USE_VAR |
                                 FILE_SIZE_IN_USE_DONE_VAR |
                                 FILE_NAME_IN_USE_VAR));
            if (timeout_flag == OFF)
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write data to SMTP-server. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
               (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         msg_str);
            }
            else
            {
               (void)rec(transfer_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to write to SMTP-server due to timeout. #%d (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         db.job_id, __FILE__, __LINE__);
            }
            exit(WRITE_REMOTE_ERROR);
         }

         no_of_bytes += length;
      } /* if ((db.special_flag & ATTACH_FILE) && (mail_header_buffer != NULL)) */

#ifdef _OUTPUT_LOG
      if (db.output_log == YES)
      {
         end_time = times(&tmsdummy);
      }
#endif

      /* Close local file */
      if (close(fd) == -1)
      {
         (void)rec(transfer_log_fd, WARN_SIGN,
                   "%-*s[%d]: Failed to close() local file %s : %s (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   final_filename, strerror(errno), __FILE__, __LINE__);
         /*
          * Since we usually do not send more then 100 files and
          * sf_smtp() will exit(), there is no point in stopping
          * the transmission.
          */
      }

      /* Close remote file */
      if ((status = smtp_close()) != SUCCESS)
      {
         if (fsa[(int)db.position].debug == YES)
         {
            if (timeout_flag == OFF)
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to close data mode (%d). (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         status, __FILE__, __LINE__);
               (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, msg_str);
            }
            else
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to close data mode due to timeout. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, __FILE__, __LINE__);
            }
         }
         (void)smtp_quit();
         (void)rec(transfer_log_fd, INFO_SIGN,
                   "%-*s[%d]: %d Bytes send in %d file(s).\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                   fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                   fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
         reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                              NO_OF_FILES_VAR |
                              NO_OF_FILES_DONE_VAR |
                              FILE_SIZE_DONE_VAR |
                              FILE_SIZE_IN_USE_VAR |
                              FILE_SIZE_IN_USE_DONE_VAR |
                              FILE_NAME_IN_USE_VAR));
         if (timeout_flag == OFF)
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to close data mode (%d). #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      status, db.job_id, __FILE__, __LINE__);
            (void)rec(transfer_log_fd, ERROR_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, msg_str);
         }
         else
         {
            (void)rec(transfer_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to close data mode due to timeout. #%d (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, db.job_id, __FILE__, __LINE__);
         }
         exit(CLOSE_REMOTE_ERROR);
      }
      else
      {
         if (fsa[(int)db.position].debug == YES)
         {
            (void)rec(trans_db_log_fd, INFO_SIGN,
                      "%-*s[%d]: Closing data mode. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      __FILE__, __LINE__);
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, msg_str);
         }
      }

      /* Tell user via FSA a file has been mailed. */
      if (host_deleted == NO)
      {
         lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);

         /* Before we read from the FSA lets make */
         /* sure that it is NOT stale!            */
         if (check_fsa() == YES)
         {
            if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
            }
            else
            {
               lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
            }
         }
         if (host_deleted == NO)
         {
            fsa[(int)db.position].job_status[(int)db.job_no].file_name_in_use[0] = '\0';
            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done = files_send + 1;
            fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use = 0;
            fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = 0;

            /* Total file counter */
            lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].total_file_counter - (char *)fsa);
            fsa[(int)db.position].total_file_counter -= 1;
#ifdef _VERIFY_FSA
            if (fsa[(int)db.position].total_file_counter < 0)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Total file counter for host %s less then zero. Correcting to %d. (%s %d)\n",
                         fsa[(int)db.position].host_dsp_name,
                         files_to_send - (files_send + 1),
                         __FILE__, __LINE__);
               fsa[(int)db.position].total_file_counter = files_to_send - (files_send + 1);
            }
#endif

            /* Total file size */
#ifdef _VERIFY_FSA
            ui_variable = fsa[(int)db.position].total_file_size;
#endif
            fsa[(int)db.position].total_file_size -= stat_buf.st_size;
#ifdef _VERIFY_FSA
            if (fsa[(int)db.position].total_file_size > ui_variable)
            {
               int   k;
               off_t *tmp_ptr = p_file_size_buffer;

               tmp_ptr++;
               fsa[(int)db.position].total_file_size = 0;
               for (k = (files_send + 1); k < files_to_send; k++)
               {
                  fsa[(int)db.position].total_file_size += *tmp_ptr;
               }

               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Total file size for host %s overflowed. Correcting to %lu. (%s %d)\n",
                         fsa[(int)db.position].host_dsp_name,
                         fsa[(int)db.position].total_file_size,
                         __FILE__, __LINE__);
            }
            else if ((fsa[(int)db.position].total_file_counter == 0) &&
                     (fsa[(int)db.position].total_file_size > 0))
                 {
                    (void)rec(sys_log_fd, DEBUG_SIGN,
                              "fc for host %s is zero but fs is not zero. Correcting. (%s %d)\n",
                              fsa[(int)db.position].host_dsp_name,
                              __FILE__, __LINE__);
                    fsa[(int)db.position].total_file_size = 0;
                 }
#endif
            unlock_region(fsa_fd, (char *)&fsa[(int)db.position].total_file_counter - (char *)fsa);

            /* File counter done */
            lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].file_counter_done - (char *)fsa);
            fsa[(int)db.position].file_counter_done += 1;
            unlock_region(fsa_fd, (char *)&fsa[(int)db.position].file_counter_done - (char *)fsa);

            /* Number of bytes send */
            lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].bytes_send - (char *)fsa);
            fsa[(int)db.position].bytes_send += stat_buf.st_size;
            unlock_region(fsa_fd, (char *)&fsa[(int)db.position].bytes_send - (char *)fsa);
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
         if (archive_file(file_path, final_filename, p_db) < 0)
         {
            if (fsa[(int)db.position].debug == YES)
            {
               (void)rec(trans_db_log_fd, ERROR_SIGN,
                         "%-*s[%d]: Failed to archive file %s (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         final_filename, __FILE__, __LINE__);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)strcpy(ol_file_name, p_file_name_buffer);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
               *ol_transfer_time = end_time - start_time;
               ol_real_size = strlen(p_file_name_buffer) + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "write() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
#endif
         }
         else
         {
            if (fsa[(int)db.position].debug == YES)
            {
               (void)rec(trans_db_log_fd, INFO_SIGN,
                         "%-*s[%d]: Archived file %s (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         final_filename, __FILE__, __LINE__);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)strcpy(ol_file_name, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
               *ol_transfer_time = end_time - start_time;
               ol_real_size = *ol_file_name_length +
                              strlen(&ol_file_name[*ol_file_name_length + 1]) +
                              ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "write() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
#endif
         }
      }
      else
      {
         /* Delete the file we just have send */
         if (remove(fullname) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not remove local file %s after sending it successfully : %s (%s %d)\n",
                      strerror(errno), fullname, __FILE__, __LINE__);
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            (void)strcpy(ol_file_name, p_file_name_buffer);
            *ol_file_size = *p_file_size_buffer;
            *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
            *ol_transfer_time = end_time - start_time;
            ol_real_size = strlen(p_file_name_buffer) + ol_size;
            if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "write() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
#endif
      }

      /*
       * After each successful transfer set error
       * counter to zero, so that other jobs can be
       * started.
       * Also move all, error entries back to the message
       * and file directories.
       */
      if (fsa[(int)db.position].error_counter > 0)
      {
         int  fd,
              j;
         char fd_wake_up_fifo[MAX_PATH_LENGTH];

         /* Number of errors that have occurred since last */
         /* successful transfer.                          */
         lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].error_counter - (char *)fsa);
         fsa[(int)db.position].error_counter = 0;

         /*
          * Wake up FD!
          */
         (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                       FIFO_DIR, FD_WAKE_UP_FIFO);
         if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to open() FIFO %s : %s (%s %d)\n",
                      fd_wake_up_fifo, strerror(errno),
                      __FILE__, __LINE__);
         }
         else
         {
            char dummy;

            if (write(fd, &dummy, 1) != 1)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to write() to FIFO %s : %s (%s %d)\n",
                         fd_wake_up_fifo, strerror(errno),
                         __FILE__, __LINE__);
            }
            if (close(fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Failed to close() FIFO %s : %s (%s %d)\n",
                         fd_wake_up_fifo, strerror(errno),
                         __FILE__, __LINE__);
            }
         }

         /*
          * Remove the error condition (NOT_WORKING) from all jobs
          * of this host.
          */
         for (j = 0; j < fsa[(int)db.position].allowed_transfers; j++)
         {
            if ((j != db.job_no) &&
                (fsa[(int)db.position].job_status[j].connect_status == NOT_WORKING))
            {
               fsa[(int)db.position].job_status[j].connect_status = DISCONNECT;
            }
         }
         unlock_region(fsa_fd, (char *)&fsa[(int)db.position].error_counter - (char *)fsa);

         /*
          * Since we have successfully transmitted a file, no need to
          * have the queue stopped anymore.
          */
         if (fsa[(int)db.position].host_status & AUTO_PAUSE_QUEUE_STAT)
         {
            fsa[(int)db.position].host_status ^= AUTO_PAUSE_QUEUE_STAT;
            (void)rec(sys_log_fd, INFO_SIGN,
                      "Starting queue for %s that was stopped by init_afd. (%s %d)\n",
                      fsa[(int)db.position].host_alias, __FILE__, __LINE__);
         }

         /*
          * Hmmm. This is very dangerous! But let see how it
          * works in practice.
          */
         if (fsa[(int)db.position].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
         {
            fsa[(int)db.position].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
         }
      }

      p_file_name_buffer += MAX_FILENAME_LENGTH;
      p_file_size_buffer++;
   } /* for (files_send = 0; files_send < files_to_send; files_send++) */

   free(buffer);

   (void)rec(transfer_log_fd, INFO_SIGN,
             "%-*s[%d]: %d Bytes mailed in %d file(s).\n",
             MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
             fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
             files_send);

   /* Logout again */
   if ((status = smtp_quit()) != SUCCESS)
   {
      if (fsa[(int)db.position].debug == YES)
      {
         if (timeout_flag == OFF)
         {
            (void)rec(trans_db_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to disconnect from SMTP-server. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      __FILE__, __LINE__);
            (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, msg_str);
         }
         else
         {
            (void)rec(trans_db_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Failed to disconnect from SMTP-server due to timeout. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, __FILE__, __LINE__);
         }
      }

      /*
       * Since all files have been transfered successful it is
       * not necessary to indicate an error in the status display.
       * It's enough when we say in the Transfer log that we failed
       * to log out.
       */
      (void)rec(transfer_log_fd, WARN_SIGN,
                "%-*s[%d]: Failed to log out (%d). #%d (%s %d)\n",
                MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                status, db.job_id, __FILE__, __LINE__);
   }
   else
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, INFO_SIGN,
                   "%-*s[%d]: Logged out. (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, __FILE__, __LINE__);
         (void)rec(trans_db_log_fd, INFO_SIGN, "%-*s[%d]: %s\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, msg_str);
      }
   }

   /* Don't need the ASCII buffer */
   free(smtp_buffer);

   /* Inform FSA that we have finished transferring */
   /* data and have disconnected.                  */
   reset_fsa(p_db, NO, (CONNECT_STATUS_VAR | NO_OF_FILES_VAR |
                        NO_OF_FILES_DONE_VAR | FILE_SIZE_DONE_VAR));

   /*
    * Remove file directory, but only when all files have
    * been transmitted.
    */
   if (files_to_send == files_send)
   {
      if (rmdir(file_path) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to remove directory %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "There are still files for %s. Will NOT remove this job! (%s %d)\n",
                file_path, __FILE__, __LINE__);
   }

   exit(TRANSFER_SUCCESS);
}


/*++++++++++++++++++++++++++++ sf_smtp_exit() +++++++++++++++++++++++++++*/
static void
sf_smtp_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   reset_fsa((struct job *)&db, NO, FILE_SIZE_VAR);

   if (db.user_rename_rule[0] != '\0')
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
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sf_fin_fifo, strerror(errno), __FILE__, __LINE__);
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
         (void)rec(sys_log_fd, WARN_SIGN,
                   "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      (void)close(fd);
   }
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   (void)rec(sys_log_fd, DEBUG_SIGN,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this! (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   reset_fsa((struct job *)&db, NO, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   exit(INCORRECT);
}
