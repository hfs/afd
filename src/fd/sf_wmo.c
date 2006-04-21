/*
 *  sf_wmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Deutscher Wetterdienst (DWD),
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
 **   sf_wmo - send files via TCP according WMO regulations
 **
 ** SYNOPSIS
 **   sf_wmo <work dir> <job no.> <FSA id> <FSA pos> <msg name> [options]
 **
 **   options
 **       --version        Version
 **       -a <age limit>   The age limit for the files being send.
 **       -A               Disable archiving of files.
 **       -o <retries>     Old/Error message and number of retries.
 **       -r               Resend from archive (job from show_olog).
 **       -t               Temp toggle.
 **
 ** DESCRIPTION
 **   sf_wmo sends the given files to the defined recipient via TCP
 **   according WMO regulations.
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
 **   30.05.1998 H.Kiehl Created
 **   08.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **   13.06.2004 H.Kiehl Added transfer rate limit.
 **   22.01.2005 H.Kiehl Added burst2.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <ctype.h>                     /* isdigit()                      */
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
#include "wmodefs.h"
#include "version.h"

/* Global variables */
int                        counter_fd = -1,     /* NOT USED */
                           exitflag = IS_FAULTY_VAR,
                           files_to_delete,     /* NOT USED */
                           no_of_hosts,    /* This variable is not used */
                                           /* in this module.           */
                           *p_no_of_hosts = NULL,
                           trans_rule_pos, /* Not used [init_sf()]      */
                           user_rule_pos,  /* Not used [init_sf()]      */
                           fsa_id,
                           fsa_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO,
                           timeout_flag;
#ifdef _WITH_BURST_2
unsigned int               burst_2_counter = 0;
#endif
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer = NULL;
long                       transfer_timeout;
char                       *p_work_dir = NULL,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           line_buffer[4096],
                           *del_file_name_buffer = NULL, /* NOT USED */
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa = NULL;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

#ifdef _WITH_WMO_SUPPORT
/* Local functions */
static void sf_wmo_exit(void),
            sig_bus(int),
            sig_segv(int),
            sig_kill(int),
            sig_exit(int);
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_WMO_SUPPORT
#ifdef _WITH_BURST_2
   int              cb2_ret;
#endif
   int              exit_status = TRANSFER_SUCCESS,
                    j,
                    fd,
                    status,
                    loops,
                    rest,
                    files_to_send,
                    files_send = 0,
                    blocksize,
                    wmo_counter_fd = -1;
   off_t            no_of_bytes;
   clock_t          clktck;
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
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
   off_t            *p_file_size_buffer;
   char             *p_file_name_buffer,
                    *buffer,
                    fullname[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
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
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_wmo_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   files_to_send = init_sf(argc, argv, file_path, WMO_FLAG);
   p_db = &db;
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
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
      exit(INCORRECT);
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
                      &ol_unl,
                      &ol_size,
                      &ol_transfer_time,
                      db.host_alias,
                      WMO);
   }
#endif

   timeout_flag = OFF;

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

   /* Connect to remote WMO-server */
   if ((status = wmo_connect(db.hostname, db.port, db.sndbuf_size)) != SUCCESS)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "WMO connection to <%s> at port %d failed (%d).",
                db.hostname, db.port, status);
      exit(eval_timeout(CONNECT_ERROR));
   }
   else
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "Connected to port %d.", db.port);
      }
   }

   /* Inform FSA that we have finished connecting and */
   /* will now start to transfer data.                */
   (void)gsf_check_fsa();
   if (db.fsa_pos != INCORRECT)
   {
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, db.lock_offset + LOCK_CON);
#endif
      fsa->job_status[(int)db.job_no].connect_status = WMO_ACTIVE;
      fsa->job_status[(int)db.job_no].no_of_files = files_to_send;
      fsa->connections += 1;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, db.lock_offset + LOCK_CON);
