/*
 *  check_permissions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_permissions - checks the file access permissions of AFD
 **
 ** SYNOPSIS
 **   void check_permissions(void)
 **
 ** DESCRIPTION
 **   The function check_permissions checks if all the file access
 **   permissions of important files in fifodir are set correctly.
 **   If not it will print a warning in the SYSTEM_LOG and correct
 **   it.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.04.2003 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy()                              */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), chmod()                       */
#include <errno.h>

struct check_list
       {
          char   *file_name;
          mode_t full_mode;
          mode_t mode;
       };

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;


/*######################### check_permissions() #########################*/
void
check_permissions(void)
{
   int               i;
   char              fullname[MAX_PATH_LENGTH],
                     *ptr;
   struct stat       stat_buf;
   struct check_list ckl[] =
          {
             { SYSTEM_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { RECEIVE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { TRANSFER_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { TRANS_DEBUG_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { MON_SYS_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { MON_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { AFD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { AFD_RESP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { AMG_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { DB_UPDATE_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { FD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { FD_RESP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { AW_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { IP_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { SF_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { RETRY_FD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { DELETE_JOBS_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { DELETE_JOBS_HOST_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { FD_WAKE_UP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { PROBE_ONLY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
#ifdef _INPUT_LOG
             { INPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
#endif
#ifdef _OUTPUT_LOG
             { OUTPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
#endif
#ifdef _DELETE_LOG
             { DELETE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
#endif
#ifdef _RENAME_LOG
             { RENAME_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
#endif
             { RETRY_MON_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { DEL_TIME_JOB_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { FD_READY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { DIR_NAME_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { JOB_ID_DATA_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
             { MSG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { CURRENT_MSG_LIST_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { AMG_DATA_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { STATUS_SHMID_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { COUNTER_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { FSA_ID_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { FRA_ID_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { JOB_ID_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { DIR_ID_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { MESSAGE_BUF_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
             { MSG_CACHE_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { MSG_QUEUE_FILE, S_IFREG | FILE_MODE, FILE_MODE },
             { NULL, 0, 0 }
          };

   ptr = fullname + sprintf(fullname, "%s%s", p_work_dir, FIFO_DIR);
   i = 0;
   do
   {
      (void)strcpy(ptr, ckl[i].file_name);
      if (stat(fullname, &stat_buf) == -1)
      {
         if (errno != ENOENT)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Can't access file %s : %s (%s %d)\n",
                      fullname, strerror(errno), __FILE__, __LINE__);
         }
      }
      else
      {
         if (stat_buf.st_mode != ckl[i].full_mode)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "File %s has mode %o, changing to %o. (%s %d)\n",
                      fullname, stat_buf.st_mode, ckl[i].full_mode,
                      __FILE__, __LINE__);
            if (chmod(fullname, ckl[i].mode) == -1)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Can't change mode to %o for file %s : %s (%s %d)\n",
                         ckl[i].mode, fullname, strerror(errno),
                         __FILE__, __LINE__);
            }
         }
      }
      i++;
   } while (ckl[i].file_name != NULL);

   return;
}
