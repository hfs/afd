/*
 *  queue_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   queue_spy - shows the messages queued by the FD
 **
 ** SYNOPSIS
 **   queue_spy [-w <AFD work dir>] [--version]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int  sys_log_fd = STDERR_FILENO;
char *p_work_dir = NULL;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              fd,
                    i,
                    *no_msg_queued;
   char             file[MAX_PATH_LENGTH],
                    *ptr,
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct queue_buf *qb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, MSG_QUEUE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_queued = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   qb = (struct queue_buf *)ptr;

   if (*no_msg_queued > 0)
   {
      (void)fprintf(stdout, "Message number   Message name                      Pid time         Pos  CP E\n");
      for (i = 0; i < *no_msg_queued; i++)
      {
         (void)fprintf(stdout, "%-16.0f %-*s %6d %-10ld %5d %3d %d\n",
                       qb[i].msg_number, MAX_MSG_NAME_LENGTH,
                       qb[i].msg_name, qb[i].pid,
                       qb[i].creation_time, qb[i].pos,
                       qb[i].connect_pos, qb[i].in_error_dir);
      }
   }
   else
   {
      (void)fprintf(stdout, "No messages queued.\n");
   }

   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr, "Failed to munmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
