/*
 *  check_changes.c - Part of AFD, an automatic file distribution program.
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
 **   check_changes - 
 **
 ** SYNOPSIS
 **   void check_changes(FILE *p_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strerror()                               */
#include <time.h>            /* time()                                   */
#include <stdlib.h>          /* atoi()                                   */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* access()                                 */
#include <errno.h>
#include "afdddefs.h"

/* External global variables */
extern int                        host_config_counter,
                                  sys_log_fd;
extern char                       afd_config_file[];
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;


/*############################ check_changes() ##########################*/
void
check_changes(FILE *p_data)
{
   static int      old_amg_status,
                   old_archive_watch_status,
                   old_fd_status,
                   old_max_connections;
   static unsigned int old_sys_log_ec;
   static time_t   next_stat_time,
                   old_st_mtime;
   time_t          now;

   if (check_fsa() == YES)
   {
      host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + sizeof(int));
      show_host_list(p_data);
   }
   else
   {
      if (host_config_counter != (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + sizeof(int)))
      {
         host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + sizeof(int));
         show_host_list(p_data);
      }
   }

   /*
    * It costs too much system performance to constantly stat()
    * the AFD_CONFIG file to see if the modification time has
    * changed. For this reason lets only stat() this file at a
    * reasonable interval of say STAT_INTERVAL seconds.
    */
   now = time(NULL);
   if (next_stat_time < now)
   {
      struct stat stat_buf;

      next_stat_time = now + STAT_INTERVAL;
      if (stat(afd_config_file, &stat_buf) == 0)
      {
         if (stat_buf.st_mtime != old_st_mtime)
         {
            char *buffer;

            old_st_mtime = stat_buf.st_mtime;
            if ((access(afd_config_file, F_OK) == 0) &&
                (read_file(afd_config_file, &buffer) != INCORRECT))
            {
               char value[MAX_INT_LENGTH];

               if (get_definition(buffer,
                                  MAX_CONNECTIONS_DEF,
                                  value, MAX_INT_LENGTH) != NULL)
               {
                  int max_connections;

                  max_connections = atoi(value);
                  if ((max_connections < 1) ||
                      (max_connections > MAX_CONFIGURABLE_CONNECTIONS))
                  {
                     max_connections = MAX_DEFAULT_CONNECTIONS;
                  }
                  if (max_connections != old_max_connections)
                  {
                     old_max_connections = max_connections;
                     (void)fprintf(p_data, "MC %d\r\n", old_max_connections);
                  }
               }
               free(buffer);
            }
         }
      }
      else
      {
         if (errno != ENOENT)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      afd_config_file, strerror(errno),
                      __FILE__, __LINE__);
         }
      }
   }

   if (old_sys_log_ec != p_afd_status->sys_log_ec)
   {
      int i;

      old_sys_log_ec = p_afd_status->sys_log_ec;
      (void)fprintf(p_data, "SR %u", old_sys_log_ec);
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         (void)fprintf(p_data, " %d", (int)p_afd_status->sys_log_fifo[i]);
      }
      (void)fprintf(p_data, "\r\n");
   }

   /*
    * Check if status of any of the main process (AMG, FD and
    * archive_watch) have changed.
    */
   if (old_amg_status != p_afd_status->amg)
   {
      old_amg_status = p_afd_status->amg;
      (void)fprintf(p_data, "AM %d\r\n", old_amg_status);
   }
   if (old_fd_status != p_afd_status->fd)
   {
      old_fd_status = p_afd_status->fd;
      (void)fprintf(p_data, "FD %d\r\n", old_fd_status);
   }
   if (old_archive_watch_status != p_afd_status->archive_watch)
   {
      old_archive_watch_status = p_afd_status->archive_watch;
      (void)fprintf(p_data, "AW %d\r\n", old_archive_watch_status);
   }

   (void)fflush(p_data);

   return;
}
