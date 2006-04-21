/*
 *  send_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void send_message(char           *outgoing_file_dir,
 **                     char           *p_unique_name,
 **                     unsigned int   split_job_counter,
 **                     unsigned short unique_number,
 **                     time_t         creation_time,
 **                     int            position,
 **                     int            files_to_send,
 **                     off_t          file_size_to_send)
 **
 ** DESCRIPTION
 **   The function send_message() sends a message of the following
 **   format to the FD:
 **   <creation time><JID><SJC><dir no><unique number><Priority><Originator>
 **          |         |    |      |         |           |       |
 **          |         |    |      |         |           | +-----+
 **          |         |    |      |         |           | |
 **          |         |    |      |         |           | |
 **          |         |    |      |         |           | |
 **          |         |    |      |         |           | +-> char
 **          |         |    |      |         |           +---> char
 **          |         |    |      |         +---------------> unsigned short
 **          |         |    |      +-------------------------> unsigned short
 **          |         |    +--------------------------------> unsigned int
 **          |         +-------------------------------------> unsigned int
 **          +-----------------------------------------------> time_t
 **
 **   JID - Job ID
 **   SJC - Split Job Counter
 **
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
 **   24.09.2004 H.Kiehl Added split job counter.
 **
 */
DESCR__E_M3

#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* exit(), strtoul()                        */
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
                                  *no_msg_buffered;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;
extern struct instant_db          *db;
extern struct message_buf         *mb;
#ifdef _WITH_PTHREAD
extern pthread_mutex_t            fsa_mutex;
#else
extern char                       *file_name_buffer;
# ifdef _DELETE_LOG
extern off_t                      *file_size_pool;
extern char                       **file_name_pool;
# endif
#endif
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif

/* Local function prototypes. */
static void                       store_msg(char *);


/*############################ send_message() ###########################*/
void
send_message(char           *outgoing_file_dir,
             char           *p_unique_name,
             unsigned int   split_job_counter,
             unsigned short unique_number,
             time_t         creation_time,
             int            position,
#ifdef _WITH_PTHREAD
             char           *file_name_buffer,
# ifdef _DELETE_LOG
             off_t          *file_size_pool,
             char           **file_name_pool,
# endif
#endif
             int            files_to_send,
             off_t          file_size_to_send)
{
   char file_path[MAX_PATH_LENGTH];

   (void)strcpy(file_path, outgoing_file_dir);
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
                                "%s delete option", DIR_CHECK);
         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
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
                            db[position].job_id,
#ifdef _DELETE_LOG
                            db[position].host_alias,
#endif
#ifdef _PRODUCTION_LOG
                            creation_time, unique_number, split_job_counter,
#endif
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
      off_t          lock_offset;
      char           fifo_buffer[MAX_BIN_MSG_LENGTH],
                     *ptr;
      unsigned short dir_no;
#ifdef _WITH_PTHREAD
      int            rtn;
#endif

      ptr = p_unique_name + 1;
      while ((*ptr != '/') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr != '/')
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Unable to find directory number in `%s'", p_unique_name);
         return;
      }
      dir_no = (unsigned short)strtoul(ptr + 1, NULL, 16);

#ifdef _WITH_PTHREAD
      if ((rtn = pthread_mutex_lock(&fsa_mutex)) != 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "pthread_mutex_lock() error : %s", strerror(rtn));
      }
#endif

      lock_offset = AFD_WORD_OFFSET +
                    (db[position].position * sizeof(struct filetransfer_status));

      /* Total file counter */
#ifdef LOCK_DEBUG
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
      fsa[db[position].position].total_file_counter += files_to_send;

      /* Total file size */
      fsa[db[position].position].total_file_size += file_size_to_send;
#ifdef LOCK_DEBUG
      unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
      unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif

#ifdef _WITH_PTHREAD
      if ((rtn = pthread_mutex_unlock(&fsa_mutex)) != 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "pthread_mutex_unlock() error : %s", strerror(rtn));
      }
#endif
      *(time_t *)(fifo_buffer) = creation_time;
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = db[position].job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                 sizeof(unsigned int) +
                 sizeof(unsigned int)) = file_size_to_send;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(off_t)) = dir_no;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(off_t) +
                          sizeof(unsigned short)) = unique_number;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned short) +
                sizeof(unsigned short)) = db[position].priority;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned short) +
                sizeof(unsigned short) + sizeof(char)) = AMG_NO;

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

         if (write(msg_fifo_fd, fifo_buffer, MAX_BIN_MSG_LENGTH) != MAX_BIN_MSG_LENGTH)
         {
            store_msg(fifo_buffer);
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to write() to message FIFO : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
      }
      else /* Queue the message! */
      {
         store_msg(fifo_buffer);
      }
   } /* if (files_to_send > 0) */
   else
   {
      if (db[position].loptions != NULL)
      {
         /* A directory has already been created. Lets remove it. */
         if (rec_rmdir(file_path) < 0)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to remove directory %s", file_path);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ store_msg() +++++++++++++++++++++++++++++*/
static void
store_msg(char *msg)
{
#ifdef LOCK_DEBUG
   lock_region_w(mb_fd, (off_t)0, __FILE__, __LINE__);
#else
   lock_region_w(mb_fd, (off_t)0);
#endif
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
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_msg_buffered = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      mb = (struct message_buf *)ptr;
   }
   (void)memcpy(&mb[*no_msg_buffered], msg, MAX_BIN_MSG_LENGTH);
   (*no_msg_buffered)++;
   if ((p_afd_status->amg_jobs & DIR_CHECK_MSG_QUEUED) == 0)
   {
      p_afd_status->amg_jobs |= DIR_CHECK_MSG_QUEUED;
   }
#ifdef LOCK_DEBUG
   unlock_region(mb_fd, 0, __FILE__, __LINE__);
#else
   unlock_region(mb_fd, 0);
#endif

   return;
}
