/*
 *  remove_connection.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001, 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   remove_connection - resets data in connection and FSA structures
 **
 ** SYNOPSIS
 **   void remove_connection(struct connection *p_con,
 **                          int               faulty,
 **                          time_t            now)
 **
 ** DESCRIPTION
 **   This function resets all necessary values in the connection and
 **   FSA structure after a job has been removed.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.10.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include "fddefs.h"

/* #define WITH_MULTI_FSA_CHECKS */

/* External global variables */
extern int                        sys_log_fd,
                                  fsa_fd;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;


/*######################### remove_connection() #########################*/
void
remove_connection(struct connection *p_con, int faulty, time_t now)
{
   if (p_con->fsa_pos != -1)
   {
      /*
       * Reset some values of FSA structure. But before we do so it's
       * essential that we do NOT write to an old FSA! So lets check if we
       * are still in the correct FSA. Otherwise we subtract the number of
       * active transfers without ever resetting the pid! This can lead to
       * some very fatal behaviour of the AFD.
       */
#ifdef WITH_MULTI_FSA_CHECKS
      if (fd_check_fsa() == YES)
      {
         if (check_fra_fd() == YES)
         {
            init_fra_data();
         }
         get_new_positions();
         init_msg_buffer();
      }
#endif /* WITH_MULTI_FSA_CHECKS */

      /* Decrease number of active transfers to this host in FSA */
      if (faulty == YES)
      {
         fsa[p_con->fsa_pos].last_retry_time = now;
         lock_region_w(fsa_fd, (char *)&fsa[p_con->fsa_pos].error_counter - (char *)fsa);
         fsa[p_con->fsa_pos].error_counter += 1;
         fsa[p_con->fsa_pos].total_errors += 1;
         unlock_region(fsa_fd, (char *)&fsa[p_con->fsa_pos].error_counter - (char *)fsa);

         /* Check if we need to toggle hosts */
         if (fsa[p_con->fsa_pos].auto_toggle == ON)
         {
            if ((fsa[p_con->fsa_pos].error_counter == fsa[p_con->fsa_pos].max_errors) &&
                (fsa[p_con->fsa_pos].original_toggle_pos == NONE))
            {
               fsa[p_con->fsa_pos].original_toggle_pos = fsa[p_con->fsa_pos].host_toggle;
            }
            if ((fsa[p_con->fsa_pos].error_counter % fsa[p_con->fsa_pos].max_errors) == 0)
            {
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Automatic host switch initiated for host %s\n",
                         fsa[p_con->fsa_pos].host_dsp_name);
               if (fsa[p_con->fsa_pos].host_toggle == HOST_ONE)
               {
                  fsa[p_con->fsa_pos].host_toggle = HOST_TWO;
               }
               else
               {
                  fsa[p_con->fsa_pos].host_toggle = HOST_ONE;
               }
               fsa[p_con->fsa_pos].host_dsp_name[(int)fsa[p_con->fsa_pos].toggle_pos] = fsa[p_con->fsa_pos].host_toggle_str[(int)fsa[p_con->fsa_pos].host_toggle];
            }
         }
      }
      else
      {
         if ((faulty != NEITHER) &&
             (fsa[p_con->fsa_pos].error_counter > 0) &&
             (p_con->temp_toggle == OFF))
         {
            int j;

            lock_region_w(fsa_fd, (char *)&fsa[p_con->fsa_pos].error_counter - (char *)fsa);
            fsa[p_con->fsa_pos].error_counter = 0;

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa[p_con->fsa_pos].allowed_transfers; j++)
            {
               if (fsa[p_con->fsa_pos].job_status[j].connect_status == NOT_WORKING)
               {
                  fsa[p_con->fsa_pos].job_status[j].connect_status = DISCONNECT;
               }
            }
            unlock_region(fsa_fd, (char *)&fsa[p_con->fsa_pos].error_counter - (char *)fsa);
         }
      }

      if (fsa[p_con->fsa_pos].active_transfers > fsa[p_con->fsa_pos].allowed_transfers)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Active transfers > allowed transfers %d!? [%d] (%s %d)\n",
                   fsa[p_con->fsa_pos].allowed_transfers,
                   fsa[p_con->fsa_pos].active_transfers,
                   __FILE__, __LINE__);
         fsa[p_con->fsa_pos].active_transfers = fsa[p_con->fsa_pos].allowed_transfers;
      }
      fsa[p_con->fsa_pos].active_transfers -= 1;
      if (fsa[p_con->fsa_pos].active_transfers < 0)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Active transfers for FSA position %d < 0!? [%d] (%s %d)\n",
                   p_con->fsa_pos, fsa[p_con->fsa_pos].active_transfers,
                   __FILE__, __LINE__);
         fsa[p_con->fsa_pos].active_transfers = 0;
      }
      if (fsa[p_con->fsa_pos].special_flag & ERROR_FILE_UNDER_PROCESS)
      {
         /* Error job is no longer under process. */
         fsa[p_con->fsa_pos].special_flag ^= ERROR_FILE_UNDER_PROCESS;
      }
      fsa[p_con->fsa_pos].job_status[p_con->job_no].proc_id = -1;
#ifdef _BURST_MODE
      fsa[p_con->fsa_pos].job_status[p_con->job_no].unique_name[0] = '\0';
      fsa[p_con->fsa_pos].job_status[p_con->job_no].burst_counter = 0;
#endif
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
      fsa[p_con->fsa_pos].job_status[p_con->job_no].job_id = NO_ID;
#endif
   }

   /* Decrease the number of active transfers. */
   if (p_afd_status->no_of_transfers > 0)
   {
      p_afd_status->no_of_transfers--;
   }
   else
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Huh?! Whats this trying to reduce number of transfers although its zero??? (%s %d)\n",
                __FILE__, __LINE__);
   }

   /*
    * Reset all values of connection structure.
    */
   p_con->hostname[0] = '\0';
   p_con->job_no = -1;
   p_con->fsa_pos = -1;
   p_con->fra_pos = -1;
   p_con->msg_name[0] = '\0';
   p_con->pid = 0;

   return;
}