#endif
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 1 + 4 /* for bulletin end */)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(ALLOC_ERROR);
   }

   if (db.special_flag & WITH_SEQUENCE_NUMBER)
   {
      char counter_file_name[MAX_FILENAME_LENGTH];

      (void)sprintf(counter_file_name, "/%s.%d", db.host_alias, db.port);
      if ((wmo_counter_fd = open_counter_file(counter_file_name)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open counter file `%s'.", counter_file_name);
      }
   }

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL, "WMO Bursting.");
         }
# ifdef _OUTPUT_LOG
         if ((db.output_log == YES) && (ol_data == NULL))
         {
            output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
                            &ol_file_name_length, &ol_archive_name_length,
                            &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                            db.host_alias, WMO);
         }
# endif
      }
#endif

      /* Send all files */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         (void)sprintf(fullname, "%s/%s", file_path, p_file_name_buffer);

         if (*p_file_size_buffer > 0)
         {
            int end_length = 0,
                header_length = 0,
                length_type_indicator = 10;

            (void)gsf_check_fsa();
            if (db.fsa_pos != INCORRECT)
            {
               fsa->job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
               (void)strcpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                            p_file_name_buffer);
            }

            /* Open local file */
#ifdef O_LARGEFILE
            if ((fd = open(fullname, O_RDONLY | O_LARGEFILE)) == -1)
#else
            if ((fd = open(fullname, O_RDONLY)) == -1)
#endif
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Failed to open local file `%s' : %s",
                         fullname, strerror(errno));
               wmo_quit();
               exit(OPEN_LOCAL_ERROR);
            }
            if (fsa->debug > NORMAL_MODE)
            {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                               "Open local file `%s'", fullname);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               start_time = times(&tmsdummy);
            }
