/*
 *  show_summary_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_summary_stat - writes the contents of a file to a socket
 **
 ** SYNOPSIS
 **   void show_summary_stat(FILE *p_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strerror()                               */
#include <unistd.h>          /* write()                                  */
#include <sys/types.h>
#include <sys/times.h>       /* times(), struct tms                      */
#include <errno.h>
#include "afdddefs.h"

/* External global variables */
extern int                        host_config_counter,
                                  no_of_hosts,
                                  sys_log_fd;
extern clock_t                    clktck;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;


/*######################### show_summary_stat() #########################*/
void
show_summary_stat(FILE *p_data)
{
   int            error_hosts = 0,
                  i,
                  j;
   unsigned int   files_to_be_send = 0,
                  total_errors = 0;
   off_t          bytes_to_be_send = 0;
   double         bytes_send = 0.0,
                  elapsed_time,
                  files_send = 0.0,
                  delta_bytes_send,
                  delta_files_send;
   static double  prev_bytes_send,
                  prev_files_send;
   struct tms     tmsdummy;
   static clock_t start_time;

   for (i = 0; i < no_of_hosts; i++)
   {
      files_to_be_send += fsa[i].total_file_counter;
      bytes_to_be_send += fsa[i].total_file_size;
      if ((fsa[i].protocol & LOC_FLAG) == 0)
      {
         for (j = 0; j < fsa[i].allowed_transfers; j++)
         {
            bytes_send += (double)fsa[i].job_status[j].bytes_send;
         }
      }
      files_send += (double)fsa[i].file_counter_done;
      if (fsa[i].error_counter >= fsa[i].max_errors)
      {
         error_hosts++;
      }
      else
      {
         total_errors += fsa[i].error_counter;
      }
   }
   if (prev_bytes_send > bytes_send)
   {
      /* Overflow! */
      delta_bytes_send = 0.0;
   }
   else
   {
      delta_bytes_send = bytes_send - prev_bytes_send;
   }
   if (prev_files_send > files_send)
   {
      /* Overflow! */
      delta_files_send = 0.0;
   }
   else
   {
      delta_files_send = files_send - prev_files_send;
   }

   (void)fprintf(p_data, "211- AFD status summary:\r\n");
   (void)fflush(p_data);

   /* Print data from structure afd_status. */
   if (start_time == 0)
   {
      elapsed_time = 1.0;
   }
   else
   {
      elapsed_time = (times(&tmsdummy) - start_time) / (double)clktck;
      if (elapsed_time < 0.00001)
      {
         elapsed_time = 1.0;
      }
   }
#if SIZEOF_OFF_T == 4
   (void)fprintf(p_data, "IS %u %ld %u %u %u %d %d %u\r\n",
#else
   (void)fprintf(p_data, "IS %u %lld %u %u %u %d %d %u\r\n",
#endif
                 files_to_be_send, bytes_to_be_send,
                 (unsigned int)(delta_bytes_send / elapsed_time),
                 (unsigned int)(delta_files_send / elapsed_time),
                 total_errors, error_hosts,
                 p_afd_status->no_of_transfers,
                 p_afd_status->jobs_in_queue);
   start_time = times(&tmsdummy);
   prev_bytes_send = bytes_send;
   prev_files_send = files_send;

   return;
}
