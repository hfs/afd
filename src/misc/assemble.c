/*
 *  assemble.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   assemble - files stored as one file per bulletin are assembled
 **              into a single file
 **
 ** SYNOPSIS
 **   int assemble(char         *source_dir,
 **                char         *p_file_name,
 **                int          file_counter,
 **                char         *dest_file,
 **                int          type,
 **                unsigned int unique_number,
 **                int          *files_to_send,
 **                off_t        *file_size)
 **
 ** DESCRIPTION
 **   The function assembles all WMO bulletin files found in the source_dir
 **   into one file. Before each bulletin a length indicator is written,
 **   to simplify later extraction.
 **
 **       <length indicator><SOH><CR><CR><LF>nnn<CR><CR><LF>
 **       WMO header<CR><CR><LF>WMO message<CR><CR><LF><ETX>
 **
 **   Five length indicators with the following length are currently
 **   created by assemble():
 **
 **                   2 Byte - Vax standard
 **                   4 Byte - Low byte first
 **                   4 Byte - High byte first
 **                   4 Byte - MSS standard
 **                   8 Byte - WMO standard (plus 2 Bytes type indicator)
 **                   4 Byte - DWD
 **
 **   In addition it is possible to generate the file without a length
 **   indicator with the help of the ASCII standard method.
 **
 **   The file name of the new file will be dest_file.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to read any valid data from the
 **   file. On success SUCCESS will be returned and the file size of
 **   the new assembled file will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.05.1999 H.Kiehl Created
 **   14.06.2002 H.Kiehl Fixed assembling MSS files.
 **   08.08.2002 H.Kiehl Added new type DWD.
 **   18.05.2005 H.Kiehl Forgot to write type indicator for WMO standard.
 **   16.11.2009 H.Kiehl Fix error if dest_file has same name as one of
 **                      the source files.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc(), free()                       */
#include <unistd.h>           /* read(), write(), close()                */
#include <sys/types.h>
#include <sys/stat.h>         /* stat(), S_ISDIR()                       */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* Local function prototypes. */
static int  write_length_indicator(int, int, int, int);

/* #define _WITH_SOH_ETX_CHECK */


/*############################## assemble() #############################*/
int
assemble(char         *source_dir,
         char         *p_file_name,
         int          file_counter,
         char         *dest_file,
         int          type,
         unsigned int unique_number,
         int          *files_to_send,
         off_t        *file_size)
{
   int         buffer_size = 0,
               fd,
               i,
               length,
               to_fd = -1;
#ifndef _WITH_SOH_ETX_CHECK
   int         have_sohetx;
#endif
   char        *buffer = NULL,
               *p_src,
               temp_dest_file[1 + MAX_INT_HEX_LENGTH + 1];
   struct stat stat_buf;

   p_src = source_dir + strlen(source_dir);
   *p_src++ = '/';
   *file_size = 0;
   temp_dest_file[0] = '\0';

   for (i = 0; i < file_counter; i++)
   {
      (void)strcpy(p_src, p_file_name);
      if ((fd = open(source_dir, O_RDONLY)) == -1)
      {
         receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to open() `%s' : %s"),
                     source_dir, strerror(errno));
      }
      else
      {
         if (fstat(fd, &stat_buf) == -1)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to fstat() `%s' : %s"),
                        source_dir, strerror(errno));
            (void)close(fd);
         }
         else
         {
            if (stat_buf.st_size > 0)
            {
               if (buffer_size < stat_buf.st_size)
               {
                  if ((buffer = realloc(buffer, stat_buf.st_size)) == NULL)
                  {
                     receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                 _("realloc() error : %s"), strerror(errno));
                     (void)close(fd);
                     exit(INCORRECT);
                  }
                  else
                  {
                     buffer_size = stat_buf.st_size;
                  }
               }

               if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
               {
                  receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                              _("Failed to read() `%s' : %s"),
                              source_dir, strerror(errno));
                  (void)close(fd);
               }
               else
               {
#ifdef _WITH_SOH_ETX_CHECK
                  int additional_length = 0;
#endif

                  if (close(fd) == -1)
                  {
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("close() error : %s"), strerror(errno));
                  }
                  if (to_fd == -1)
                  {
                     if (temp_dest_file[0] == '\0')
                     {
                        (void)sprintf(temp_dest_file, ".%x", unique_number);
                     }
                     if ((to_fd = open(temp_dest_file, O_CREAT | O_RDWR,
                                       FILE_MODE)) == -1)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to open() `%s' : %s"),
                                    temp_dest_file, strerror(errno));
                        free(buffer);
                        return(INCORRECT);
                     }
                     if (type == FOUR_BYTE_DWD)
                     {
                        if (write(to_fd, "\000\000\000\000", 4) != 4)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                       _("Failed to write() first four zeros : %s"),
                                       strerror(errno));
                        }
                        else
                        {
                           *file_size += 4;
                        }
                     }
                  }
#ifdef _WITH_SOH_ETX_CHECK
                  if (type != ASCII_STANDARD)
                  {
                     if (buffer[0] != 1)
                     {
                        additional_length += 4;
                     }
                     if (buffer[stat_buf.st_size - 1] != 3)
                     {
                        additional_length += 4;
                     }
                     if ((length = write_length_indicator(to_fd,
                                                          type, YES,
                                                          stat_buf.st_size + additional_length)) < 0)
#else
                  if (type != ASCII_STANDARD)
                  {
                     if ((buffer[0] == 1) &&
                         (buffer[stat_buf.st_size - 1] == 3))
                     {
                        have_sohetx = YES;
                     }
                     else
                     {
                        have_sohetx = NO;
                     }
                     if ((length = write_length_indicator(to_fd,
                                                          type, have_sohetx,
                                                          stat_buf.st_size)) < 0)
#endif
                     {
                        if (length == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("write() error : %s"), strerror(errno));
                        }
                        continue;
                     }
                     *file_size += length;
                  }

#ifdef _WITH_SOH_ETX_CHECK
                  /* Check for SOH */
                  if (buffer[0] != 1)
                  {
                     if (write(to_fd, "\001\015\015\012", 4) != 4)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to write() SOH<CR><CR><LF> : %s"),
                                    strerror(errno));
                     }
                     else
                     {
                        *file_size += 4;
                     }
                  }
#endif

                  /* Write data */
                  if (writen(to_fd, buffer, stat_buf.st_size,
                             stat_buf.st_blksize) != stat_buf.st_size)
                  {
                      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                  _("Failed to writen() data part : %s"),
                                  strerror(errno));
                  }
                  else
                  {
                     *file_size += stat_buf.st_size;
                  }
                  if (type == FOUR_BYTE_DWD)
                  {
                     if ((length = write_length_indicator(to_fd,
                                                          type, NO,
                                                          stat_buf.st_size)) < 0)
                     {
                        if (length == -1)
                        {
                           receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                                       _("write() error : %s"),
                                       strerror(errno));
                        }
                     }
                     else
                     {
                        *file_size += length;
                     }
                  }

#ifdef _WITH_SOH_ETX_CHECK
                  /* Check for ETX */
                  if (buffer[stat_buf.st_size - 1] != 3)
                  {
                     if (write(to_fd, "\015\015\012\003", 4) != 4)
                     {
                        receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                                    _("Failed to write() <CR><CR><LF>ETX : %s"),
                                    strerror(errno));
                     }
                     else
                     {
                        *file_size += 4;
                     }
                  }
#endif
               } /* read() == successful */
            }
            else
            {
               if (close(fd) == -1)
               {
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("close() error : %s"), strerror(errno));
               }
            }
         }
         if (unlink(source_dir) == -1)
         {
            receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to unlink() `%s' : %s"),
                        source_dir, strerror(errno));
         }
      }
      p_file_name += MAX_FILENAME_LENGTH;
   } /* for (i = 0; i < file_counter; i++) */

   if (to_fd != -1)
   {
      if (type == FOUR_BYTE_DWD)
      {
         if (write(to_fd, "\000\000\000\000", 4) != 4)
         {
            receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                        _("Failed to write() last four zeros : %s"),
                        strerror(errno));
         }
         else
         {
            *file_size += 4;
         }
      }
      if (close(to_fd) == -1)
      {
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("close() error : %s"), strerror(errno));
      }
      if (rename(temp_dest_file, dest_file) == -1)
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Failed to rename() `%s' to `%s' : %s"),
                     temp_dest_file, dest_file, strerror(errno));
      }
   }

   *(p_src - 1) = '\0';
   if (buffer != NULL)
   {
      free(buffer);
   }
   *files_to_send = 1;

   return(SUCCESS);
}


