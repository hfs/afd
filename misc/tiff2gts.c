/*
 *  tiff2gts.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Deutscher Wetterdienst (DWD),
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
 **   tiff2gts - changes TIFF files to GTS files
 **
 ** SYNOPSIS
 **   int tiff2gts(char *path, char *filename)
 **         path     - path where the TIFF file can be found
 **         filename - file name of the TIFF file
 **
 ** DESCRIPTION
 **   Removes the TIFF-header and inserts a WMO-bulletin header.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to convert the file. Otherwise
 **   the size of the converted file is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.05.1996 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>              /* stat()                             */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>

#define OFFSET_START     8
#define OFFSET_END      12

/* External global variables */
extern int sys_log_fd;

/* Function prototypes */
static void byte_swap(char *);


/*############################# tiff2gts() ##############################*/
off_t
tiff2gts(char *path, char* filename)
{
   int          fd,
                data_start,
                data_end,
                byte_order = 1;
   static off_t data_size;
   char         *buf,
                dest_file_name[MAX_PATH_LENGTH],
                fullname[MAX_PATH_LENGTH];
   struct stat  stat_buf;

   (void)sprintf(fullname, "%s/%s", path, filename);

   if (stat(fullname, &stat_buf) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Can't access file %s : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (stat_buf.st_size <= (OFFSET_END + 4))
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Could not convert file %s. File does not have the correct length. (%s %d)\n",
                filename, __FILE__, __LINE__);
      return(INCORRECT);
   }

   if ((buf = (char *)malloc(stat_buf.st_size)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if ((fd = open(fullname, O_RDONLY, 0)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to open() %s : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
      free(buf);
      return(INCORRECT);
   }

   if (read(fd, buf, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "read() error : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
      free(buf);
      (void)close(fd);
      return(INCORRECT);
   }

   (void)close(fd);

   memcpy(&data_start, &buf[OFFSET_START], 4);
   memcpy(&data_end, &buf[OFFSET_END], 4);
   if (*(char *)&byte_order == 1)
   {
      byte_swap((char *)&data_start);
      byte_swap((char *)&data_end);
   }
   data_size = data_end - data_start + 1;
   if (data_size > stat_buf.st_size)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "File %s is corrupt. Data size (%d) larger then file size (%d). (%s %d)\n",
                filename, data_size, stat_buf.st_size, __FILE__, __LINE__);
      free(buf);
      return(INCORRECT);
   }
   else if (data_size <= 0)
        {
           (void)rec(sys_log_fd, ERROR_SIGN, "File %s is corrupt. Data size (%d) less then or equal to file size (%d). (%s %d)\n",
                     filename, data_size, stat_buf.st_size, __FILE__, __LINE__);
           free(buf);
           return(INCORRECT);
        }

   /*
    * Open destination file and copy data.
    */
   (void)sprintf(dest_file_name, "%s/.%s", path, filename);
   if ((fd = open(dest_file_name, (O_WRONLY | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to open() %s : %s (%s %d)\n",
                dest_file_name, strerror(errno), __FILE__, __LINE__);
      free(buf);
      return(INCORRECT);
   }

   if (write(fd, &buf[data_start], data_size) != data_size)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "write() error : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
      free(buf);
      (void)close(fd);
      return(INCORRECT);
   }

   free(buf);
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Remove the original file */
   if (remove(fullname) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to remove() original TIFF file %s : %s (%s %d)\n",
                fullname, strerror(errno), __FILE__, __LINE__);
   }

   /* Rename file to its new name */
   (void)sprintf(fullname, "%s/%s", path, filename);
   if (rename(dest_file_name, fullname) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to rename() file %s to %s : %s (%s %d)\n",
                dest_file_name, fullname, strerror(errno), __FILE__, __LINE__);
   }

   return(data_size);
}


/*+++++++++++++++++++++++++++++ byte_swap() +++++++++++++++++++++++++++++*/
static void
byte_swap(char *ptr)
{
  char  tmp_byte;

  tmp_byte = ptr[0];
  ptr[0] = ptr[3];
  ptr[3] = tmp_byte;
  tmp_byte = ptr[1];
  ptr[1] = ptr[2];
  ptr[2] = tmp_byte;

  return;
}
