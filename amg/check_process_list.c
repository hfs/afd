/*
 *  check_process_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_process_list - checks how many process have been forked for
 **                        this directory
 **
 ** SYNOPSIS
 **   int check_process_list(int dir_no)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns 1 when it may create another process for this directory,
 **   otherwise 0 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.12.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "amgdefs.h"

/* External global variables */
extern int                 no_of_process;
#ifdef _LIMIT_PROCESS_PER_DIR
extern int                 max_proc_per_dir;
extern struct dc_proc_list *dcpl;      /* Dir Check Process List */
#endif


/*######################### check_process_list() ########################*/
int
check_process_list(int dir_no)
{
#ifdef _LIMIT_PROCESS_PER_DIR
   int counter = 0,
       i;

   for (i = 0; i < no_of_process; i++)
   {
      if (dcpl[i].dir_no == dir_no)
      {
         counter++;
      }
   }
   if (counter < max_proc_per_dir)
   {
      return(1);
   }
#endif

   return(0);
}
