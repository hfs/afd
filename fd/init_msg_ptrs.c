/*
 *  init_msg_ptrs.c - Part of AFD, an automatic file distribution program.
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
 **   init_msg_ptrs - initialises and sets data pointers for message
 **                   pointers
 **
 ** SYNOPSIS
 **   void init_msg_ptrs(size_t         *msg_fifo_buf_size,
 **                      time_t         **creation_time,
 **                      int            **job_id,
 **                      unsigned short **unique_number,
 **                      char           **msg_priority,
 **                      char           **fifo_buffer)
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
 **   18.04.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                  /* strerror()                       */
#include <stdlib.h>                  /* malloc()                         */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int sys_log_fd;


/*########################### init_msg_ptrs() ###########################*/
void
init_msg_ptrs(size_t         *msg_fifo_buf_size,
              time_t         **creation_time,
              int            **job_id,
              unsigned short **unique_number,
              char           **msg_priority,
              char           **fifo_buffer)
{
   int offset = sizeof(time_t);

   if (sizeof(int) > offset)
   {
      offset = sizeof(int);
   }
   *msg_fifo_buf_size = offset + offset  + sizeof(unsigned short) + 1;
   if ((*fifo_buffer = malloc(*msg_fifo_buf_size)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   *creation_time = (time_t *)*fifo_buffer;
   *job_id = (int *)(*fifo_buffer + offset);
   *unique_number = (unsigned short *)(*fifo_buffer + offset + offset);
   *msg_priority = (char *)(*fifo_buffer + offset + offset + sizeof(unsigned short));

   return;
}
