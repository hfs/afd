/*
 *  fsa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2002 Deutscher Wetterdienst (DWD),
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
 **   fsa_view - shows all information in the FSA about a specific
 **              host
 **
 ** SYNOPSIS
 **   fsa_view [--version] [-w working directory] hostname|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific host in the
 **   FSA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.02.1996 H.Kiehl Created
 **   05.01.1997 H.Kiehl Added support for burst mode.
 **   21.08.1997 H.Kiehl Show real hostname as well.
 **   12.10.1997 H.Kiehl Show bursting and mailing.
 **   05.12.2000 H.Kiehl If available show host toggle string.
 **   04.08.2001 H.Kiehl Show more details of special_flag and added
 **                      active|passive mode and idle time to protocol.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr, stdout    */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "version.h"

/* Local functions */
static void usage(void);

int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_id,
                           fsa_fd = -1,
                           no_of_hosts = 0;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fsa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i, j,
        last,
        position = -1;
   char hostname[MAX_HOSTNAME_LENGTH + 1],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit(argv[1][0]) != 0)
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr, "WARNING : Could not find host %s in FSA. (%s %d)\n",
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_hosts;
           position = 0;
        }
   else if (position >= no_of_hosts)
        {
           (void)fprintf(stderr,
                         "WARNING : There are only %d directories in the FSA. (%s %d)\n",
                         no_of_hosts, __FILE__, __LINE__);
           exit(INCORRECT);
        }

   (void)fprintf(stdout, "            Number of hosts: %d   FSA ID: %d\n\n",
                 no_of_hosts, fsa_id);
   for (j = position; j < last; j++)
   {
      (void)fprintf(stdout, "=============================> %s <=============================\n",
                    fsa[j].host_alias);
      (void)fprintf(stdout, "Real hostname 1    : %s\n", fsa[j].real_hostname[0]);
      (void)fprintf(stdout, "Real hostname 2    : %s\n", fsa[j].real_hostname[1]);
      (void)fprintf(stdout, "Hostname (display) : >%s<\n", fsa[j].host_dsp_name);
      if (fsa[j].host_toggle == HOST_ONE)
      {
         (void)fprintf(stdout, "Host toggle        : HOST_ONE\n");
      }
      else if (fsa[j].host_toggle == HOST_TWO)
           {
              (void)fprintf(stdout, "Host toggle        : HOST_TWO\n");
           }
           else
           {
              (void)fprintf(stdout, "Host toggle        : HOST_???\n");
           }
      if (fsa[j].auto_toggle == ON)
      {
         (void)fprintf(stdout, "Auto toggle        : ON\n");
      }
      else
      {
         (void)fprintf(stdout, "Auto toggle        : OFF\n");
      }
      if (fsa[j].original_toggle_pos == HOST_ONE)
      {
         (void)fprintf(stdout, "Original toggle    : HOST_ONE\n");
      }
      else if (fsa[j].original_toggle_pos == HOST_TWO)
           {
              (void)fprintf(stdout, "Original toggle    : HOST_TWO\n");
           }
      else if (fsa[j].original_toggle_pos == NONE)
           {
              (void)fprintf(stdout, "Original toggle    : NONE\n");
           }
           else
           {
              (void)fprintf(stdout, "Original toggle    : HOST_???\n");
           }
      (void)fprintf(stdout, "Toggle position    : %d\n", fsa[j].toggle_pos);
      if (fsa[j].host_toggle_str[0] != '\0')
      {
         (void)fprintf(stdout, "Host toggle string : %s\n",
                       fsa[j].host_toggle_str);
      }
      (void)fprintf(stdout, "Protocol (%8d): ", fsa[j].protocol);
      if (fsa[j].protocol & FTP_FLAG)
      {
         (void)fprintf(stdout, "FTP ");
         if (fsa[j].protocol & FTP_PASSIVE_MODE)
         {
            (void)fprintf(stdout, "passive ");
         }
         else
         {
            (void)fprintf(stdout, "active ");
         }
         if (fsa[j].protocol & SET_IDLE_TIME)
         {
            (void)fprintf(stdout, "idle ");
         }
      }
      if (fsa[j].protocol & LOC_FLAG)
      {
         (void)fprintf(stdout, "LOC ");
      }
      if (fsa[j].protocol & SMTP_FLAG)
      {
         (void)fprintf(stdout, "SMTP ");
      }
#ifdef _WITH_MAP_SUPPORT
      if (fsa[j].protocol & MAP_FLAG)
      {
         (void)fprintf(stdout, "MAP ");
      }
#endif
#ifdef _WITH_SCP1_SUPPORT
      if (fsa[j].protocol & SCP1_FLAG)
      {
         (void)fprintf(stdout, "SCP1 ");
      }
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
      if (fsa[j].protocol & WMO_FLAG)
      {
         (void)fprintf(stdout, "WMO ");
      }
#endif
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Direction          : ");
      if (fsa[j].protocol & SEND_FLAG)
      {
         (void)fprintf(stdout, "SEND ");
      }
      if (fsa[j].protocol & RETRIEVE_FLAG)
      {
         (void)fprintf(stdout, "RETRIEVE ");
      }
      (void)fprintf(stdout, "\n");
      if (fsa[j].proxy_name[0] != '\0')
      {
         (void)fprintf(stdout, "Proxy name         : >%s<\n", fsa[j].proxy_name);
      }
      else
      {
         (void)fprintf(stdout, "Proxy name         : NONE\n");
      }
      if (fsa[j].debug == NO)
      {
         (void)fprintf(stdout, "Debug mode         : NO\n");
      }
      else
      {
         (void)fprintf(stdout, "Debug mode         : YES\n");
      }
      (void)fprintf(stdout, "Host status (%4d) : ", fsa[j].host_status);
      if (fsa[j].host_status & PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "PAUSE_QUEUE ");
      }
      if (fsa[j].host_status & AUTO_PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "AUTO_PAUSE_QUEUE ");
      }
      if (fsa[j].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
      {
         (void)fprintf(stdout, "AUTO_PAUSE_QUEUE_LOCK ");
      }
      if (fsa[j].host_status & STOP_TRANSFER_STAT)
      {
         (void)fprintf(stdout, "STOP_TRANSFER ");
      }
      if (fsa[j].host_status & HOST_CONFIG_HOST_DISABLED)
      {
         (void)fprintf(stdout, "HOST_CONFIG_HOST_DISABLED ");
      }
      if (fsa[j].host_status & DANGER_PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "DANGER_PAUSE_QUEUE_STAT ");
      }
      if (fsa[j].error_counter >= fsa[j].max_errors)
      {
         if (fsa[j].error_counter >= (2 * fsa[j].max_errors))
         {
            (void)fprintf(stdout, "NOT_WORKING2 ");
         }
         else
         {
            (void)fprintf(stdout, "NOT_WORKING ");
         }
      }
      if (fsa[j].active_transfers > 0)
      {
         (void)fprintf(stdout, "TRANSFER_ACTIVE");
      }
      else
      {
         (void)fprintf(stdout, "NORMAL_STATUS");
      }
      (void)fprintf(stdout, "\n");

      (void)fprintf(stdout, "Transfer timeout   : %ld\n",
                    fsa[j].transfer_timeout);
      (void)fprintf(stdout, "File size offset   : %d\n",
                    fsa[j].file_size_offset);
      (void)fprintf(stdout, "Successful retries : %d\n",
                    fsa[j].successful_retries);
      (void)fprintf(stdout, "MaxSuccessful ret. : %d\n",
                    fsa[j].max_successful_retries);
      (void)fprintf(stdout, "Special flag (%3d) : ", fsa[j].special_flag);
      if (fsa[j].special_flag & HOST_DISABLED)
      {
         (void)fprintf(stdout, "HOST_DISABLED ");
      }
      if (fsa[j].special_flag & HOST_IN_DIR_CONFIG)
      {
         (void)fprintf(stdout, "HOST_IN_DIR_CONFIG ");
      }
      if (fsa[j].special_flag & ERROR_FILE_UNDER_PROCESS)
      {
         (void)fprintf(stdout, "ERROR_FILE_UNDER_PROCESS ");
      }
      (void)fprintf(stdout, "NO_BURST=%d\n",
                    fsa[j].special_flag & NO_BURST_COUNT_MASK);
      (void)fprintf(stdout, "Error counter      : %d\n",
                    fsa[j].error_counter);
      (void)fprintf(stdout, "Total errors       : %u\n",
                    fsa[j].total_errors);
      (void)fprintf(stdout, "Max. errors        : %d\n",
                    fsa[j].max_errors);
      (void)fprintf(stdout, "Retry interval     : %d\n",
                    fsa[j].retry_interval);
      (void)fprintf(stdout, "Transfer block size: %d\n",
                    fsa[j].block_size);
      (void)fprintf(stdout, "Time of last retry : %s",
                    ctime(&fsa[j].last_retry_time));
      (void)fprintf(stdout, "Last connection    : %s",
                    ctime(&fsa[j].last_connection));
      (void)fprintf(stdout, "Total file counter : %d\n",
                    fsa[j].total_file_counter);
      (void)fprintf(stdout, "Total file size    : %lu\n",
                    fsa[j].total_file_size);
      (void)fprintf(stdout, "File counter done  : %u\n",
                    fsa[j].file_counter_done);
      (void)fprintf(stdout, "Bytes send         : %lu\n",
                    fsa[j].bytes_send);
      (void)fprintf(stdout, "Connections        : %u\n",
                    fsa[j].connections);
      (void)fprintf(stdout, "Jobs queued        : %u\n",
                    fsa[j].jobs_queued);
      (void)fprintf(stdout, "Active transfers   : %d\n",
                    fsa[j].active_transfers);
      (void)fprintf(stdout, "Allowed transfers  : %d\n",
                    fsa[j].allowed_transfers);

      (void)fprintf(stdout, "                    |   Job 0   |   Job 1   |   Job 2   |   Job 3   |   Job 4   \n");
      (void)fprintf(stdout, "--------------------+-----------+-----------+-----------+-----------+-----------\n");
      (void)fprintf(stdout, "PID                 |%10d |%10d |%10d |%10d |%10d \n", fsa[j].job_status[0].proc_id, fsa[j].job_status[1].proc_id, fsa[j].job_status[2].proc_id, fsa[j].job_status[3].proc_id, fsa[j].job_status[4].proc_id);
      (void)fprintf(stdout, "Connect status      ");
      for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
      {
         switch(fsa[j].job_status[i].connect_status)
         {
            case CONNECTING :
               (void)fprintf(stdout, "|CON or LOCB");
               break;

            case DISCONNECT :
               (void)fprintf(stdout, "|DISCONNECT ");
               break;

            case NOT_WORKING :
               (void)fprintf(stdout, "|NOT WORKING");
               break;

            case FTP_ACTIVE :
               (void)fprintf(stdout, "|    FTP    ");
               break;

            case FTP_BURST_TRANSFER_ACTIVE :
               (void)fprintf(stdout, "| FTP BURST ");
               break;

            case FTP_BURST2_TRANSFER_ACTIVE :
               (void)fprintf(stdout, "| FTP BURST2");
               break;

            case LOC_ACTIVE :
               (void)fprintf(stdout, "|    LOC    ");
               break;

            case EMAIL_ACTIVE :
               (void)fprintf(stdout, "|  MAILING  ");
               break;

#ifdef _WITH_SCP1_SUPPORT
            case SCP1_BURST_TRANSFER_ACTIVE :
               (void)fprintf(stdout, "| SCP1 BURST");
               break;

            case SCP1_ACTIVE :
               (void)fprintf(stdout, "| SCP1 ACTIV");
               break;
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
            case WMO_BURST_TRANSFER_ACTIVE :
               (void)fprintf(stdout, "| WMO BURST ");
               break;

            case WMO_ACTIVE :
               (void)fprintf(stdout, "| WMO ACTIV ");
               break;
#endif

#ifdef _WITH_MAP_SUPPORT
            case MAP_ACTIVE :
               (void)fprintf(stdout, "| MAP ACTIV ");
               break;
#endif

            case CLOSING_CONNECTION :
               (void)fprintf(stdout, "|CLOSING CON");
               break;

            default :
               (void)fprintf(stdout, "|  Unknown  ");
               break;
         }
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout,
                    "Number of files     |%10d |%10d |%10d |%10d |%10d \n",
                    fsa[j].job_status[0].no_of_files,
                    fsa[j].job_status[1].no_of_files,
                    fsa[j].job_status[2].no_of_files,
                    fsa[j].job_status[3].no_of_files,
                    fsa[j].job_status[4].no_of_files);
      (void)fprintf(stdout,
                    "No. of files done   |%10d |%10d |%10d |%10d |%10d \n",
                    fsa[j].job_status[0].no_of_files_done,
                    fsa[j].job_status[1].no_of_files_done,
                    fsa[j].job_status[2].no_of_files_done,
                    fsa[j].job_status[3].no_of_files_done,
                    fsa[j].job_status[4].no_of_files_done);
      (void)fprintf(stdout,
                    "File size           |%10lu |%10lu |%10lu |%10lu |%10lu \n",
                    fsa[j].job_status[0].file_size,
                    fsa[j].job_status[1].file_size,
                    fsa[j].job_status[2].file_size,
                    fsa[j].job_status[3].file_size,
                    fsa[j].job_status[4].file_size);
      (void)fprintf(stdout,
                    "File size done      |%10lu |%10lu |%10lu |%10lu |%10lu \n",
                    fsa[j].job_status[0].file_size_done,
                    fsa[j].job_status[1].file_size_done,
                    fsa[j].job_status[2].file_size_done,
                    fsa[j].job_status[3].file_size_done,
                    fsa[j].job_status[4].file_size_done);
      (void)fprintf(stdout,
                    "Bytes send          |%10lu |%10lu |%10lu |%10lu |%10lu \n",
                    fsa[j].job_status[0].bytes_send,
                    fsa[j].job_status[1].bytes_send,
                    fsa[j].job_status[2].bytes_send,
                    fsa[j].job_status[3].bytes_send,
                    fsa[j].job_status[4].bytes_send);
      (void)fprintf(stdout,
                    "File name in use    |%11.11s|%11.11s|%11.11s|%11.11s|%11.11s\n",
                    fsa[j].job_status[0].file_name_in_use,
                    fsa[j].job_status[1].file_name_in_use,
                    fsa[j].job_status[2].file_name_in_use,
                    fsa[j].job_status[3].file_name_in_use,
                    fsa[j].job_status[4].file_name_in_use);
      (void)fprintf(stdout,
                    "File size in use    |%10lu |%10lu |%10lu |%10lu |%10lu \n",
                    fsa[j].job_status[0].file_size_in_use,
                    fsa[j].job_status[1].file_size_in_use,
                    fsa[j].job_status[2].file_size_in_use,
                    fsa[j].job_status[3].file_size_in_use,
                    fsa[j].job_status[4].file_size_in_use);
      (void)fprintf(stdout,
                    "Filesize in use done|%10lu |%10lu |%10lu |%10lu |%10lu \n",
                    fsa[j].job_status[0].file_size_in_use_done,
                    fsa[j].job_status[1].file_size_in_use_done,
                    fsa[j].job_status[2].file_size_in_use_done,
                    fsa[j].job_status[3].file_size_in_use_done,
                    fsa[j].job_status[4].file_size_in_use_done);
