/*
 *  start_all.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   start_all - start all monitor process
 **
 ** SYNOPSIS
 **   void start_all(void)
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
 **   30.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* strerror()                             */
#include <stdlib.h>            /* malloc(), realloc()                    */
#include <time.h>              /* time()                                 */
#include <errno.h>
#include "mondefs.h"

/* External global variables */
extern int                    no_of_afds,
                              sys_log_fd;
extern size_t                 proc_list_size;
extern struct mon_status_area *msa;
extern struct process_list    *pl;


/*############################# start_all() #############################*/
void
start_all(void)
{
   int    i;
   size_t new_size = no_of_afds * sizeof(struct process_list);

   if (new_size > proc_list_size)
   {
      if (pl == NULL)
      {
         if ((pl = malloc(new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "malloc() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         if ((pl = realloc(pl, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "realloc() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      proc_list_size = new_size;
   }

   for (i = 0; i < no_of_afds; i++)
   {
      if (msa[i].connect_status == DISABLED)
      {
         pl[i].pid = 0;
         pl[i].start_time = 0L;
         pl[i].afd_alias[0] = '\0';
      }
      else
      {
         if ((pl[i].pid = start_process("mon", i)) != INCORRECT)
         {
            pl[i].start_time = time(NULL);
            pl[i].number_of_restarts = 0;
            (void)strcpy(pl[i].afd_alias, msa[i].afd_alias);
         }
      }
   }

   return;
}
