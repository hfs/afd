/*
 *  fsa_attach.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fsa_attach - attaches to the FSA
 **
 ** SYNOPSIS
 **   int fsa_attach(void)
 **
 ** DESCRIPTION
 **   Attaches to the memory mapped area of the FSA (File Transfer
 **   Status Area). The first 4 Bytes of this area contains the number
 **   of host in the FSA. The rest consist of the structure
 **   filetransfer_status for each host. When the function returns
 **   successful it returns the pointer 'fsa' to the begining of the
 **   first structure.
 **
 ** RETURN VALUES
 **   SUCCESS when attaching to the FSA successful and sets the
 **   global pointer 'fsa' to the start of the FSA. Also the FSA ID,
 **   the size of the FSA and the number of host in the FSA is returned
 **   in 'fsa_id', 'fsa_size' and 'no_of_hosts' respectively. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.12.1995 H.Kiehl Created
 **   15.08.1997 H.Kiehl When failing to open() the FSA file don't just
 **                      exit. Give it another try.
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
extern int                        fsa_fd,
                                  fsa_id,
                                  no_of_hosts;
#ifndef _NO_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;


/*############################ fsa_attach() #############################*/
int
fsa_attach(void)
{
   int          fd,
                loop_counter,
                retries = 0,
                timeout_loops = 0;
   char         *ptr = NULL,
                *p_fsa_stat_file,
                fsa_id_file[MAX_PATH_LENGTH],
                fsa_stat_file[MAX_PATH_LENGTH];
   struct flock wlock = {F_WRLCK, SEEK_SET, 0, 1},
                ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat  stat_buf;

   /* Get absolute path of FSA_ID_FILE */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(fsa_stat_file, fsa_id_file);
   (void)strcat(fsa_stat_file, FSA_STAT_FILE);
   p_fsa_stat_file = fsa_stat_file + strlen(fsa_stat_file);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   do
   {
      /* Make sure this is not the case when the */
      /* no_of_hosts is stale.                   */
      if ((no_of_hosts < 0) && (fsa != NULL))
      {
         /* Unmap from FSA */
         ptr = (char *)fsa - AFD_WORD_OFFSET;
#ifdef _NO_MMAP
         if (munmap_emu((void *)fsa) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       fsa_stat_file, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }
#else
         if (munmap(ptr, fsa_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       fsa_stat_file, strerror(errno));
         }
         else
         {
            fsa = NULL;
         }
#endif

         timeout_loops++;
         if (timeout_loops > 100)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to attach to a new FSA.");
            return(INCORRECT);
         }

         /* No need to speed things up here */
         my_usleep(800000L);
      }

      /*
       * Retrieve the FSA (Filetransfer Status Area) ID from FSA_ID_FILE.
       * Make sure that it is not locked.
       */
      loop_counter = 0;
      while ((fd = coe_open(fsa_id_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            my_usleep(800000L);
            if (loop_counter++ > 12)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          fsa_id_file, strerror(errno));
               return(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       fsa_id_file, strerror(errno));
            return(INCORRECT);
         }
      }

      /* Check if its locked */
      if (fcntl(fd, F_SETLKW, &wlock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not set write lock for %s : %s",
                    fsa_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /* Read the fsa_id */
      if (read(fd, &fsa_id, sizeof(int)) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not read the value of the fsa_id : %s",
                    strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }

      /* Unlock file and close it */
      if (fcntl(fd, F_SETLKW, &ulock) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not unlock %s : %s", fsa_id_file, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not close() %s : %s", fsa_id_file, strerror(errno));
      }

      (void)sprintf(p_fsa_stat_file, ".%d", fsa_id);

      if (fsa_fd > 0)
      {
         if (close(fsa_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }

      if ((fsa_fd = coe_open(fsa_stat_file, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (retries++ > 8)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          fsa_stat_file, strerror(errno));
               return(INCORRECT);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          fsa_stat_file, strerror(errno));
               (void)sleep(1);
               continue;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       fsa_stat_file, strerror(errno));
            return(INCORRECT);
         }
      }

      if (fstat(fsa_fd, &stat_buf) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to stat() %s : %s",
                    fsa_stat_file, strerror(errno));
         return(INCORRECT);
      }

#ifdef _NO_MMAP
      if ((ptr = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, fsa_stat_file, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fsa_fd, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "mmap() error : %s", strerror(errno));
         return(INCORRECT);
      }

      /* Read number of current FSA */
      no_of_hosts = *(int *)ptr;

      ptr += AFD_WORD_OFFSET;
      fsa = (struct filetransfer_status *)ptr;
#ifndef _NO_MMAP
      fsa_size = stat_buf.st_size;
#endif
   } while (no_of_hosts <= 0);

   return(SUCCESS);
}
