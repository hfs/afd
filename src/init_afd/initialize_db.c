/*
 *  initialize_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   initialize_db - initializes the AFD database
 **
 ** SYNOPSIS
 **   void initialize_db(int full_init)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.10.2011 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* sprintf(), fprintf()                  */
#include <string.h>             /* strcpy()                              */
#include <unistd.h>             /* unlink()                              */
#include <errno.h>
#include "amgdefs.h"
#include "logdefs.h"
#include "statdefs.h"

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local function prototypes. */
static void delete_fifodir_files(char *, int),
            delete_log_files(char *, int);


/*########################## initialize_db() ############################*/
void
initialize_db(int full_init)
{
   int  offset;
   char dirs[MAX_PATH_LENGTH];

   offset = sprintf(dirs, "%s%s", p_work_dir, FIFO_DIR);
   delete_fifodir_files(dirs, offset);
   (void)sprintf(dirs, "%s%s", p_work_dir, AFD_MSG_DIR);
   if (rec_rmdir(dirs) == INCORRECT)
   {
      (void)fprintf(stderr,
                     _("WARNING : Failed to delete everything in %s.\n"),
                     dirs);
   }
#ifdef WITH_DUP_CHECK
   (void)sprintf(dirs, "%s%s%s", p_work_dir, AFD_FILE_DIR,
                 CRC_DIR);
   if (rec_rmdir(dirs) == INCORRECT)
   {
      (void)fprintf(stderr,
                    _("WARNING : Failed to delete everything in %s.\n"),
                    dirs);
   }
#endif
   (void)sprintf(dirs, "%s%s%s%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, FILE_MASK_DIR);
   if (rec_rmdir(dirs) == INCORRECT)
   {
      (void)fprintf(stderr,
                    _("WARNING : Failed to delete everything in %s.\n"),
                    dirs);
   }
   (void)sprintf(dirs, "%s%s%s%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, LS_DATA_DIR);
   if (rec_rmdir(dirs) == INCORRECT)
   {
      (void)fprintf(stderr,
                    _("WARNING : Failed to delete everything in %s.\n"),
                    dirs);
   }
   if (full_init == YES)
   {
      (void)sprintf(dirs, "%s%s", p_work_dir, AFD_FILE_DIR);
      if (rec_rmdir(dirs) == INCORRECT)
      {
         (void)fprintf(stderr,
                       _("WARNING : Failed to delete everything in %s.\n"),
                       dirs);
      }
      (void)sprintf(dirs, "%s%s", p_work_dir, AFD_ARCHIVE_DIR);
      if (rec_rmdir(dirs) == INCORRECT)
      {
         (void)fprintf(stderr,
                       _("WARNING : Failed to delete everything in %s.\n"),
                       dirs);
      }
      offset = sprintf(dirs, "%s%s", p_work_dir, LOG_DIR);
      delete_log_files(dirs, offset);
   }

   return;
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
        {
           FSA_ID_FILE,
           FRA_ID_FILE,
           STATUS_SHMID_FILE,
           BLOCK_FILE,
           AMG_COUNTER_FILE,
           COUNTER_FILE,
           MESSAGE_BUF_FILE,
           MSG_CACHE_FILE,
           MSG_QUEUE_FILE,
#ifdef WITH_ERROR_QUEUE
           ERROR_QUEUE_FILE,
#endif
           FILE_MASK_FILE,
           DC_LIST_FILE,
           DIR_NAME_FILE,
           JOB_ID_DATA_FILE,
           DCPL_FILE_NAME,
           PWB_DATA_FILE,
           CURRENT_MSG_LIST_FILE,
           AMG_DATA_FILE,
           AMG_DATA_FILE_TMP,
           LOCK_PROC_FILE,
           AFD_ACTIVE_FILE,
           WINDOW_ID_FILE,
           SYSTEM_LOG_FIFO,
           EVENT_LOG_FIFO,
           RECEIVE_LOG_FIFO,
           TRANSFER_LOG_FIFO,
           TRANS_DEBUG_LOG_FIFO,
           AFD_CMD_FIFO,
           AFD_RESP_FIFO,
           AMG_CMD_FIFO,
           DB_UPDATE_FIFO,
           FD_CMD_FIFO,
           AW_CMD_FIFO,
           IP_FIN_FIFO,
#ifdef WITH_ONETIME
           OT_FIN_FIFO,
#endif
           SF_FIN_FIFO,
           RETRY_FD_FIFO,
           FD_DELETE_FIFO,
           FD_WAKE_UP_FIFO,
           TRL_CALC_FIFO,
           QUEUE_LIST_READY_FIFO,
           QUEUE_LIST_DONE_FIFO,
           PROBE_ONLY_FIFO,
#ifdef _INPUT_LOG
           INPUT_LOG_FIFO,
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_LOG_FIFO,
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_LOG_FIFO,
#endif
#ifdef _DELETE_LOG
           DELETE_LOG_FIFO,
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_LOG_FIFO,
#endif
           DEL_TIME_JOB_FIFO,
           FD_READY_FIFO,
           MSG_FIFO,
           DC_CMD_FIFO, /* from amgdefs.h */
           DC_RESP_FIFO,
           AFDD_LOG_FIFO,
           TYPESIZE_DATA_FILE
        },
        *mfilelist[] =
        {
           FSA_STAT_FILE_ALL,
           FRA_STAT_FILE_ALL,
           ALTERNATE_FILE_ALL,
           DB_UPDATE_REPLY_FIFO_ALL
        };

   file_ptr = fifodir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      (void)strcpy(file_ptr, filelist[i]);
      (void)unlink(fifodir);
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      (void)remove_files(fifodir, &mfilelist[i][1]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*++++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
           "/DAEMON_LOG.init_afd",
           NEW_STATISTIC_FILE,
           NEW_ISTATISTIC_FILE
        },
        *mloglist[] =
        {
           SYSTEM_LOG_NAME_ALL,
           EVENT_LOG_NAME_ALL,
           RECEIVE_LOG_NAME_ALL,
           TRANSFER_LOG_NAME_ALL,
           TRANS_DB_LOG_NAME_ALL,
#ifdef _INPUT_LOG
           INPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_BUFFER_FILE_ALL,
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DELETE_LOG
           DELETE_BUFFER_FILE_ALL,
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_BUFFER_FILE_ALL,
#endif
           ISTATISTIC_FILE_ALL,
           STATISTIC_FILE_ALL
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      (void)unlink(logdir);
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      (void)remove_files(logdir, mloglist[i]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}
