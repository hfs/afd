/*
 *  remove_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   remove_ls_data - Removes directory listing for given directory.
 **
 ** SYNOPSIS
 **   void remove_ls_data(int fra_pos)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.06.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ remove_ls_data() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
remove_ls_data(int fra_pos)
{
   char list_file[MAX_PATH_LENGTH];

   (void)sprintf(list_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, LS_DATA_DIR, fra[fra_pos].dir_alias);
   if ((unlink(list_file) == -1) && (errno != ENOENT))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to unlink() `%s' : %s",
                 list_file, strerror(errno));
   }

   return;
}
