/*
 *  cache_spy.c - Part of AFD, an automatic file distribution program.
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
 **   cache_spy - shows all messages currently being cached by the FD
 **
 ** SYNOPSIS
 **   cache_spy [-w <AFD work dir>] [--version]
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int sys_log_fd = STDERR_FILENO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  fd,
                        i,
                        *no_msg_cached;
   char                 file[MAX_PATH_LENGTH],
                        *ptr,
                        work_dir[MAX_PATH_LENGTH];
   struct stat          stat_buf;
   struct msg_cache_buf *mb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, MSG_CACHE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mb = (struct msg_cache_buf *)ptr;

   if (*no_msg_cached > 0)
   {
#ifdef _AGE_LIMIT
      (void)fprintf(stdout, "Hostname FSA-pos Job-ID      msg-time    last-trans  Age-limit Type in FSA\n");
      for (i = 0; i < *no_msg_cached; i++)
      {
         (void)fprintf(stdout, "%-*s %-7d %-11u %-11lu %-11lu %-9u %-4d %-4d\n",
                       MAX_HOSTNAME_LENGTH, mb[i].host_name,
                       mb[i].fsa_pos,
                       mb[i].job_id,
                       mb[i].msg_time,
                       mb[i].last_transfer_time,
                       mb[i].age_limit,
                       (int)mb[i].type,
                       (int)mb[i].in_current_fsa);
      }
#else
      (void)fprintf(stdout, "Hostname FSA-pos Job-ID      msg-time    last-trans  Type in FSA\n");
      for (i = 0; i < *no_msg_cached; i++)
      {
         (void)fprintf(stdout, "%-*s %-7d %-11u %-11u %-11ul %-4d %-4d\n",
                       MAX_HOSTNAME_LENGTH, mb[i].host_name,
                       mb[i].fsa_pos,
                       mb[i].job_id,
                       mb[i].msg_time,
                       mb[i].last_transfer_time,
                       (int)mb[i].type,
                       (int)mb[i].in_current_fsa);
      }
#endif /* _AGE_LIMIT */
   }
   else
   {
      (void)fprintf(stdout, "No messages cached.\n");
   }

   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to munmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
