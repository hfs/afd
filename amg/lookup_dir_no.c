/*
 *  lookup_dir_no.c - Part of AFD, an automatic file distribution program.
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
 **   lookup_dir_no - searches for a directory number
 **
 ** SYNOPSIS
 **   int lookup_dir_no(char *dir_name, int *did_number)
 **
 ** DESCRIPTION
 **   This function searches in the structure dir_name_buf for the
 **   directory name 'dir_name'. If it does not find this directory
 **   it adds it to the structure.
 **
 ** RETURN VALUES
 **   Returns the directory ID.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **   27.03.2000 H.Kiehl Return position in struct dir_name_buf
 **                      and not the directory ID.
 **
 */
DESCR__E_M3

#include <string.h>           /* strcmp(), strcpy(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                 dnb_fd,
                           *no_of_dir_names,
                           sys_log_fd;
extern struct dir_name_buf *dnb;


/*########################### lookup_dir_no() ###########################*/
int
lookup_dir_no(char *dir_name, int *did_number)
{
   register int i;

   for (i = 0; i < *no_of_dir_names; i++)
   {
      if (strcmp(dnb[i].dir_name, dir_name) == 0)
      {
         return(i);
      }
   }

   /*
    * This is a new directory, append it to the directory list
    * structure.
    */
   if ((*no_of_dir_names != 0) &&
       ((*no_of_dir_names % DIR_NAME_BUF_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_dir_names / DIR_NAME_BUF_SIZE) + 1) *
                        DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)dnb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(dnb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "mmap() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_dir_names = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   (void)strcpy(dnb[i].dir_name, dir_name);

   /* Determine the directory ID */
   dnb[i].dir_id = *did_number;
   (*did_number)++;
   if ((*did_number > 2147483647) || (*did_number < 0))
   {
      *did_number = 0;
   }
   (*no_of_dir_names)++;

   return(i);
}