#endif

            /*
             * When the contents does not contain a bulletin header
             * it must be stored in the file name.
             */
            if (db.special_flag & FILE_NAME_IS_HEADER)
            {
               char *ptr = p_file_name_buffer;

               buffer[length_type_indicator] = 1; /* SOH */
               buffer[length_type_indicator + 1] = '\015'; /* CR */
               buffer[length_type_indicator + 2] = '\015'; /* CR */
               buffer[length_type_indicator + 3] = '\012'; /* LF */
               header_length = 4;

               if (wmo_counter_fd > 0)
               {
                  int wmo_counter;

                  if ((wmo_counter = next_wmo_counter(wmo_counter_fd)) < 0)
                  {
                     if (close(wmo_counter_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "close() error : %s", strerror(errno));
                     }
                     wmo_counter_fd = -1;
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to get next WMO counter.");
                  }
                  else
                  {
                     if (wmo_counter < 10)
                     {
                        buffer[length_type_indicator + header_length] = '0';
                        buffer[length_type_indicator + header_length + 1] = '0';
                        buffer[length_type_indicator + header_length + 2] = wmo_counter + '0';
                     }
                     else if (wmo_counter < 100)
                          {
                             buffer[length_type_indicator + header_length] = '0';
                             buffer[length_type_indicator + header_length + 1] = (wmo_counter / 10) + '0';
                             buffer[length_type_indicator + header_length + 2] = (wmo_counter % 10) + '0';
                          }
                     else if (wmo_counter < 1000)
                          {
                             buffer[length_type_indicator + header_length] = ((wmo_counter / 100) % 10) + '0';
                             buffer[length_type_indicator + header_length + 1] = ((wmo_counter / 10) % 10) + '0';
                             buffer[length_type_indicator + header_length + 2] = (wmo_counter % 10) + '0';
                          }
                     buffer[length_type_indicator + header_length + 3] = '\015'; /* CR */
                     buffer[length_type_indicator + header_length + 4] = '\015'; /* CR */
                     buffer[length_type_indicator + header_length + 5] = '\012'; /* LF */
                     header_length += 6;
                  }
               } /* if (wmo_counter_fd > 0) */

               for (;;)
               {
                  while ((*ptr != '_') && (*ptr != '-') &&
                         (*ptr != ' ') && (*ptr != '\0') && (*ptr != ';'))
                  {
                     buffer[length_type_indicator + header_length] = *ptr;
                     header_length++; ptr++;
                  }
                  if ((*ptr == '\0') || (*ptr == ';'))
                  {
                     break;
                  }
                  else
                  {
                     buffer[length_type_indicator + header_length] = ' ';
                     header_length++; ptr++;
                  }
               } /* for (;;) */
               buffer[length_type_indicator + header_length] = '\015'; /* CR */
               buffer[length_type_indicator + header_length + 1] = '\015'; /* CR */
               buffer[length_type_indicator + header_length + 2] = '\012'; /* LF */
               header_length += 3;
               end_length = 4;
            }

            /* Read (local) and write (remote) file */
            no_of_bytes = 0;
            loops = (length_type_indicator + header_length + *p_file_size_buffer) / blocksize;
            rest = (length_type_indicator + header_length + *p_file_size_buffer) % blocksize;

            if ((db.special_flag & FILE_NAME_IS_HEADER) && (rest == 0))
            {
               loops--;
               rest = blocksize;
            }

            /* Write length and type indicator. */
            (void)sprintf(buffer, "%08lu",
                          (unsigned long)(*p_file_size_buffer + header_length + end_length));
            if (db.transfer_mode == 'I')
            {
               buffer[length_type_indicator - 2] = 'B';
               buffer[length_type_indicator - 1] = 'I';
            }
            else if (db.transfer_mode == 'A')
                 {
                    buffer[length_type_indicator - 2] = 'A';
                    buffer[length_type_indicator - 1] = 'N';
                 }
                 else
                 {
                    buffer[length_type_indicator - 2] = 'F';
                    buffer[length_type_indicator - 1] = 'X';
                 }

            if (fsa->trl_per_process > 0)
            {
               init_limit_transfer_rate();
            }
            for (;;)
            {
               for (j = 0; j < loops; j++)
               {
#ifdef _SIMULATE_SLOW_TRANSFER
                  (void)sleep(2);
#endif
                  if ((status = read(fd,
                                     (buffer + length_type_indicator + header_length),
                                     (blocksize - length_type_indicator - header_length))) != (blocksize - length_type_indicator - header_length))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Could not read() local file `%s' : %s",
                               fullname, strerror(errno));
                     wmo_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if ((status = wmo_write(buffer, blocksize)) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write block from file `%s' to remote port %d [%d].",
                               p_file_name_buffer, db.port, status);
                     wmo_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  if (fsa->trl_per_process > 0)
                  {
                     limit_transfer_rate(blocksize, fsa->trl_per_process,
                                         clktck);
                  }

                  no_of_bytes += blocksize;

                  (void)gsf_check_fsa();
                  if (db.fsa_pos != INCORRECT)
                  {
                     fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                     fsa->job_status[(int)db.job_no].file_size_done += blocksize;
                     fsa->job_status[(int)db.job_no].bytes_send += blocksize;
                  }
                  if (length_type_indicator > 0)
                  {
                     length_type_indicator = 0;
                     header_length = 0;
                  }
               } /* for (j = 0; j < loops; j++) */

               if (rest > 0)
               {
                  if ((status = read(fd,
                                     (buffer + length_type_indicator + header_length),
                                     (rest - length_type_indicator - header_length))) != (rest - length_type_indicator - header_length))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Could not read() local file `%s' : %s",
                               fullname, strerror(errno));
                     wmo_quit();
                     exit(READ_LOCAL_ERROR);
                  }
                  if (end_length == 4)
                  {
                     buffer[rest] = '\015';
                     buffer[rest + 1] = '\015';
                     buffer[rest + 2] = '\012';
                     buffer[rest + 3] = 3;  /* ETX */
                  }
                  if ((status = wmo_write(buffer, rest + end_length)) != SUCCESS)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to write rest of file to remote port %d [%d].",
                               p_file_name_buffer, db.port, status);
                     wmo_quit();
                     exit(eval_timeout(WRITE_REMOTE_ERROR));
                  }
                  if (fsa->trl_per_process > 0)
                  {
                     limit_transfer_rate(rest + end_length,
                                         fsa->trl_per_process, clktck);
                  }

                  no_of_bytes += rest + end_length;

                  (void)gsf_check_fsa();
                  if (db.fsa_pos != INCORRECT)
                  {
                     fsa->job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                     fsa->job_status[(int)db.job_no].file_size_done += rest;
                     fsa->job_status[(int)db.job_no].bytes_send += rest;
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
                            "Hmmm. Failed to stat() `%s' : %s (%s %d)\n",
                            fullname, strerror(errno), __FILE__, __LINE__);
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
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "File `%s' for host %s was DEFINITELY NOT send in dot notation.",
                                p_file_name_buffer,
                                fsa->host_dsp_name);
                  }
                  else
                  {
                     break;
                  }
               }
            } /* for (;;) */

            if (db.special_flag & WMO_CHECK_ACKNOWLEDGE)
            {
               int ret;

               if ((ret = wmo_check_reply()) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            "Failed to receive reply from port %d.", db.port);
                  wmo_quit();
                  exit(eval_timeout(CHECK_REPLY_ERROR));
               }
               else if (ret == NEGATIV_ACKNOWLEDGE)
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                                 "Received negative acknowledge from remote port %d for file %s.",
                                 db.port, p_file_name_buffer);
                    }
            }

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
                         p_file_name_buffer, strerror(errno),
                         __FILE__, __LINE__);
               /*
                * Since we usually do not send more then 100 files and
                * sf_wmo() will exit(), there is no point in stopping
                * the transmission.
                */
            }
         }
         else
         {
            trans_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "File `%s' is of zero length, ignoring.",
                      p_file_name_buffer);
         }

         /* Update FSA, one file transmitted. */
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
                          "Total file counter for host %s less then zero. Correcting to %d.",
                          fsa->host_dsp_name, files_to_send - (files_send + 1));
               fsa->total_file_counter = files_to_send - (files_send + 1);
            }
