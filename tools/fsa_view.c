/*
 *  fsa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Deutscher Wetterdienst (DWD),
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
 **   fsa_view [working directory] hostname|position
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
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
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
   int  i,
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
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
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

   if (position < 0)
   {
      if ((position = get_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr, "WARNING : Could not find host %s in FSA. (%s %d)\n",
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   (void)fprintf(stderr, "=============================> %s <=============================\n",
                 fsa[position].host_alias);
   (void)fprintf(stderr, "            (Number of hosts: %d   FSA ID: %d)\n\n",
                 no_of_hosts, fsa_id);
   (void)fprintf(stderr, "Real hostname 1    : %s\n", fsa[position].real_hostname[0]);
   (void)fprintf(stderr, "Real hostname 2    : %s\n", fsa[position].real_hostname[1]);
   (void)fprintf(stderr, "Hostname (display) : >%s<\n", fsa[position].host_dsp_name);
   if (fsa[position].host_toggle == HOST_ONE)
   {
      (void)fprintf(stderr, "Host toggle        : HOST_ONE\n");
   }
   else if (fsa[position].host_toggle == HOST_TWO)
        {
           (void)fprintf(stderr, "Host toggle        : HOST_TWO\n");
        }
        else
        {
           (void)fprintf(stderr, "Host toggle        : HOST_???\n");
        }
   if (fsa[position].auto_toggle == ON)
   {
      (void)fprintf(stderr, "Auto toggle        : ON\n");
   }
   else
   {
      (void)fprintf(stderr, "Auto toggle        : OFF\n");
   }
   if (fsa[position].original_toggle_pos == HOST_ONE)
   {
      (void)fprintf(stderr, "Original toggle    : HOST_ONE\n");
   }
   else if (fsa[position].original_toggle_pos == HOST_TWO)
        {
           (void)fprintf(stderr, "Original toggle    : HOST_TWO\n");
        }
        else if (fsa[position].original_toggle_pos == NONE)
             {
                (void)fprintf(stderr, "Original toggle    : NONE\n");
             }
             else
             {
                (void)fprintf(stderr, "Original toggle    : HOST_???\n");
             }
   (void)fprintf(stderr, "Toggle position    : %d\n", fsa[position].toggle_pos);
   (void)fprintf(stderr, "Protocol (%4d)    : ", fsa[position].protocol);
   if (fsa[position].protocol & FTP_FLAG)
   {
      (void)fprintf(stderr, "FTP ");
   }
   if (fsa[position].protocol & LOC_FLAG)
   {
      (void)fprintf(stderr, "LOC ");
   }
   if (fsa[position].protocol & SMTP_FLAG)
   {
      (void)fprintf(stderr, "SMTP ");
   }
#ifdef _WITH_MAP_SUPPORT
   if (fsa[position].protocol & MAP_FLAG)
   {
      (void)fprintf(stderr, "MAP ");
   }
#endif
#ifdef _WITH_WMO_SUPPORT
   if (fsa[position].protocol & WMO_FLAG)
   {
      (void)fprintf(stderr, "WMO ");
   }
#endif
   (void)fprintf(stderr, "\n");
   if (fsa[position].proxy_name[0] != '\0')
   {
      (void)fprintf(stderr, "Proxy name         : >%s<\n", fsa[position].proxy_name);
   }
   else
   {
      (void)fprintf(stderr, "Proxy name         : NONE\n");
   }
   if (fsa[position].debug == NO)
   {
      (void)fprintf(stderr, "Debug mode         : NO\n");
   }
   else
   {
      (void)fprintf(stderr, "Debug mode         : YES\n");
   }
   (void)fprintf(stderr, "Host status (%4d) : ", fsa[position].host_status);
   if (fsa[position].host_status & PAUSE_QUEUE_STAT)
   {
      (void)fprintf(stderr, "PAUSE_QUEUE ");
   }
   if (fsa[position].host_status & AUTO_PAUSE_QUEUE_STAT)
   {
      (void)fprintf(stderr, "AUTO_PAUSE_QUEUE ");
   }
   if (fsa[position].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
   {
      (void)fprintf(stderr, "AUTO_PAUSE_QUEUE_LOCK ");
   }
   if (fsa[position].host_status & STOP_TRANSFER_STAT)
   {
      (void)fprintf(stderr, "STOP_TRANSFER ");
   }
   if (fsa[position].error_counter >= fsa[position].max_errors)
   {
      if (fsa[position].error_counter >= (2 * fsa[position].max_errors))
      {
         (void)fprintf(stderr, "NOT_WORKING2 ");
      }
      else
      {
         (void)fprintf(stderr, "NOT_WORKING ");
      }
   }
   if (fsa[position].active_transfers > 0)
   {
      (void)fprintf(stderr, "TRANSFER_ACTIVE");
   }
   else
   {
      (void)fprintf(stderr, "NORMAL_STATUS");
   }
   (void)fprintf(stderr, "\n");

   (void)fprintf(stderr, "Transfer timeout   : %ld\n", fsa[position].transfer_timeout);
   (void)fprintf(stderr, "File size offset   : %d\n", fsa[position].file_size_offset);
   (void)fprintf(stderr, "Successful retries : %d\n", fsa[position].successful_retries);
   (void)fprintf(stderr, "MaxSuccessful ret. : %d\n", fsa[position].max_successful_retries);
   (void)fprintf(stderr, "Special flag       : %d\n", fsa[position].special_flag);
   (void)fprintf(stderr, "Error counter      : %d\n", fsa[position].error_counter);
   (void)fprintf(stderr, "Total errors       : %u\n", fsa[position].total_errors);
   (void)fprintf(stderr, "Max. errors        : %d\n", fsa[position].max_errors);
   (void)fprintf(stderr, "Retry interval     : %d\n", fsa[position].retry_interval);
   (void)fprintf(stderr, "Transfer block size: %d\n", fsa[position].block_size);
   (void)fprintf(stderr, "Time of last retry : %s", ctime(&fsa[position].last_retry_time));
   (void)fprintf(stderr, "Last connection    : %s", ctime(&fsa[position].last_connection));
   (void)fprintf(stderr, "Total file counter : %d\n", fsa[position].total_file_counter);
   (void)fprintf(stderr, "Total file size    : %lu\n", fsa[position].total_file_size);
   (void)fprintf(stderr, "File counter done  : %u\n", fsa[position].file_counter_done);
   (void)fprintf(stderr, "Bytes send         : %lu\n", fsa[position].bytes_send);
   (void)fprintf(stderr, "Connections        : %u\n", fsa[position].connections);
   (void)fprintf(stderr, "Active transfers   : %d\n", fsa[position].active_transfers);
   (void)fprintf(stderr, "Allowed transfers  : %d\n", fsa[position].allowed_transfers);
   (void)fprintf(stderr, "Transfer rate      : %d\n", fsa[position].transfer_rate);

   (void)fprintf(stderr, "                    |   Job 0   |   Job 1   |   Job 2   |   Job 3   |   Job 4   \n");
   (void)fprintf(stderr, "--------------------+-----------+-----------+-----------+-----------+-----------\n");
   (void)fprintf(stderr, "PID                 | %9d | %9d | %9d | %9d | %9d \n", fsa[position].job_status[0].proc_id, fsa[position].job_status[1].proc_id, fsa[position].job_status[2].proc_id, fsa[position].job_status[3].proc_id, fsa[position].job_status[4].proc_id);
   (void)fprintf(stderr, "Connect status      ");
   for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
   {
      switch(fsa[position].job_status[i].connect_status)
      {
         case TRANSFER_ACTIVE :

            (void)fprintf(stderr, "|TRANS ACTIV");
            break;

         case CONNECTING :

            (void)fprintf(stderr, "|CONNECTING ");
            break;

         case DISCONNECT :

            (void)fprintf(stderr, "|DISCONNECT ");
            break;

         case NOT_WORKING :

            (void)fprintf(stderr, "|NOT WORKING");
            break;

         case FTP_BURST_TRANSFER_ACTIVE :

            (void)fprintf(stderr, "| FTP BURST ");
            break;

         case EMAIL_ACTIVE :

            (void)fprintf(stderr, "|  MAILING  ");
            break;

#ifdef _WITH_WMO_SUPPORT

         case WMO_BURST_TRANSFER_ACTIVE :

            (void)fprintf(stderr, "| WMO BURST ");
            break;

         case WMO_ACTIVE :

            (void)fprintf(stderr, "| WMO ACTIV ");
            break;
#endif

#ifdef _WITH_MAP_SUPPORT
         case MAP_ACTIVE :

            (void)fprintf(stderr, "| MAP ACTIV ");
            break;
#endif

         default :

            (void)fprintf(stderr, "|  Unknown  ");
            break;
      }
   }
   (void)fprintf(stderr, "\n");
   (void)fprintf(stderr, "Number of files     | %9d | %9d | %9d | %9d | %9d \n", fsa[position].job_status[0].no_of_files, fsa[position].job_status[1].no_of_files, fsa[position].job_status[2].no_of_files, fsa[position].job_status[3].no_of_files, fsa[position].job_status[4].no_of_files);
   (void)fprintf(stderr, "No. of files done   | %9d | %9d | %9d | %9d | %9d \n", fsa[position].job_status[0].no_of_files_done, fsa[position].job_status[1].no_of_files_done, fsa[position].job_status[2].no_of_files_done, fsa[position].job_status[3].no_of_files_done, fsa[position].job_status[4].no_of_files_done);
   (void)fprintf(stderr, "File size           | %9lu | %9lu | %9lu | %9lu | %9lu \n", fsa[position].job_status[0].file_size, fsa[position].job_status[1].file_size, fsa[position].job_status[2].file_size, fsa[position].job_status[3].file_size, fsa[position].job_status[4].file_size);
   (void)fprintf(stderr, "File size done      | %9lu | %9lu | %9lu | %9lu | %9lu \n", fsa[position].job_status[0].file_size_done, fsa[position].job_status[1].file_size_done, fsa[position].job_status[2].file_size_done, fsa[position].job_status[3].file_size_done, fsa[position].job_status[4].file_size_done);
   (void)fprintf(stderr, "Bytes send          | %9lu | %9lu | %9lu | %9lu | %9lu \n", fsa[position].job_status[0].bytes_send, fsa[position].job_status[1].bytes_send, fsa[position].job_status[2].bytes_send, fsa[position].job_status[3].bytes_send, fsa[position].job_status[4].bytes_send);
   (void)fprintf(stderr, "File name in use    |%11.11s|%11.11s|%11.11s|%11.11s|%11.11s\n", fsa[position].job_status[0].file_name_in_use, fsa[position].job_status[1].file_name_in_use, fsa[position].job_status[2].file_name_in_use, fsa[position].job_status[3].file_name_in_use, fsa[position].job_status[4].file_name_in_use);
   (void)fprintf(stderr, "File size in use    | %9lu | %9lu | %9lu | %9lu | %9lu \n", fsa[position].job_status[0].file_size_in_use, fsa[position].job_status[1].file_size_in_use, fsa[position].job_status[2].file_size_in_use, fsa[position].job_status[3].file_size_in_use, fsa[position].job_status[4].file_size_in_use);
   (void)fprintf(stderr, "Filesize in use done| %9lu | %9lu | %9lu | %9lu | %9lu \n", fsa[position].job_status[0].file_size_in_use_done, fsa[position].job_status[1].file_size_in_use_done, fsa[position].job_status[2].file_size_in_use_done, fsa[position].job_status[3].file_size_in_use_done, fsa[position].job_status[4].file_size_in_use_done);
#ifdef _BURST_MODE
   (void)fprintf(stderr, "Unique name         |%11.11s|%11.11s|%11.11s|%11.11s|%11.11s\n", fsa[position].job_status[0].unique_name, fsa[position].job_status[1].unique_name, fsa[position].job_status[2].unique_name, fsa[position].job_status[3].unique_name, fsa[position].job_status[4].unique_name);
   (void)fprintf(stderr, "Burst counter       | %9d | %9d | %9d | %9d | %9d \n", fsa[position].job_status[0].burst_counter, fsa[position].job_status[1].burst_counter, fsa[position].job_status[2].burst_counter, fsa[position].job_status[3].burst_counter, fsa[position].job_status[4].burst_counter);
   (void)fprintf(stderr, "Job ID              | %9d | %9d | %9d | %9d | %9d \n", (int)fsa[position].job_status[0].job_id, (int)fsa[position].job_status[1].job_id, (int)fsa[position].job_status[2].job_id, (int)fsa[position].job_status[3].job_id, (int)fsa[position].job_status[4].job_id);
   (void)fprintf(stderr, "Error file (0 = NO) | %9d | %9d | %9d | %9d | %9d \n", fsa[position].job_status[0].error_file, fsa[position].job_status[1].error_file, fsa[position].job_status[2].error_file, fsa[position].job_status[3].error_file, fsa[position].job_status[4].error_file);
#endif

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr, "SYNTAX  : fsa_view [-w working directory] hostname|position\n");
   return;
}