#ifdef _BURST_MODE
      (void)fprintf(stdout,
                    "Unique name         |%11.11s|%11.11s|%11.11s|%11.11s|%11.11s\n",
                    fsa[j].job_status[0].unique_name,
                    fsa[j].job_status[1].unique_name,
                    fsa[j].job_status[2].unique_name,
                    fsa[j].job_status[3].unique_name,
                    fsa[j].job_status[4].unique_name);
      (void)fprintf(stdout,
                    "Burst counter       | %9d | %9d | %9d | %9d | %9d \n",
                    fsa[j].job_status[0].burst_counter,
                    fsa[j].job_status[1].burst_counter,
                    fsa[j].job_status[2].burst_counter,
                    fsa[j].job_status[3].burst_counter,
                    fsa[j].job_status[4].burst_counter);
      (void)fprintf(stdout,
                    "Job ID              |%10d |%10d |%10d |%10d |%10d \n",
                    (int)fsa[j].job_status[0].job_id,
                    (int)fsa[j].job_status[1].job_id,
                    (int)fsa[j].job_status[2].job_id,
                    (int)fsa[j].job_status[3].job_id,
                    (int)fsa[j].job_status[4].job_id);
      (void)fprintf(stdout,
                    "Error file (0 = NO) | %9d | %9d | %9d | %9d | %9d \n",
                    fsa[j].job_status[0].error_file,
                    fsa[j].job_status[1].error_file,
                    fsa[j].job_status[2].error_file,
                    fsa[j].job_status[3].error_file,
                    fsa[j].job_status[4].error_file);
#endif
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : fsa_view [--version] [-w working directory] hostname|position\n");
   return;
}
