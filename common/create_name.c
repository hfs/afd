/*
 *  create_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_name - creates a unique name for the AFD
 **
 ** SYNOPSIS
 **   int create_name(char           *p_path,   - the path where the job
 **                                               can be found by the FD.
 **                   signed char    priority,  - the priority of the job
 **                   time_t         time_val,  - Date value in seconds
 **                   int            id,        - job or dir ID
 **                   unsigned short *unique_number,
 **                   char           *msg_name) - Storage for the message
 **                                               name
 **
 ** DESCRIPTION
 **   Generates name for message and directory for each job.
 **   The syntax will be as follows:
 **            x_nnnnnnnnnn_llll_jjj
 **            |     |       |    |
 **            |     |       |    +--> Job Identifier
 **            |     |       +-------> Counter which is set to zero
 **            |     |                 after each second. This
 **            |     |                 ensures that no message can
 **            |     |                 have the same name in one
 **            |     |                 second.
 **            |     +---------------> creation time in seconds
 **            +---------------------> priority of the job
 **
 **   When priority is set to NO_PRIORITY, x_ is omitted in the
 **   directory name and a directory is created in the AFD_TMP_DIR
 **   (pool).
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it has created a new directory. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.10.1995 H.Kiehl Created
 **   11.05.1997 H.Kiehl The caller of the function is now responsible
 **                      for the storage area of the message name.
 **   19.01.1998 H.Kiehl Added job ID to name.
 **
 */
DESCR__E_M3

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <unistd.h>                   /* read(), write(), close()        */
#include <sys/types.h>
#include <sys/stat.h>                 /* mkdir()                         */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

extern int  counter_fd,
            sys_log_fd;
extern char *p_work_dir;


/*############################ create_name() ############################*/
int
create_name(char           *p_path,     /* Path where the new directory  */
                                        /* is to be created.             */
            signed char    priority,    /* Priority of the job.          */
            time_t         time_val,    /* Date value in seconds.        */
            int            id,          /* Job or dir ID.                */
            unsigned short *unique_number,
            char           *msg_name)   /* Storage to return msg name.   */
{
   int  i,
        j = 0,
        max_value = MAX_MSG_PER_SEC,
        tmp_max_value,
        counter;
   char *ptr,
        tmpname[MAX_PATH_LENGTH];

   if (next_counter(&counter) < 0)
   {
      return(INCORRECT);
   }
   counter--;

   /* Initialise variables */
   i = tmp_max_value = counter;

   /*
    * Reset errno. When we enter the following loop errno
    * might already have either EMLINK or ENOSPC. In the 
    * function archive_files() used by sf_xxx this might
    * result in an infinite loop!
    */
   errno = 0;

   /* Now try to create directory */
   (void)strcpy(tmpname, p_path);
   ptr = tmpname + strlen(tmpname);
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
   }
   do
   {
      if ((errno == EMLINK) || (errno == ENOSPC))
      {
         *msg_name = '\0';
         return(INCORRECT);
      }
      if (counter++ == MAX_MSG_PER_SEC)
      {
         counter = 0;
      }
      if (priority == NO_PRIORITY)
      {
         (void)sprintf(msg_name, "%ld_%04d_%d", time_val, counter,
                       id); /* NOTE: dir ID is inserted here! */
      }
      else
      {
         (void)sprintf(msg_name, "%c_%ld_%04d_%d", priority,
                       time_val, counter, id);
      }
      (void)strcpy(ptr, msg_name);
      if (i++ == max_value)
      {
         max_value = tmp_max_value;
         i = 0;

         if (j++ > 1)
         {
            /*
             * We have tried all possible values and none where
             * successful. So we don't end up in an endless loop
             * lets return here.
             */
            *msg_name = '\0';
            return(INCORRECT);
         }
      }
   } while(mkdir(tmpname, DIR_MODE) == -1);

   if (unique_number != NULL)
   {
      *unique_number = (unsigned short)counter;
   }

   return(SUCCESS);
}
