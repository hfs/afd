/*
 *  get_recipient.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   get_recipient       - get recipient for a certain job ID.
 **   get_recipient_alias - get recipient for a certain job ID.
 **
 ** SYNOPSIS
 **   int get_recipient(unsigned int job_id)
 **   int get_recipient_alias(unsigned int job_id)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.04.2010 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>
#include "aldadefs.h"

/* External global variables. */
extern struct alda_odata olog;
extern struct jid_data   jidd;


/*########################### get_recipient() ###########################*/
int
get_recipient(unsigned int job_id)
{
   if ((jidd.prev_pos != -1) && (job_id == jidd.jd[jidd.prev_pos].job_id))
   {
      (void)strcpy(olog.recipient, jidd.jd[jidd.prev_pos].recipient);
      return(SUCCESS);
   }
   else
   {
      int i;

      for (i = 0; i < jidd.no_of_job_ids; i++)
      {
         if (job_id == jidd.jd[i].job_id)
         {
            (void)strcpy(olog.recipient, jidd.jd[i].recipient);
            jidd.prev_pos = i;
            return(SUCCESS);
         }
      }
   }   
   olog.recipient[0] = '\0';
   jidd.prev_pos = -1;

   return(INCORRECT);
}


/*######################## get_recipient_alias() ########################*/
int
get_recipient_alias(unsigned int job_id)
{
   if ((jidd.prev_pos != -1) && (job_id == jidd.jd[jidd.prev_pos].job_id))
   {
      (void)strcpy(olog.recipient, jidd.jd[jidd.prev_pos].recipient);
      return(SUCCESS);
   }
   else
   {
      int i;

      for (i = 0; i < jidd.no_of_job_ids; i++)
      {
         if (job_id == jidd.jd[i].job_id)
         {
            (void)strcpy(olog.recipient, jidd.jd[i].recipient);
            (void)strcpy(olog.alias_name, jidd.jd[i].host_alias);
            jidd.prev_pos = i;
            return(SUCCESS);
         }
      }
   }   
   olog.recipient[0] = '\0';
   jidd.prev_pos = -1;

   return(INCORRECT);
}
