/*
 *  init_msg_buffer.c - Part of AFD, an automatic file distribution program.
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
 **   init_msg_buffer - 
 **
 ** SYNOPSIS
 **   void init_msg_buffer(void)
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
 **   22.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>       /* strcpy(), strcat(), strerror()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                mb_fd,
                          msg_fifo_fd,
                          *no_msg_buffered,
                          sys_log_fd;
extern char               *p_work_dir;
extern struct message_buf *mb;


/*########################## init_msg_buffer() ##########################*/
void
init_msg_buffer(void)
{
   char        *ptr,
               message_buf_file[MAX_PATH_LENGTH],
               msg_fifo[MAX_PATH_LENGTH];
   size_t      new_size;
   struct stat stat_buf;

   (void)strcpy(message_buf_file, p_work_dir);
   (void)strcat(message_buf_file, FIFO_DIR);
   (void)strcpy(msg_fifo, message_buf_file);
   (void)strcat(msg_fifo, MSG_FIFO);
   (void)strcat(message_buf_file, MESSAGE_BUF_FILE);

   /* Attach to message buffer. */
   new_size = (MESSAGE_BUF_STEP_SIZE * sizeof(struct message_buf)) +
              AFD_WORD_OFFSET;
   if ((ptr = attach_buf(message_buf_file, &mb_fd, new_size, NULL)) == (caddr_t) -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to mmap() to %s : %s (%s %d)\n",
                message_buf_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_buffered = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mb = (struct message_buf *)ptr;

   if ((stat(msg_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(msg_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create fifo %s. (%s %d)\n",
                   msg_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((msg_fifo_fd = open(msg_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                msg_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
