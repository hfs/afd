/*
 *  map_instant_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   map_instant_db - uses mmap() to map and create structure instant_db
 **
 ** SYNOPSIS
 **   int map_instant_db(size_t size)
 **
 ** DESCRIPTION
 **   This function will create a new INSTANT_DB_FILE when size is
 **   greater then zero and the mmap to it. If size is not greater
 **   then zero it will mmap to the existing INSTANT_DB_FILE.
 **
 ** RETURN VALUES
 **   SUCCESS when it manages to create and map to a new instant
 **   database. Otherwise INCORRECT will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.03.1999 H.Kiehl Created
 **   04.04.2001 H.Kiehl Don't truncate file anymore.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strerror()                               */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>          /* lseek(), close(), write()                */
#include <errno.h>
#include "amgdefs.h"

#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif

/* External global variables */
extern int               sys_log_fd;
extern char              *p_work_dir;
extern struct instant_db *db;


/*########################## map_instant_db() ###########################*/
int
map_instant_db(size_t size)
{
   int  fd;
   char instant_db_file[MAX_PATH_LENGTH],
        *ptr;

   (void)sprintf(instant_db_file, "%s%s%s",
                 p_work_dir, FIFO_DIR, INSTANT_DB_FILE);

   if (size > 0)
   {
      struct stat stat_buf;

      if ((fd = open(instant_db_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      if (stat_buf.st_size > size)
      {
         if (ftruncate(fd, size) == -1)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to ftruncate() %s : %s (%s %d)\n",
                      instant_db_file, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (stat_buf.st_size < size)
      {
         int  i,
              loops,
              rest;
         char buffer[4096];

         loops = (size - stat_buf.st_size) / 4096;
         rest = (size - stat_buf.st_size) % 4096;

         if (stat_buf.st_size > 0)
         {
            if (lseek(fd, stat_buf.st_size, SEEK_SET) == -1)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to lseek() %s : %s (%s %d)\n",
                         instant_db_file, strerror(errno), __FILE__, __LINE__);
               (void)close(fd);
               return(INCORRECT);
            }
         }
         for (i = 0; i < loops; i++)
         {
            if (write(fd, buffer, 4096) != 4096)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to write() to %s : %s (%s %d)\n",
                         instant_db_file, strerror(errno), __FILE__, __LINE__);
               (void)close(fd);
               return(INCORRECT);
            }
         }
         if (rest > 0)
         {
            if (write(fd, buffer, rest) != rest)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to write() to %s : %s (%s %d)\n",
                         instant_db_file, strerror(errno), __FILE__, __LINE__);
               (void)close(fd);
               return(INCORRECT);
            }
         }
      }
      if ((ptr = mmap(0, size, (PROT_READ | PROT_WRITE),
                      (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      (void)memset(ptr, 0, size);
   }
   else
   {
      struct stat stat_buf;

      if ((fd = open(instant_db_file, O_RDWR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      size = stat_buf.st_size;
      if ((ptr = mmap(0, size, (PROT_READ | PROT_WRITE),
                      (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   instant_db_file, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
   }
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   db = (struct instant_db *)ptr;

   return(SUCCESS);
}
