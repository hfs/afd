/*
 *  msa_attach.c - Part of AFD, an automatic file distribution program.
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
 **   msa_attach - attaches to the MSA
 **
 ** SYNOPSIS
 **   int msa_attach(void)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the MSA (Monitor Status
 **   Area). The first 8 Bytes of this area contains the number of
 **   AFD's that are being monitored. The rest consist of the
 **   structure mon_status_area for each AFD. When the function
 **   returns successful it returns the pointer 'msa' to the begining
 **   of the first structure.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the MSA successful and sets the
 **   global pointer 'msa' to the start of the MSA and the size of
 **   the memory mapped file is returned. Also the MSA ID and the
 **   number of AFD's in the MSA is returned in 'msa_id' and
 **   'no_of_afds' respectively. Otherwise INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror(), strlen()         */
#include <unistd.h>                      /* close(), read()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>                    /* mmap(), munmap()             */
#endif
#include <fcntl.h>                       /* fcntl()                      */
#include <errno.h>
#include "mondefs.h"

/* Global variables */
extern int                    sys_log_fd,
                              msa_fd,
                              msa_id,
                              no_of_afds;
#ifndef _NO_MMAP
extern off_t                  msa_size;
#endif
extern char                   *p_work_dir;
extern struct mon_status_area *msa;


/*############################ msa_attach() #############################*/
int
msa_attach(void)
{
   int          fd,
                loop_counter,
                retries = 0;
   char         *ptr = NULL,
                *p_msa_stat_file,
                msa_id_file[MAX_PATH_LENGTH],
                msa_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat  stat_buf;

   /* Get absolute path of MSA_ID_FILE */
   (void)strcpy(msa_id_file, p_work_dir);
   (void)strcat(msa_id_file, FIFO_DIR);
   (void)strcpy(msa_stat_file, msa_id_file);
   (void)strcat(msa_stat_file, MON_STATUS_FILE);
   p_msa_stat_file = msa_stat_file + strlen(msa_stat_file);
   (void)strcat(msa_id_file, MSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_afds is stale.                    */
      if (no_of_afds < 0)
      {
         /* Unmap from MSA */
#ifdef _NO_MMAP
         if (munmap_emu((void *)msa) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      msa_stat_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            msa = NULL;
         }
#else
         (void)sprintf(p_msa_stat_file, ".%d", msa_id);

         if (stat(msa_stat_file, &stat_buf) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      msa_stat_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (munmap((void *)msa, stat_buf.st_size) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to munmap() %s : %s (%s %d)\n",
                         msa_stat_file, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               msa = NULL;
            }
         }
#endif

         /* No need to speed things up here */
         my_usleep(800000L);
      }

      /*
       * Retrieve the MSA (Monitor Status Area) ID from MSA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(msa_id_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            my_usleep(800000L);
            if (loop_counter++ > 12)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         msa_id_file, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      msa_id_file, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
      }

      /* Check if its locked */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not set write lock for %s : %s (%s %d)\n",
                   msa_id_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }

      /* Read the msa_id */
      if (read(fd, &msa_id, sizeof(int)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not read the value of the msa_id : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }

      /* Unlock file and close it */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not unlock %s : %s (%s %d)\n",
                   msa_id_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not close() %s : %s (%s %d)\n",
                   msa_id_file, strerror(errno), __FILE__, __LINE__);
      }

      (void)sprintf(p_msa_stat_file, ".%d", msa_id);

      if (msa_fd > 0)
      {
         if (close(msa_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }

      if ((msa_fd = coe_open(msa_stat_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         msa_stat_file, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
            else
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         msa_stat_file, strerror(errno), __FILE__, __LINE__);
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      msa_stat_file, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
      }

      if (fstat(msa_fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   msa_stat_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

#ifdef _NO_MMAP
      if ((ptr = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, msa_stat_file, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, msa_fd, 0)) == (caddr_t) -1)
#endif
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "mmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      /* Read number of AFD's */
      no_of_afds = *(int *)ptr;
   } while (no_of_afds <= 0);

   ptr += AFD_WORD_OFFSET;
   msa = (struct mon_status_area *)ptr;
#ifndef _NO_MMAP
   msa_size = stat_buf.st_size;
#endif

   return(SUCCESS);
}
