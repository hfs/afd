/*
 *  fax2gts.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Deutscher Wetterdienst (DWD),
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
 **   fax2gts - changes FAX files to GTS T4 files
 **
 ** SYNOPSIS
 **   off_t fax2gts(char *path, char *filename)
 **            path     - path where the FAX file can be found
 **            filename - file name of the FAX file
 **
 ** DESCRIPTION
 **   Inserts a WMO-bulletin header.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to convert the file. Otherwise
 **   the size of the converted file is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.02.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                /* malloc(), free()                   */
#include <sys/stat.h>              /* stat()                             */
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"


/*############################## fax2gts() ##############################*/
off_t
fax2gts(char *path, char* filename)
{
   off_t bytes_written = 0;

   if (strlen(filename) < 18)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "Filename %s must be at least 18 bytes long and must be a WMO header.",
                  filename);
   }
   else
   {
      int  to_fd;
      char dest_file_name[MAX_PATH_LENGTH];

      /* Open destination file. */
      (void)sprintf(dest_file_name, "%s/.%s", path, filename);
      if ((to_fd = open(dest_file_name,
                        (O_WRONLY | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "Failed to open() %s : %s",
                     dest_file_name, strerror(errno));
      }
      else
      {
         int  ret = SUCCESS;
         char fullname[MAX_PATH_LENGTH],
              wmo_header[33];

         /* Write header. */
         wmo_header[0] = 1;
         wmo_header[1] = '\015'; /* CR */
         wmo_header[2] = '\015'; /* CR */
         wmo_header[3] = '\012'; /* LF */
         (void)memcpy(&wmo_header[4], filename, 18);
         wmo_header[10] = wmo_header[15] = ' ';
         wmo_header[22] = '\015'; /* CR */
         wmo_header[23] = '\015'; /* CR */
         wmo_header[24] = '\012'; /* LF */
         wmo_header[25] = 'D';
         wmo_header[26] = 'F';
         wmo_header[27] = 'A';
         wmo_header[28] = 'X';
         wmo_header[29] = '1';
         wmo_header[30] = '0';
         wmo_header[31] = '6';
         wmo_header[32] = '4';
         if (write(to_fd, wmo_header, 33) != 33)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to write() WMO header to `%s' : %s",
                        dest_file_name, strerror(errno));
            (void)close(to_fd);
            (void)unlink(dest_file_name);
            return(INCORRECT);
         }
         else
         {
            int from_fd;

            /* Open source file. */
            (void)sprintf(fullname, "%s/%s", path, filename);
            if ((from_fd = open(fullname, O_RDONLY, 0)) < 0)
            {
               receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to open() `%s' : %s",
                           fullname, strerror(errno));
               (void)close(to_fd);
               (void)unlink(dest_file_name);
               return(INCORRECT);
            }
            else
            {
               struct stat stat_buf;

               if (fstat(from_fd, &stat_buf) == -1)
               {
                  receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to fstat() `%s' : %s",
                              fullname, strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  if (stat_buf.st_size > 0)
                  {
                     char *buffer;

                     if ((buffer = malloc(stat_buf.st_blksize)) == NULL)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    "Failed to malloc() %d bytes : %s",
                                    stat_buf.st_blksize, strerror(errno));
                        ret = INCORRECT;
                     }
                     else
                     {
                        int bytes_buffered;

                        do
                        {
                           if ((bytes_buffered = read(from_fd, buffer,
                                                      stat_buf.st_blksize)) == -1)
                           {
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                          "Failed to read() %d bytes from `%s' : %s",
                                          stat_buf.st_blksize, fullname,
                                          strerror(errno));
                              ret = INCORRECT;
                              break;
                           }
                           if (bytes_buffered > 0)
                           {
                              if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                              {
                                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                             "Failed to write() %d bytes to `%s' : %s",
                                             bytes_buffered, dest_file_name,
                                             strerror(errno));
                                 ret = INCORRECT;
                                 break;
                              }
                           }
                        } while (bytes_buffered == stat_buf.st_blksize);
                        free(buffer);

                        if (ret != INCORRECT)
                        {
                           wmo_header[0] = '\015'; /* CR */
                           wmo_header[1] = '\015'; /* CR */
                           wmo_header[2] = '\012'; /* LF */
                           wmo_header[3] = 3;
                           if (write(to_fd, wmo_header, 4) != 4)
                           {
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                          "Failed to write() WMO end to `%s' : %s",
                                          dest_file_name, strerror(errno));
                           }
                           else
                           {
                              bytes_written = 33 + stat_buf.st_size + 4;
                           }
                        }
                     }
                  }
                  else
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 "Source FAX file `%s' is empty.", fullname);
                     ret = INCORRECT;
                  }
               }
               if (close(from_fd) == -1)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              "Failed to close() `%s' : %s",
                              fullname, strerror(errno));
               }
            }
         }
         if (close(to_fd) == -1)
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        "Failed to close() `%s' : %s",
                        dest_file_name, strerror(errno));
         }
         if (ret == SUCCESS)
         {
            /* Remove the original file */
            if (unlink(fullname) < 0)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to unlink() original TIFF file %s : %s",
                           fullname, strerror(errno));
            }
            if (rename(dest_file_name, fullname) < 0)
            {
               receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                           "Failed to rename() file %s to %s : %s",
                           dest_file_name, fullname, strerror(errno));
            }
         }
      }
   }
   return(bytes_written);
}
