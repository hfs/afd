/*
 *  send_message.c - Part of AFD, an automatic file distribution program.
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
 **   send_message - sends a message via MSG_FIFO to the FD
 **
 ** SYNOPSIS
 **   void send_message(void)
 **
 ** DESCRIPTION
 **   The function send_message() sends a message of the following
 **   format to the FD:
 **     <creation time><job ID><unique number><Priority>
 **            |           |          |           |
 **            |           |          |           +-> char
 **            |           |          +-------------> unsigned short
 **            |           +------------------------> int
 **            +------------------------------------> time_t
 **   But only when the FD is currently active. If not these messages
 **   get stored in the buffer 'mb'.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* exit()                                   */
#include <unistd.h>          /* write()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WITH_PTHREAD                                                            
#include <pthread.h>                                                            
#endif                                                                          
#include <errno.h>                                                              
#include "amgdefs.h"                                                            

/* External global variables. */
extern int                        fsa_fd,
                                  mb_fd,
                                  msg_fifo_fd,
                                  *no_msg_buffered,
                                  sys_log_fd;
extern char                       *p_work_dir;
extern size_t                     msg_fifo_buf_size;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;
extern struct instant_db          *db;
extern struct message_buf         *mb;
#ifdef _WITH_PTHREAD
extern pthread_mutex_t            fsa_mutex;
#else
# ifdef _DELETE_LOG
extern off_t                      *file_size_pool;
extern char                       **file_name_pool;
# endif /* _DELETE_LOG */
#endif
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototypes. */
static void                       store_msg(char *);


/*############################ send_message() ###########################*/
void
send_message(char           *afd_file_dir,
             char           *p_unique_name,
             unsigned short unique_number,
             time_t         creation_time,
             int            position,
#ifdef _DELETE_LOG
#ifdef _WITH_PTHREAD
             off_t          *file_size_pool,
             char           **file_name_pool,
#endif
#endif
             int            files_to_send,
             off_t          file_size_to_send)
{
   char fifo_buffer[MAX_BIN_MSG_LENGTH],
        file_path[MAX_PATH_LENGTH];

   (void)strcpy(file_path, afd_file_dir);
   (void)strcat(file_path, p_unique_name);

   if (db[position].lfs & DELETE_ALL_FILES)
   {
#ifdef _DELETE_LOG
      int    i;
      size_t dl_real_size;

      for (i = 0; i < files_to_send; i++)
      {
         (void)strcpy(dl.file_name, file_name_pool[i]);
         (void)sprintf(dl.host_name, "%-*s %x",
                       MAX_HOSTNAME_LENGTH, db[position].host_alias, OTHER_DEL);
         *dl.file_size = file_size_pool[i];
         *dl.job_number = db[position].job_id;
         *dl.file_name_length = strlen(file_name_pool[i]);
         dl_real_size = *dl.file_name_length + dl.size +
                        sprintf((dl.file_name + *dl.file_name_length + 1),
                                "%s delete option (%s %d)",
                                DIR_CHECK, __FILE__, __LINE__);
         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
         {
            (void)rec(sys_log_fd, ERROR_SIGN, "write() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }                                           
      }
#endif /* _DELETE_LOG */
      files_to_send = 0;
   }
   else
   {
      if (db[position].no_of_loptions > 0)
      {
         if (handle_options(db[position].no_of_loptions, db[position].loptions,
                            file_path, &files_to_send, &file_size_to_send) != 0)
         {
            /* The function reported the error, so */
            /* no need to do it here again.        */
            return;
         }
      }
   }

   if (files_to_send > 0)
   {
#ifdef _WITH_PTHREAD
      int rtn;

      if ((rtn = pthread_mutex_lock(&fsa_mutex)) != 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "pthread_mutex_lock() error : %s (%s %d)\n",
                   strerror(rtn), __FILE__, __LINE__);
      }
#endif

      /* Total file counter */
      lock_region_w(fsa_fd, (char *)&fsa[db[position].position].total_file_counter - (char *)fsa);
      fsa[db[position].position].total_file_counter += files_to_send;

      /* Total file size */
      fsa[db[position].position].total_file_size += file_size_to_send;
      unlock_region(fsa_fd, (char *)&fsa[db[position].position].total_file_counter - (char *)fsa);

#ifdef _WITH_PTHREAD
      if ((rtn = pthread_mutex_unlock(&fsa_mutex)) != 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "pthread_mutex_unlock() error : %s (%s %d)\n",
                   strerror(rtn), __FILE__, __LINE__);
      }
#endif

#ifdef _BURST_MODE
      /*
       * When a connection is currently open to the remote site
       * just add the files to the directory where files are
       * currently transmitted. This we may only do under the
       * following conditions:
       *
       *     - This is an FTP (or WMO when enabled) job.
       *     - All connections for this host are in use.
       *     - The job ID's are the same.
       *     - The number of jobs that are queued for bursting
       *       may not be larger then 256.
       *
       * NOTE: I am not checking if an error has occurred.
       *       All that happens in such a situation is that a job
       *       gets larger and the next time we try to connect
       *       this and the additional job will connect only once.
       */
      if (check_burst((char)db[position].protocol,
                      db[position].position,
                      db[position].job_id,
                      file_path, afd_file_dir, YES) != YES)
      {
#endif
         int offset = sizeof(time_t);

         if (sizeof(int) > offset)
         {
            offset = sizeof(int);
         }

         *(time_t *)(fifo_buffer) = creation_time;
         *(int *)(fifo_buffer + offset) = db[position].job_id;
         *(unsigned short *)(fifo_buffer + offset + offset) = unique_number;
         *(char *)(fifo_buffer + offset + offset + sizeof(unsigned short)) = db[position].priority;

         /*
          * Send the message name via fifo to the FD. If the
          * FD is not active queue it in a special buffer.
          * When sending a message always make sure that this
          * buffer is empty.
          */
         if (p_afd_status->fd == ON)
         {
            /* Check if there are any messages in the queue. */
            if (*no_msg_buffered > 0)
            {
               clear_msg_buffer();
            }

            if (write(msg_fifo_fd, fifo_buffer, msg_fifo_buf_size) != msg_fifo_buf_size)
            {
               store_msg(fifo_buffer);
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to write() to message FIFO : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         else /* Queue the message! */
         {
            store_msg(fifo_buffer);
         }
#ifdef _BURST_MODE
      }
#endif
   } /* if (files_to_send > 0) */
   else
   {
      /* A directory has already been created. Lets remove it. */
      if (rec_rmdir(file_path) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to remove directory %s (%s %d)\n",
                   file_path, __FILE__, __LINE__);
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ store_msg() +++++++++++++++++++++++++++++*/
static void
store_msg(char *msg)
{
   lock_region_w(mb_fd, (off_t)0);
   if ((*no_msg_buffered != 0) &&
       ((*no_msg_buffered % MESSAGE_BUF_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_msg_buffered / MESSAGE_BUF_STEP_SIZE) + 1) *
                        MESSAGE_BUF_STEP_SIZE * sizeof(struct message_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)mb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(mb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "mmap_resize() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_msg_buffered = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      mb = (struct message_buf *)ptr;
   }
   (void)memcpy(&mb[*no_msg_buffered], msg, MAX_BIN_MSG_LENGTH);
   (*no_msg_buffered)++;
   unlock_region(mb_fd, 0);

   return;
}
