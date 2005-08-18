/*
 *  remove_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2000 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   remove_msg - removes a message from the internal queue of the FD
 **
 ** SYNOPSIS
 **   void remove_msg(int qb_pos)
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
 **   27.07.1998 H.Kiehl Created
 **   11.08.2000 H.Kiehl Support for retrieving files.
 **
 */
DESCR__E_M3

#include <string.h>
#include <time.h>
#include "fddefs.h"

/* External global variables */
extern int                        *no_msg_queued;
extern struct queue_buf           *qb;
extern struct fileretrieve_status *fra;


/*############################ remove_msg() #############################*/
void
remove_msg(int qb_pos)
{
   if (qb[qb_pos].msg_name[0] == '\0')
   {
      /* Dequeue in FRA. */
      fra[qb[qb_pos].pos].queued = NO;

      /* Calculate the next scan time. */
      if (fra[qb[qb_pos].pos].time_option == YES)
      {
         fra[qb[qb_pos].pos].next_check_time = calc_next_time(&fra[qb[qb_pos].pos].te, time(NULL));
      }
   }
   if (qb_pos != (*no_msg_queued - 1))
   {
      size_t move_size;

      move_size = (*no_msg_queued - 1 - qb_pos) * sizeof(struct queue_buf);
      (void)memmove(&qb[qb_pos], &qb[qb_pos + 1], move_size);
   }
   (*no_msg_queued)--;

   return;
}
