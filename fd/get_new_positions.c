/*
 *  get_new_positions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_new_positions - get new FSA and FRA positions for connection
 **                       structure
 **
 ** SYNOPSIS
 **   void get_new_positions(void)
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
 **   18.10.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables */
extern int                        max_connections,
                                  no_of_dirs,
                                  no_of_hosts,
                                  sys_log_fd;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct connection          *connection;


/*######################### get_new_positions() #########################*/
void
get_new_positions(void)
{
   register int i;

   for (i = 0; i < max_connections; i++)
   {
      if (connection[i].pid > 0)
      {
         if ((connection[i].fsa_pos = get_host_position(fsa,
                                                        connection[i].hostname,
                                                        no_of_hosts)) < 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmm. Failed to locate host <%s> for connection job %d [pid = %d] has been removed. Writing data to end of FSA 8-( (%s %d)\n",
                      connection[i].hostname, i, connection[i].pid,
                      __FILE__, __LINE__);
            connection[i].fsa_pos = no_of_hosts;
         }
         if (connection[i].msg_name[0] == '\0')
         {
            if ((connection[i].fra_pos = get_dir_position(fra,
                                                          connection[i].dir_alias,
                                                          no_of_dirs)) < 0)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Hmm. Failed to locate dir_alias %s for connection job %d [pid = %d] has been removed. Writing data to end of FRA 8-( (%s %d)\n",
                         connection[i].dir_alias, i, connection[i].pid,
                         __FILE__, __LINE__);
               connection[i].fra_pos = no_of_dirs;
            }
         }
      }
   }

   return;
}
