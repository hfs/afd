/*
 *  show_shm.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   print_shm - prints contents of the shared memory region to a file
 **
 ** SYNOPSIS
 **   print_shm <output filename>
 **
 ** DESCRIPTION
 **   The program print_shm is used for debugging only. It shows
 **   the contents the shared memory region used by AMG and dir_check.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.01.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf(), fopen(), fclose()    */
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>                 /* mmap(), munmap()                */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                   /* STDERR_FILENO                   */
#include <errno.h>
#include "amgdefs.h"

int   shm_id,                      /* The shared memory ID's for     */
                                   /* the sorted data and pointers.  */
      data_length = -1,            /* The size of data for each      */
                                   /* main sorted data and pointers. */
      sys_log_fd = STDERR_FILENO;
char *p_work_dir;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  afd_active_fd;
   char afd_active_file[MAX_PATH_LENGTH],
        work_dir[MAX_PATH_LENGTH];
   FILE *fp;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <output filename>\n", argv[1]);
      exit(1);
   }

   /*
    * First lets get the shm_id from the afd_active file.
    */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(1);
   }
   (void)sprintf(afd_active_file, "%s%s%s",
                 work_dir, FIFO_DIR, AFD_ACTIVE_FILE);
   if ((afd_active_fd = open(afd_active_file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    afd_active_file, strerror(errno));
      exit(1);
   }
   else
   {
      char        *pid_list;
      struct stat stat_buf;

      if (fstat(afd_active_fd, &stat_buf) < 0)
      {
         (void)fprintf(stderr, "Failed to fstat() %s : %s\n",
                       afd_active_file, strerror(errno));
         exit(1);
      }
      if ((pid_list = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                           MAP_SHARED, afd_active_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                       afd_active_file, strerror(errno));
         exit(1);
      }
      if (close(afd_active_fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() %s : %s\n",
                       afd_active_file, strerror(errno));
      }
      shm_id = *(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t)));
      if (munmap((void *)pid_list, stat_buf.st_size) == -1)
      {
         (void)fprintf(stderr, "Failed to munmap() %s : %s\n",
                       afd_active_file, strerror(errno));
      }
   }
   if ((fp = fopen(argv[1], "w")) == NULL)
   {
      (void)fprintf(stderr, "Failed to fopen() %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }
   show_shm(fp);
   fclose(fp);

   exit(0);
}
