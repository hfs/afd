/*
 *  job_list_spy.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   job_list_spy - shows all jobs that are held by the AFD
 **
 ** SYNOPSIS
 **   job_list_spy [-w <AFD work dir>] [--version]
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
 **   15.05.1999 H.Kiehl Option to show only one job.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* atoi()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "version.h"

/* Global variables */
int sys_log_fd = STDERR_FILENO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                 fd,
                       i,
                       j,
                       *no_of_job_ids,
                       search_id = -1;
   char                file[MAX_PATH_LENGTH],
                       *ptr,
                       work_dir[MAX_PATH_LENGTH];
   struct stat         stat_buf;
   struct job_id_data *jd;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   if (argc == 2)
   {
      search_id = atoi(argv[1]);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
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
   no_of_job_ids = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jd = (struct job_id_data *)ptr;

   if (*no_of_job_ids > 0)
   {
      for (i = 0; i < *no_of_job_ids; i++)
      {
         if ((search_id == -1) || (search_id == jd[i].job_id))
         {
            (void)fprintf(stdout, "Job-ID      Dir-ID      P Hostalias\n");
            (void)fprintf(stdout, "%-11d %-11d %c %-*s\n",
                          jd[i].job_id,
                          jd[i].dir_id_pos,
                          jd[i].priority,
                          MAX_HOSTNAME_LENGTH, jd[i].host_alias);
            (void)fprintf(stdout, "Number of file filters: %d\n",
                          jd[i].no_of_files);
            for (j = 0; j < jd[i].no_of_files; j++)
            {
               (void)fprintf(stdout, "                        %s\n", jd[i].file_list[j]);
            }
            (void)fprintf(stdout, "Number of local options: %d\n",
                          jd[i].no_of_loptions);
            if (jd[i].no_of_loptions > 0)
            {
               (void)fprintf(stdout, "%s\n", jd[i].loptions);
            }
            (void)fprintf(stdout, "Number of standart options: %d\n",
                          jd[i].no_of_soptions);
            if (jd[i].no_of_soptions > 0)
            {
               (void)fprintf(stdout, "%s\n", jd[i].soptions);
            }
            (void)fprintf(stdout, "RECIPIENT: %s\n\n", jd[i].recipient);
         }
      }
   }
   else
   {
      (void)fprintf(stdout, "Job ID list is empty.\n");
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
