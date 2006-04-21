/*
 *  msa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2005 Deutscher Wetterdienst (DWD),
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
 **   msa_view - shows all information in the MSA about a specific
 **              AFD
 **
 ** SYNOPSIS
 **   msa_view [-w <working directory>] afdname|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific AFD in the
 **   MSA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.01.1999 H.Kiehl Created
 **   13.09.2000 H.Kiehl Addition of top number of transfers.
 **   03.12.2003 H.Kiehl Added connection and disconnection time.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
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
#include "mondefs.h"
#include "version.h"

/* Local functions */
static void usage(void);

int                    sys_log_fd = STDERR_FILENO,   /* Not used!    */
                       msa_id,
                       msa_fd = -1,
                       no_of_afds = 0;
off_t                  msa_size;
char                   *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ msa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  i,
        position = -1;
   char hostname[MAX_HOSTNAME_LENGTH + 1],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         (void)strcpy(hostname, argv[1]);
      }
   }
   else
   {
      usage();
      exit(INCORRECT);
   }

   if (msa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (position < 0)
   {
      for (i = 0; i < no_of_afds; i++)
      {
         if (strcmp(msa[i].afd_alias, hostname) == 0)
         {
            position = i;
            break;
         }
      }
      if (position < 0)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not find AFD `%s' in MSA. (%s %d)\n",
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   (void)fprintf(stdout, "=============================> %s <=============================\n",
                 msa[position].afd_alias);
   (void)fprintf(stdout, "          (Number of AFD's: %d   Position: %d   MSA ID: %d)\n\n",
                 no_of_afds, position, msa_id);
   (void)fprintf(stdout, "Remote work dir    : %s\n", msa[position].r_work_dir);
   (void)fprintf(stdout, "Remote AFD version : %s\n", msa[position].afd_version);
   (void)fprintf(stdout, "Remote command     : %s\n", msa[position].rcmd);
   (void)fprintf(stdout, "Remote options     : %d =>", msa[position].options);
   if (msa[position].options == 0)
   {
      (void)fprintf(stdout, " None");
   }
   else
   {
      if (msa[position].options & COMPRESS_FLAG)
      {
         (void)fprintf(stdout, " COMPRESS");
      }
      if (msa[position].options & MINUS_Y_FLAG)
      {
         (void)fprintf(stdout, " MINUS_Y");
      }
      if (msa[position].options & DONT_USE_FULL_PATH_FLAG)
      {
         (void)fprintf(stdout, " DONT_USE_FULL_PATH");
      }
   }
   (void)fprintf(stdout, "\n");
   if (msa[position].afd_switching != NO_SWITCHING)
   {
      (void)fprintf(stdout, "Real hostname 0    : %s\n", msa[position].hostname[0]);
      (void)fprintf(stdout, "TCP port 0         : %d\n", msa[position].port[0]);
      (void)fprintf(stdout, "Real hostname 1    : %s\n", msa[position].hostname[1]);
      (void)fprintf(stdout, "TCP port 1         : %d\n", msa[position].port[1]);
      (void)fprintf(stdout, "Current host       : AFD %d\n", msa[position].afd_toggle);
      (void)fprintf(stdout, "Switch type        : %s\n", (msa[position].afd_switching == AUTO_SWITCHING) ? "Auto" : "User");
   }
   else
   {
      (void)fprintf(stdout, "Real hostname      : %s\n", msa[position].hostname[0]);
      (void)fprintf(stdout, "TCP port           : %d\n", msa[position].port[0]);
      (void)fprintf(stdout, "Switch type        : No switching.\n");
   }
   (void)fprintf(stdout, "Poll interval      : %d\n", msa[position].poll_interval);
   (void)fprintf(stdout, "Connect time       : %d\n", msa[position].connect_time);
   (void)fprintf(stdout, "Disconnect time    : %d\n", msa[position].disconnect_time);
   (void)fprintf(stdout, "Status of AMG      : %d\n", (int)msa[position].amg);
   (void)fprintf(stdout, "Status of FD       : %d\n", (int)msa[position].fd);
   (void)fprintf(stdout, "Status of AW       : %d\n", (int)msa[position].archive_watch);
   (void)fprintf(stdout, "Jobs in queue      : %d\n", msa[position].jobs_in_queue);
   (void)fprintf(stdout, "Active transfers   : %d\n", msa[position].no_of_transfers);
   (void)fprintf(stdout, "TOP no. process    : %d", msa[position].top_no_of_transfers[0]);
   for (i = 1; i < STORAGE_TIME; i++)
   {
      (void)fprintf(stdout, " %d", msa[position].top_no_of_transfers[i]);
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Last TOP no process: %s", ctime(&msa[position].top_not_time));
   (void)fprintf(stdout, "Maximum connections: %d\n", msa[position].max_connections);
   (void)fprintf(stdout, "Sys log EC         : %u  |", msa[position].sys_log_ec);
   for (i = 0; i < LOG_FIFO_SIZE; i++)
   {
      switch (msa[position].sys_log_fifo[i])
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
   (void)fprintf(stdout, " |\n");
   (void)fprintf(stdout, "Receive History    :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (msa[position].log_history[RECEIVE_HISTORY][i])
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
   (void)fprintf(stdout, "System History     :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (msa[position].log_history[SYSTEM_HISTORY][i])
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
   (void)fprintf(stdout, "Transfer History   :");
   for (i = 0; i < MAX_LOG_HISTORY; i++)
   {
      switch (msa[position].log_history[TRANSFER_HISTORY][i])
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
   (void)fprintf(stdout, "Host error counter : %d\n", msa[position].host_error_counter);
   (void)fprintf(stdout, "Number of hosts    : %d\n", msa[position].no_of_hosts);
   (void)fprintf(stdout, "Number of jobs     : %d\n", msa[position].no_of_jobs);
   (void)fprintf(stdout, "fc                 : %u\n", msa[position].fc);
   (void)fprintf(stdout, "fs                 : %u\n", msa[position].fs);
   (void)fprintf(stdout, "tr                 : %u\n", msa[position].tr);
   (void)fprintf(stdout, "TOP tr             : %u", msa[position].top_tr[0]);
   for (i = 1; i < STORAGE_TIME; i++)
   {
      (void)fprintf(stdout, " %u", msa[position].top_tr[i]);
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Last TOP tr time   : %s", ctime(&msa[position].top_tr_time));
   (void)fprintf(stdout, "fr                 : %u\n", msa[position].fr);
   (void)fprintf(stdout, "TOP fr             : %u", msa[position].top_fr[0]);
   for (i = 1; i < STORAGE_TIME; i++)
   {
      (void)fprintf(stdout, " %u", msa[position].top_fr[i]);
   }
   (void)fprintf(stdout, "\n");
   (void)fprintf(stdout, "Last TOP fr time   : %s", ctime(&msa[position].top_fr_time));
   (void)fprintf(stdout, "ec                 : %u\n", msa[position].ec);
   (void)fprintf(stdout, "Last data time     : %s", ctime(&msa[position].last_data_time));
   switch (msa[position].connect_status)
   {
      case CONNECTION_ESTABLISHED :
         (void)fprintf(stdout, "Connect status     : CONNECTION_ESTABLISHED\n");
         break;
      case CONNECTION_DEFUNCT :
         (void)fprintf(stdout, "Connect status     : CONNECTION_DEFUNCT\n");
         break;
      case DISCONNECTED :
         (void)fprintf(stdout, "Connect status     : DISCONNECTED\n");
         break;
      case DISABLED : /* This AFD is disabled, ie should not be monitored. */
         (void)fprintf(stdout, "Connect status     : DISABLED\n");
         break;
      default : /* Should not get here. */
         (void)fprintf(stdout, "Connect status     : Unknown\n");
   }
   if (msa[position].convert_username[0][0] != '\0')
   {
      (void)fprintf(stdout, "Convert user name  : %s -> %s\n",
                    msa[position].convert_username[0][0],
                    msa[position].convert_username[0][1]);
      for (i = 1; i < MAX_CONVERT_USERNAME; i++)
      {
         (void)fprintf(stdout, "                   : %s -> %s\n",
                       msa[position].convert_username[i][0],
                       msa[position].convert_username[i][1]);
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : msa_view [--version][-w <working directory>] afdname|position\n");
   return;
}
