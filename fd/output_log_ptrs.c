/*
 *  output_log_ptrs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   output_log_ptrs - initialises and sets data pointers for output log
 **
 ** SYNOPSIS
 **   void output_log_ptrs(int            *ol_fd,
 **                        unsigned int   **ol_job_number,
 **                        char           **ol_data,
 **                        char           **ol_file_name,
 **                        unsigned short **ol_file_name_length,
 **                        off_t          **ol_file_size,
 **                        size_t         *ol_size,
 **                        clock_t        **ol_transfer_time,
 **                        char           *tr_hostname,
 **                        int            protocol)
 **
 ** DESCRIPTION
 **   When sf_xxx() writes data to the output log via a fifo, it
 **   does so by writing 'ol_data' once. To do this a set of
 **   pointers have to be prepared which point to the right
 **   place in the buffer 'ol_data'. Once the buffer has been
 **   filled with the necessary data it will look as follows:
 **   <TD><FS><JN><FNL><HN>\0<LFN>[ /<RFN>]\0[<AD>\0]
 **     |   |   |   |    |     |       |       |
 **     |   |   |   |    |     |       |       +-> If FNL > 0 this contains
 **     |   |   |   |    |     |       |           a \0 terminated string of
 **     |   |   |   |    |     |       |           the Archive Directory.
 **     |   |   |   |    |     |       +---------> Remote file name.
 **     |   |   |   |    |     +-----------------> Local file name.
 **     |   |   |   |    +-----------------------> \0 terminated string of
 **     |   |   |   |                              the Host Name and protocol.
 **     |   |   |   +----------------------------> An unsigned short holding
 **     |   |   |                                  the File Name Length if
 **     |   |   |                                  the file has been archived.
 **     |   |   |                                  If not, FNL = 0.
 **     |   |   +--------------------------------> Unsigned int holding the
 **     |   |                                      Job Number.
 **     |   +------------------------------------> File Size of type off_t.
 **     +----------------------------------------> Transfer Duration of type
 **                                                time_t.
 **
 ** RETURN VALUES
 **   When successful it opens the fifo to the output log and assigns
 **   memory for the buffer 'ol_data'. The following values are being
 **   returned: ol_fd, *ol_job_number, *ol_data, *ol_file_name,
 **             *ol_file_name_length, *ol_file_size, ol_size,
 **             *ol_transfer_time.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.05.1997 H.Kiehl Created
 **   10.12.1997 H.Kiehl Made ol_file_name_length unsigned short due to
 **                      trans_rename option.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int        sys_log_fd;
extern char       *p_work_dir;
extern struct job db;


/*########################### output_log_ptrs() #########################*/
void
output_log_ptrs(int            *ol_fd,
                unsigned int   **ol_job_number,
                char           **ol_data,
                char           **ol_file_name,
                unsigned short **ol_file_name_length,
                off_t          **ol_file_size,
                size_t         *ol_size,
                clock_t        **ol_transfer_time,
                char           *tr_hostname,
                int            protocol)
{
   char output_log_fifo[MAX_PATH_LENGTH];

   (void)strcpy(output_log_fifo, p_work_dir);
   (void)strcat(output_log_fifo, FIFO_DIR);
   (void)strcat(output_log_fifo, OUTPUT_LOG_FIFO);
   if ((*ol_fd = coe_open(output_log_fifo, O_RDWR)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                output_log_fifo, strerror(errno), __FILE__, __LINE__);
      db.output_log = NO;
   }
   else
   {
      int offset;

      /*
       * Lets determine the largest offset so the 'structure'
       * is aligned correctly.
       */
      offset = sizeof(clock_t);
      if (sizeof(off_t) > offset)
      {
         offset = sizeof(off_t);
      }
      if (sizeof(unsigned int) > offset)
      {
         offset = sizeof(unsigned int);
      }

      /*
       * Create a buffer which we use can use to send our
       * data to the output log process. The buffer has the
       * following structure:
       *
       * <transfer time><file size><job number><host name>
       * <file name length><file name + archive dir>
       */
      *ol_size = offset + offset + offset +
                 sizeof(unsigned short) +       /* file size length */
                 MAX_HOSTNAME_LENGTH + 2 + 1 +
                 MAX_FILENAME_LENGTH + 1 +      /* local file name  */
                 MAX_FILENAME_LENGTH + 2 +      /* remote file name */
                 MAX_FILENAME_LENGTH + 1;       /* archive dir      */
      if ((*ol_data = calloc(*ol_size, sizeof(char))) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "calloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         db.output_log = NO;
      }
      else
      {
         *ol_size = offset + offset + offset +
                    sizeof(unsigned short) +
                    MAX_HOSTNAME_LENGTH + 2 +
                    1 + 1 + 1;                /* NOTE: + 1 + 1 + 1 is */
                                              /* for '\0' at end      */
                                              /* of host + file name  */
                                              /* and archive dir.     */
         *ol_transfer_time = (clock_t *)*ol_data;
         *ol_file_size = &(*(off_t *)(*ol_data + offset));
         *ol_job_number = (unsigned int *)(*ol_data + offset + offset);
         *ol_file_name_length = (unsigned short *)(*ol_data + offset +
                                offset + offset);
         *ol_file_name = (char *)(*ol_data + offset + offset + offset +
                         sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 2 + 1);
         (void)sprintf((char *)(*ol_data + offset + offset + offset + sizeof(unsigned short)),
                       "%-*s %x", MAX_HOSTNAME_LENGTH, tr_hostname, protocol);
      }
   }

   return;
}
