/*
 *  reset_fsa.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   reset_fsa - resets certain values in the FSA
 **
 ** SYNOPSIS
 **   void reset_fsa(struct job *p_db, int faulty, int mode)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* Global variables */
extern int                        no_of_hosts,
                                  fsa_id;
#ifndef _NO_MMAP
extern off_t                      fsa_size;
#endif
extern char                       host_deleted,
                                  *p_work_dir;
extern struct filetransfer_status *fsa;


/*############################# reset_fsa() #############################*/
void
reset_fsa(struct job *p_db, int mode)
{
   if ((fsa != NULL) && (host_deleted == NO))
   {
      if (check_fsa() == YES)
      {
         if ((p_db->fsa_pos = get_host_position(fsa, p_db->host_alias,
                                                no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
      }
      if (host_deleted == NO)
      {
         if (mode & IS_FAULTY_VAR)
         {
            fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].connect_status = NOT_WORKING;
         }
         else
         {
            fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].connect_status = DISCONNECT;
         }
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].no_of_files_done = 0;
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].file_size_done = 0;
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].file_size_in_use = 0;
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].file_size_in_use_done = 0;
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].file_name_in_use[0] = '\0';
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].no_of_files = 0;
         fsa[p_db->fsa_pos].job_status[(int)p_db->job_no].file_size = 0;
      }
   }

   return;
}
