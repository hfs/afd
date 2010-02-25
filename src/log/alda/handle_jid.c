/*
 *  handle_jid.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   handle_jid - attaches or detaches to ADL (AFD Directory List)
 **
 ** SYNOPSIS
 **   (void)alloc_jid(char *alias)
 **   (void)dealloc_jid(void)
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
 **   11.02.2010 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf()                      */
#include <stdlib.h>                    /* free()                         */
#include <errno.h>
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif

/* External global variables. */
extern char            *p_work_dir;
extern struct jid_data jidd;


/*############################# alloc_jid() #############################*/
void
alloc_jid(char *alias)
{
#ifdef WITH_AFD_MON
   if (alias == NULL)
   {
      (void)sprintf(jidd.name, "%s%s%s",
                    p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   }
   else
   {
      (void)sprintf(jidd.name, "%s%s%s.%s",
                    p_work_dir, FIFO_DIR, AJL_FILE_NAME, alias);
   }
#else
   (void)sprintf(jidd.name, "%s%s%s", p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
#endif
   if (read_job_ids(jidd.name, &jidd.no_of_job_ids, &jidd.jd) == INCORRECT)
   {
      (void)fprintf(stderr,
                    "Failed to read `%s', see system log for more details (%s %d)\n",
                    jidd.name, __FILE__, __LINE__);
      jidd.jd = NULL;
      jidd.name[0] = '\0';
      jidd.prev_pos = -1;
   }

   return;
}


/*########################### dealloc_jid() #############################*/
void
dealloc_jid(void)
{
   if (jidd.jd != NULL)
   {
      free(jidd.jd);
      jidd.jd = NULL;
      jidd.name[0] = '\0';
      jidd.prev_pos = -1;
   }

   return;
}
