/*
 *  make_fifo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   make_fifo - creates a new fifo
 **
 ** SYNOPSIS
 **   int make_fifo(char *fifoname)
 **
 ** DESCRIPTION
 **   The function make_fifo() creates the fifo 'fifoname' with read
 **   and write permissions of the owner.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when the fifo was created. Otherwise it returns
 **   INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.12.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                      /* strerror()                   */
#include <sys/types.h>
#include <sys/stat.h>                    /* mkfifo()                     */
#include <errno.h>

extern int sys_log_fd;


/*############################ make_fifo() ##############################*/
int
make_fifo(char *fifoname)
{
   /* Create new fifo */
   if (mkfifo(fifoname, S_IRUSR | S_IWUSR) == -1)
   {
      if (errno != EEXIST)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not create fifo %s : %s (%s %d)\n",
                   fifoname, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }

   return(SUCCESS);
}
