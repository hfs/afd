/*
 *  eval_filename_file.c - Part of AFD, an automatic file distribution
 *                         program.
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

#include "asmtpdefs.h"

DESCR__S_M3
/*
 ** NAME
 **   eval_filename_file - evaluates file with filenames
 **
 ** SYNOPSIS
 **   void eval_filename_file(char *file_name, struct data *p_db)
 **
 ** DESCRIPTION
 **   This function reads the filename file and store the
 **   data into struct data.
 **
 ** RETURN VALUES
 **   None, will just exit when it encounters any difficulties.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.12.2000 H.Kiehl    Created
 **
 */
DESCR__E_M3

#include <string.h>                /* strerror()                         */
#include <stdlib.h>                /* free()                             */
#include <errno.h>

/* External global variables. */
extern int sys_log_fd;


/*####################### eval_filename_file() ##########################*/
void
eval_filename_file(char *file_name, struct data *p_db)
{
   char *buffer;

   if (read_file(file_name, &buffer) != INCORRECT)
   {
      register int i, j;
      char         *ptr = buffer;

      /* First lets count the number of entries so we can later */
      /* allocate the memory we need.                           */
      do
      {
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            p_db->no_of_files++;
            ptr++;
         }
      } while (*ptr != '\0');

      RT_ARRAY(p_db->filename, p_db->no_of_files, MAX_PATH_LENGTH, char);

      ptr = buffer;
      j = 0;
      do
      {
         i = 0;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            p_db->filename[j][i] = *ptr;
            ptr++; i++;
         }
         if (*ptr == '\n')
         {
            ptr++;
         }
         p_db->filename[j][i] = '\0';
         j++;
      } while (*ptr != '\0');

      free(buffer);
   }

   return;
}
