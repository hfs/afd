/*
 *  check_fsa_entries.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fsa_entries - checks all FSA entries if they are still
 **                       correct
 **
 ** SYNOPSIS
 **   void check_fsa_entries(void)
 **
 ** DESCRIPTION
 **   The function check_fsa_entries() checks the file counter,
 **   file size, active transfers and error counter of all hosts
 **   in the FSA. This check is only performed when there are
 **   currently no message for the checked host in the message
 **   queue qb.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.06.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables */
extern int                        no_of_hosts,
                                  *no_msg_queued,
                                  sys_log_fd;
extern struct filetransfer_status *fsa;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;


/*########################## check_fsa_entries() ########################*/
void
check_fsa_entries(void)
{
   int gotcha,
       i,
       j;

   for (i = 0; i < no_of_hosts; i++)
   {
      gotcha = NO;
      for (j = 0; j < *no_msg_queued; j++)
      {
         if (mdb[qb[j].pos].fsa_pos == i)
         {
            gotcha = YES;
            break;
         }
      }

      /*
       * If there are currently no messages stored for this host
       * we can check if the values for file size, number of files,
       * number of active transfers and the error counter in the FSA
       * are still correct.
       */
      if (gotcha == NO)
      {
         if (fsa[i].active_transfers != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Active transfers for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name,
                      fsa[i].active_transfers,
                      __FILE__, __LINE__);
            fsa[i].active_transfers = 0;
         }
         if (fsa[i].total_file_counter != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "File counter for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name,
                      fsa[i].total_file_counter,
                      __FILE__, __LINE__);
            fsa[i].total_file_counter = 0;
         }
         if (fsa[i].total_file_size != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "File size for host %s is %lu. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name,
                      fsa[i].total_file_size,
                      __FILE__, __LINE__);
            fsa[i].total_file_size = 0;
         }
         if (fsa[i].error_counter != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Error counter for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name,
                      fsa[i].error_counter,
                      __FILE__, __LINE__);
            fsa[i].error_counter = 0;
         }
      } /* if (gotcha == NO) */
   } /* for (i = 0; i < no_of_hosts; i++) */

   return;
}
