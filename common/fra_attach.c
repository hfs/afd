/*
 *  fra_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_attach - attaches to the FRA
 **
 ** SYNOPSIS
 **   int fra_attach(void)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FRA (File Retrieve
 **   Status Area). The first 4 Bytes of this area contains the number
 **   of host in the FRA. The rest consist of the structure
 **   fileretrieve_status for each host. When the function returns
 **   successful it returns the pointer 'fra' to the begining of the
 **   first structure.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FRA successful and sets the
 **   global pointer 'fra' to the start of the FRA. Also the FRA ID,
 **   the size of the FRA and the number of host in the FRA is returned
 **   in 'fra_id', 'fra_size' and 'no_of_dirs' respectively. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.01.2000 H.Kiehl Created
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

/* Global variables */
extern int                        sys_log_fd,
                                  fra_fd,
                                  fra_id,
                                  no_of_dirs;
#ifndef _NO_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ fra_attach() #############################*/
int
fra_attach(void)
{
   int          fd,
                loop_counter,
                retries = 0;
   char         *ptr = NULL,
                *p_fra_stat_file,
                fra_id_file[MAX_PATH_LENGTH],
                fra_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat  stat_buf;

   /* Get absolute path of FRA_ID_FILE */
   (void)strcpy(fra_id_file, p_work_dir);
   (void)strcat(fra_id_file, FIFO_DIR);
   (void)strcpy(fra_stat_file, fra_id_file);
   (void)strcat(fra_stat_file, FRA_STAT_FILE);
   p_fra_stat_file = fra_stat_file + strlen(fra_stat_file);
   (void)strcat(fra_id_file, FRA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_dirs is stale.                    */
      if ((no_of_dirs < 0) && (fra != NULL))
      {
         /* Unmap from FRA */
#ifdef _NO_MMAP
         if (munmap_emu((void *)fra) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      fra_stat_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            fra = NULL;
         }
#else
         (void)sprintf(p_fra_stat_file, ".%d", fra_id);

         if (stat(fra_stat_file, &stat_buf) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      fra_stat_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (munmap((void *)fra, stat_buf.st_size) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to munmap() %s : %s (%s %d)\n",
                         fra_stat_file, strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               fra = NULL;
            }
         }
#endif

         /* No need to speed things up here */
         my_usleep(800000L);
      }

      /*
       * Retrieve the FRA (Fileretrieve Status Area) ID from FRA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(fra_id_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            my_usleep(800000L);
            if (loop_counter++ > 12)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         fra_id_file, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      fra_id_file, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
      }

      /* Check if its locked */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not set write lock for %s : %s (%s %d)\n",
                   fra_id_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }

      /* Read the fra_id */
      if (read(fd, &fra_id, sizeof(int)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not read the value of the fra_id : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }

      /* Unlock file and close it */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not unlock %s : %s (%s %d)\n",
                   fra_id_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not close() %s : %s (%s %d)\n",
                   fra_id_file, strerror(errno), __FILE__, __LINE__);
      }

      (void)sprintf(p_fra_stat_file, ".%d", fra_id);

      if (fra_fd > 0)
      {
         if (close(fra_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }

      if ((fra_fd = coe_open(fra_stat_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         fra_stat_file, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
            else
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         fra_stat_file, strerror(errno), __FILE__, __LINE__);
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      fra_stat_file, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
      }

      if (fstat(fra_fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   fra_stat_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

#ifdef _NO_MMAP
      if ((ptr = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, fra_stat_file, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fra_fd, 0)) == (caddr_t) -1)
#endif
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "mmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      /* Read number of current FRA */
      no_of_dirs = *(int *)ptr;
   } while (no_of_dirs <= 0);

   ptr += AFD_WORD_OFFSET;
   fra = (struct fileretrieve_status *)ptr;
#ifndef _NO_MMAP
   fra_size = stat_buf.st_size;
#endif

   return(SUCCESS);
}
