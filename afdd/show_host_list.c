/*
 *  show_host_list.c - Part of AFD, an automatic file distribution program.
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
 **   show_host_list - handles a TCP request
 **
 ** SYNOPSIS
 **   void show_host_list(FILE *p_data)
 **
 ** DESCRIPTION
 **   This function prints a list of all host currently being served
 **   by this AFD in the following format:
 **     HL <host_number> <host alias> <real hostname 1> [<real hostname 2>]
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.06.1999 H.Kiehl Created
 */
DESCR__E_M3

#include <stdio.h>
#include "afdddefs.h"

/* External global variables */
extern int                        sys_log_fd;
extern int                        no_of_hosts;
extern struct filetransfer_status *fsa;


/*########################### show_host_list() ##########################*/
void
show_host_list(FILE *p_data)
{
   int i;

   (void)fprintf(p_data, "211- AFD host list:\r\n");
   (void)fflush(p_data);

   (void)fprintf(p_data, "NH %d\r\n", no_of_hosts);
   (void)fflush(p_data);

   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[1][0] == '\0')
      {
         (void)fprintf(p_data, "HL %d %s %s\r\n", i, fsa[i].host_alias,
                       fsa[i].real_hostname[0]);
      }
      else
      {
         (void)fprintf(p_data, "HL %d %s %s %s\r\n", i, fsa[i].host_alias,
                       fsa[i].real_hostname[0], fsa[i].real_hostname[1]);
      }
      (void)fflush(p_data);
   }

   return;
}
