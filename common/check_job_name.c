/*
 *  check_job_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_job_name - checks if name is an AFD job name
 **
 ** SYNOPSIS
 **   int check_job_name(char *name)
 **
 ** DESCRIPTION
 **   The function check_job_name() checks if 'name' conforms to
 **   the AFD job name format. This format is as follows:
 **             x_nnnnnnnnnn_llll_jjjj
 **             |     |       |    |
 **             |     |       |    +--> Job number.
 **             |     |       +-------> Unique counter.
 **             |     +---------------> Unix time in seconds.
 **             +---------------------> Priority of the job.
 **
 ** RETURN VALUES
 **   If name does not conform to the AFD job name, INCORRECT
 **   will be returned. Otherwise it will return SUCCESS.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <ctype.h>                 /* isdigit()                          */


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ check_job_name() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
check_job_name(char *name)
{
   if (isdigit(name[0]) != 0)
   {
      if (name[1] == '_')
      {
         register int i = 2;

         while (name[i] != '\0')
         {
            if (isdigit(name[i]) == 0)
            {
               if (name[i] != '_')
               {
                  return(INCORRECT);
               }
            }
            i++;
         }

         return(SUCCESS);
      }
   }

   return(INCORRECT);
}