#endif

            if (*p_file_size_buffer > 0)
            {
               /* Total file size */
               fsa->total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
               if (fsa->total_file_size < 0)
               {
                  int   k;
                  off_t *tmp_ptr = p_file_size_buffer;

                  tmp_ptr++;
                  fsa->total_file_size = 0;
                  for (k = (files_send + 1); k < files_to_send; k++)
                  {     
                     fsa->total_file_size += *tmp_ptr;
                  }

                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
# if SIZEOF_OFF_T == 4
                             "Total file size for host %s overflowed. Correcting to %ld.",
# else
                             "Total file size for host %s overflowed. Correcting to %lld.",
# endif
                             fsa->host_dsp_name, fsa->total_file_size);
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
               /* Number of bytes send */
               fsa->bytes_send += *p_file_size_buffer;
            }

            /* File counter done */
            fsa->file_counter_done += 1;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
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
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
            {
               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                               "Failed to archive file `%s'",
                               p_file_name_buffer);
               }

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if (unlink(fullname) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file `%s' after sending it successfully : %s",
                             fullname, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
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
                               "Archived file `%s'", p_file_name_buffer);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
                  (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
                  ol_file_name[*ol_file_name_length + 1] = '\0';
                  (*ol_file_name_length)++;
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1],
                               &db.archive_dir[db.archive_offset]);
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
            /* Delete the file we just have send */
            if (unlink(fullname) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file %s after sending it successfully : %s",
                          fullname, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)memcpy(ol_file_name, db.p_unique_name, db.unl);
               (void)strcpy(ol_file_name + db.unl, p_file_name_buffer);
               *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
               ol_file_name[*ol_file_name_length + 1] = '\0';
               (*ol_file_name_length)++;
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
         if ((*p_file_size_buffer > 0) && (fsa->error_counter > 0))
         {
            int  fd,
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
            (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                          FIFO_DIR, FD_WAKE_UP_FIFO);
            if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
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
               fsa->host_status ^= AUTO_PAUSE_QUEUE_STAT;
               error_action(fsa->host_alias, "stop");
               system_log(INFO_SIGN, __FILE__, __LINE__,
                          "Starting input queue for %s that was stopped by init_afd.",
                          fsa->host_alias);
            }
         } /* if (fsa->error_counter > 0) */

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

      /*
       * Remove file directory, but only when all files have
       * been transmitted.
       */
      if ((files_to_send == files_send) || (files_to_send < 1))
      {
         if (rmdir(file_path) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to remove directory %s : %s",
                       file_path, strerror(errno));
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "There are still %d files for %s. Will NOT remove this job!",
                    files_to_send - files_send, file_path);
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
#endif

   free(buffer);

   /* Disconnect from remote port */
   wmo_quit();
   if (fsa->debug > NORMAL_MODE)
   {
      trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                   "Disconnected from port %d.", db.port);
   }

   if (wmo_counter_fd > 0)
   {
      if (close(wmo_counter_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }
#endif /* _WITH_WMO_SUPPORT */

   exitflag = 0;
   exit(exit_status);
}


#ifdef _WITH_WMO_SUPPORT
/*+++++++++++++++++++++++++++++ sf_wmo_exit() +++++++++++++++++++++++++++*/
static void
sf_wmo_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   if ((fsa != NULL) && (db.fsa_pos >= 0))
   {
      if ((fsa->job_status[(int)db.job_no].file_size_done > 0) ||
          (fsa->job_status[(int)db.job_no].no_of_files_done > 0))
      {
#ifdef _WITH_BURST_2
# if SIZEOF_OFF_T == 4
         fd = sprintf(sf_fin_fifo, "%lu Bytes send in %d file(s).",
# else
         fd = sprintf(sf_fin_fifo, "%llu Bytes send in %d file(s).",
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
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo %s : %s", sf_fin_fifo, strerror(errno));
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
#endif /* _WITH_WMO_SUPPORT */
