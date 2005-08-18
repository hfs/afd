/*
 *  check_changes.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   03.09.2000 H.Kiehl Addition of log history.
 **   26.06.2004 H.Kiehl Added error history for each host.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strerror()                               */
#include <time.h>            /* time()                                   */
#include <stdlib.h>          /* atoi()                                   */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>          /* F_OK                                     */
#include <errno.h>
#include "afdddefs.h"

/* External global variables */
extern int                        host_config_counter,
                                  no_of_hosts;
extern char                       afd_config_file[],
                                  current_msg_list_file[];
extern unsigned char              **old_error_history;
extern struct afd_status          *p_afd_status;
extern struct filetransfer_status *fsa;


/*############################ check_changes() ##########################*/
void
check_changes(FILE *p_data)
{
   static int      old_amg_status,
                   old_archive_watch_status,
                   old_fd_status,
                   old_max_connections,
                   old_no_of_jobs;
   static unsigned int old_sys_log_ec;
   static time_t   next_stat_time,
                   old_st_mtime;
   static char     old_receive_log_history[MAX_LOG_HISTORY],
                   old_sys_log_history[MAX_LOG_HISTORY],
                   old_trans_log_history[MAX_LOG_HISTORY];
   register int    i;
   time_t          now;

   if (check_fsa(NO) == YES)
   {
      int fd,
          no_of_jobs = -1;

      if ((fd = open(current_msg_list_file, O_RDONLY)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                   "Failed to open() `%s' : %s",
                   current_msg_list_file, strerror(errno));
      }
      else
      {
         if (read(fd, &no_of_jobs, sizeof(int)) != sizeof(int))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to read() %d bytes from `%s' : %s",
                      sizeof(int), current_msg_list_file, strerror(errno));
            no_of_jobs = -1;
         }
         if (close(fd) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to close() `%s' : %s",
                      current_msg_list_file, strerror(errno));
         }
      }
      FREE_RT_ARRAY(old_error_history);
      RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
               unsigned char);
      for (i = 0; i < no_of_hosts; i++)
      {
         (void)memcpy(old_error_history[i], fsa[i].error_history,
                      ERROR_HISTORY_LENGTH);
      }
      host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);
      show_host_list(p_data);
      if (old_no_of_jobs != no_of_jobs)
      {
         (void)fprintf(p_data, "NJ %u\r\n", no_of_jobs);
         old_no_of_jobs = no_of_jobs;
      }
   }
   else
   {
      if (host_config_counter != (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT))
      {
         FREE_RT_ARRAY(old_error_history);
         RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH,
                  unsigned char);
         for (i = 0; i < no_of_hosts; i++)
         {
            (void)memcpy(old_error_history[i], fsa[i].error_history,
                         ERROR_HISTORY_LENGTH);
         }
         host_config_counter = (int)*(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT);
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
            if ((eaccess(afd_config_file, F_OK) == 0) &&
                (read_file(afd_config_file, &buffer) != INCORRECT))
            {
               int  max_connections = 0;
               char value[MAX_INT_LENGTH];

               if (get_definition(buffer,
                                  MAX_CONNECTIONS_DEF,
                                  value, MAX_INT_LENGTH) != NULL)
               {
                  max_connections = atoi(value);
               }
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
               free(buffer);
            }
         }
      }
      else
      {
         if (errno != ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                      "Failed to stat() `%s' : %s",
                      afd_config_file, strerror(errno));
         }
      }
   }

   if (old_sys_log_ec != p_afd_status->sys_log_ec)
   {
      char buf[LOG_FIFO_SIZE + 1];

      old_sys_log_ec = p_afd_status->sys_log_ec;
      for (i = 0; i < LOG_FIFO_SIZE; i++)
      {
         buf[i] = p_afd_status->sys_log_fifo[i] + ' ';
      }
      buf[i] = '\0';
      (void)fprintf(p_data, "SR %u %s\r\n", old_sys_log_ec, buf);
   }

   if (memcmp(old_receive_log_history, p_afd_status->receive_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_receive_log_history, p_afd_status->receive_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->receive_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)fprintf(p_data, "RH %s\r\n", buf);
   }
   if (memcmp(old_sys_log_history, p_afd_status->sys_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_sys_log_history, p_afd_status->sys_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->sys_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)fprintf(p_data, "SH %s\r\n", buf);
   }
   if (memcmp(old_trans_log_history, p_afd_status->trans_log_history,
              MAX_LOG_HISTORY) != 0)
   {
      char buf[MAX_LOG_HISTORY + 1];

      (void)memcpy(old_trans_log_history, p_afd_status->trans_log_history,
                   MAX_LOG_HISTORY);
      for (i = 0; i < MAX_LOG_HISTORY; i++)
      {
         buf[i] = p_afd_status->trans_log_history[i] + ' ';
      }
      buf[i] = '\0';
      (void)fprintf(p_data, "TH %s\r\n", buf);
   }
   for (i = 0; i < no_of_hosts; i++)
   {
      if (memcmp(old_error_history[i], fsa[i].error_history,
                 ERROR_HISTORY_LENGTH) != 0)
      {
         int k;

         (void)memcpy(old_error_history[i], fsa[i].error_history,
                      ERROR_HISTORY_LENGTH);
         (void)fprintf(p_data, "EL %d %d", i, old_error_history[i][0]);
         for (k = 1; k < ERROR_HISTORY_LENGTH; k++)
         {
            (void)fprintf(p_data, " %d", old_error_history[i][k]);
         }
         (void)fprintf(p_data, "\r\n");
      }
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