/*+++++++++++++++++++++++ write_length_indicator() ++++++++++++++++++++++*/
static int
write_length_indicator(int fd, int type, int have_sohetx, int length)
{
   int           byte_order = 1,
                 write_length;
   unsigned char buffer[10];

   switch (type)
   {
      case TWO_BYTE      : /* Vax Standard */
         {
            unsigned short short_length = (unsigned short)length;

            if (*(char *)&byte_order == 1)
            {
               /* little-endian */
               buffer[0] = ((char *)&short_length)[0];
               buffer[1] = ((char *)&short_length)[1];
            }
            else
            {
               /* big-endian */
               buffer[0] = ((char *)&short_length)[1];
               buffer[1] = ((char *)&short_length)[0];
            }
            write_length = 2;
         }
         break;

      case FOUR_BYTE_LBF : /* Low byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = ((char *)&length)[0];
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         else
         {
            /* big-endian */
            buffer[0] = ((char *)&length)[3];
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         write_length = 4;
         break;

      case FOUR_BYTE_DWD :
      case FOUR_BYTE_HBF : /* High byte first */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = ((char *)&length)[3];
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         else
         {
            /* big-endian */
            buffer[0] = ((char *)&length)[0];
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         write_length = 4;
         break;

      case FOUR_BYTE_MSS : /* MSS Standard */
         if (*(char *)&byte_order == 1)
         {
            /* little-endian */
            buffer[0] = 250;
            buffer[1] = ((char *)&length)[2];
            buffer[2] = ((char *)&length)[1];
            buffer[3] = ((char *)&length)[0];
         }
         else
         {
            /* big-endian */
            buffer[0] = 250;
            buffer[1] = ((char *)&length)[1];
            buffer[2] = ((char *)&length)[2];
            buffer[3] = ((char *)&length)[3];
         }
         write_length = 4;
         break;

      case WMO_STANDARD  : /* WMO Standard */
         (void)sprintf((char *)buffer, "%08lu", (unsigned long)length);
         write_length = 10;
         buffer[8] = '0';
         if (have_sohetx == YES)
         {
            buffer[9] = '0';
         }
         else
         {
            buffer[9] = '1';
         }
         break;

      default            : /* Impossible! */
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown length type (%d) for assembling bulletins."),
                     type);
         return(-2);

   }

   return(write(fd, buffer, write_length));
}
