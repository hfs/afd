/*
 *  afd_status.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_status - shows status of AFD by printing contents of
 **                afd_status structure
 **
 ** SYNOPSIS
 **   afd_status [-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.10.1998 H.Kiehl Created
 **
 */

#include <stdio.h>
#include <string.h>                   /* strcmp() in CHECK_FOR_VERSION   */
#include <stdlib.h>                   /* exit()                          */
#include <time.h>                     /* ctime()                         */
#include <unistd.h>                   /* STDERR_FILENO                   */
#include "version.h"

/* Global variables */
int               sys_log_fd = STDERR_FILENO;
char              *p_work_dir;
struct afd_status *p_afd_status;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afd_status() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i;
   char work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   /* Attach to the AFD Status Area */
   if (attach_afd_status(NULL) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to map to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   (void)fprintf(stdout, "AMG                  : %d\n", p_afd_status->amg);
   (void)fprintf(stdout, "AMG jobs             : %d\n", p_afd_status->amg_jobs);
   (void)fprintf(stdout, "FD                   : %d\n", p_afd_status->fd);
   (void)fprintf(stdout, "System log           : %d\n", p_afd_status->sys_log);
   (void)fprintf(stdout, "Transfer log         : %d\n", p_afd_status->trans_log);
   (void)fprintf(stdout, "Trans debug log      : %d\n", p_afd_status->trans_db_log);
   (void)fprintf(stdout, "Archive watch        : %d\n", p_afd_status->archive_watch);
   (void)fprintf(stdout, "afd_stat             : %d\n", p_afd_status->afd_stat);
   (void)fprintf(stdout, "afdd                 : %d\n", p_afd_status->afdd);
#ifdef _NO_MMAP
   (void)fprintf(stdout, "mapper               : %d\n", p_afd_status->mapper);
#endif
#ifdef _INPUT_LOG
   (void)fprintf(stdout, "input_log            : %d\n", p_afd_status->input_log);
#endif
#ifdef _OUTPUT_LOG
   (void)fprintf(stdout, "output_log           : %d\n", p_afd_status->output_log);
#endif
#ifdef _DELETE_LOG
   (void)fprintf(stdout, "delete_log           : %d\n", p_afd_status->delete_log);
#endif
   (void)fprintf(stdout, "Receivelog indicator : %u <",
                 p_afd_status->receive_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->receive_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "Receive log history  :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->receive_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Syslog indicator     : %u <",
                 p_afd_status->sys_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->sys_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "System log history   :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->sys_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case CONFIG_ID :
            (void)fprintf(stdout, " C");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Translog indicator   : %u <",
                 p_afd_status->trans_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (p_afd_status->trans_log_fifo[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;
         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;
         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;
         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;
         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, " >\n");
   (void)fprintf(stdout, "Transfer log history :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (p_afd_status->trans_log_history[i])
      {
         case INFO_ID :
            (void)fprintf(stdout, " I");
            break;

         case ERROR_ID :
            (void)fprintf(stdout, " E");
            break;

         case WARNING_ID :
            (void)fprintf(stdout, " W");
            break;

         case FAULTY_ID :
            (void)fprintf(stdout, " F");
            break;

         default :
            (void)fprintf(stdout, " ?");
            break;
      }
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Number of transfers  : %d\n", p_afd_status->no_of_transfers);
   (void)fprintf(stdout, "Jobs in queue        : %d\n", p_afd_status->jobs_in_queue);
   (void)fprintf(stdout, "AMG fork() counter   : %u\n", p_afd_status->amg_fork_counter);
   (void)fprintf(stdout, "FD fork() counter    : %u\n", p_afd_status->fd_fork_counter);
   (void)fprintf(stdout, "AMG burst counter    : %u\n", p_afd_status->amg_burst_counter);
   (void)fprintf(stdout, "FD burst counter     : %u\n", p_afd_status->fd_burst_counter);
   (void)fprintf(stdout, "Burst2 counter       : %u\n", p_afd_status->burst2_counter);
   (void)fprintf(stdout, "AFD start time       : %s", ctime(&p_afd_status->start_time));

   exit(SUCCESS);
}
