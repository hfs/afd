/*
 *  get_position.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
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
 **   get_position - gets the position in the array which holds the host
 **
 ** SYNOPSIS
 **   int get_position(struct filetransfer_status *fsa,
 **                    char                       *host,
 **                    int                        no_of_hosts)
 **
 ** DESCRIPTION
 **   This function gets the position of host 'host' in the FSA
 **   (File Transfer Status Area) 'fsa'. The FSA consists of
 **   'no_of_hosts' structures, i.e. for each host a structure
 **   of filetransfer_status exists.
 **
 ** RETURN VALUES
 **   Returns the position of the host 'host' in the FSA pointed
 **   to by 'fsa'. If the host is not found in the FSA, INCORRECT
 **   is returned instead.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.11.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                 /* strcmp()                          */


/*########################## get_position() #############################*/
int
get_position(struct filetransfer_status *fsa,
             char                       *host,
             int                        no_of_hosts)
{
   static int position;

   for (position = 0; position < no_of_hosts; position++)
   {
      if (strcmp(fsa[position].host_alias, host) == 0)
      {
         return(position);
      }
   }

   /* Host not found in struct */
   return(INCORRECT);
}
