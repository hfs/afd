/*
 *  fprint_dup_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_max_log_number - gets the maximum number of logfiles to keep
 **                        from AFD_CONFIG file
 **
 ** SYNOPSIS
 **   void get_max_log_number(int  *max_log_file_number,
 **                           char *definition,
 **                           int  default_value)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   Returns the number of logfiles to keep.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.01.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* sprintf()                       */
#include <stdlib.h>                   /* free()                          */
#include <unistd.h>                   /* F_OK                            */
#include "logdefs.h"

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;


/*######################## get_max_log_number() #########################*/
void
get_max_log_number(int  *max_log_file_number,
                   char *definition,
                   int  default_value)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file(config_file, &buffer) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, definition,
                         value, MAX_INT_LENGTH) != NULL)
      {
         *max_log_file_number = atoi(value);
         if ((*max_log_file_number < 1) || (*max_log_file_number > 599))
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Incorrect value (%d, must be more then 1 but less then 600) set in AFD_CONFIG for %s. Setting to default %d. (%s %d)\n",
                      *max_log_file_number, definition,
                      default_value, __FILE__, __LINE__);
            *max_log_file_number = default_value;
         }
      }
      free(buffer);
   }
   return;
}
