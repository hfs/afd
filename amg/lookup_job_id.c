/*
 *  lookup_job_id.c - Part of AFD, an automatic file distribution program.
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
 **   lookup_job_id - searches for a job ID
 **
 ** SYNOPSIS
 **   void lookup_job_id(struct instant_db *p_db, int *jid_number)
 **
 ** DESCRIPTION
 **   The function lookup_job_id() searches for the job ID of the
 **   job described in 'p_db'. If it is found, the job_id is
 **   initialised with the position in the job_id_data structure.
 **   If the job ID is not found, it gets appended to the current
 **   structure and a new message is created in the message
 **   directory.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strcmp(), strcpy(), strlen(), strerror() */
#include <stdlib.h>          /* realloc()                                */
#include <sys/types.h>
#include <utime.h>           /* utime()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                 jd_fd,
                           *no_of_dir_names,
                           *no_of_job_ids,
                           sys_log_fd;
extern char                *gotcha,
                           msg_dir[],
                           *p_msg_dir;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;


/*########################### lookup_job_id() ###########################*/
void
lookup_job_id(struct instant_db *p_db, int *jid_number)
{
   register int i;
   char         *ptr;

   for (i = 0; i < *no_of_job_ids; i++)
   {
      if ((gotcha[i] == NO) &&
          (dnb[jd[i].dir_id_pos].dir_id == p_db->dir_no) &&
          (jd[i].priority == p_db->priority) &&
          (jd[i].no_of_files == p_db->no_of_files) &&
          (jd[i].no_of_loptions == p_db->no_of_loptions) &&
          (jd[i].no_of_soptions == p_db->no_of_soptions) &&
          (strcmp(jd[i].recipient, p_db->recipient) == 0))
      {
         /*
          * NOTE: Since all standart options are stored in a character
          *       array separated by a newline, it is NOT necessary
          *       to check each element.
          */
         if (jd[i].no_of_soptions > 0)
         {
            if (strcmp(jd[i].soptions, p_db->soptions) != 0)
            {
               continue;
            }
         }

         /*
          * NOTE: Local options are stored in an array separated by a
          *       binary zero. Thus we have to go through the list
          *       and check each element.
          */
         if (jd[i].no_of_loptions > 0)
         {
            register int gotcha = NO,
                         j;
            char         *p_loptions_db = p_db->loptions,
                         *p_loptions_jd = jd[i].loptions;

            for (j = 0; j < jd[i].no_of_loptions; j++)
            {
               if (strcmp(p_loptions_jd, p_loptions_db) != 0)
               {
                  gotcha = YES;
                  break;
               }
               NEXT(p_loptions_db);
               NEXT(p_loptions_jd);
            }
            if (gotcha == YES)
            {
               continue;
            }
         }

         /*
          * Same is true for the number of file filters. Each filter
          * must be compared!
          */
         if (jd[i].no_of_files > 0)
         {
            register int gotcha = NO,
                         j;
            char         *p_file = p_db->files;

            for (j = 0; j < jd[i].no_of_files; j++)
            {
               if (strcmp(jd[i].file_list[j], p_file) != 0)
               {
                  gotcha = YES;
                  break;
               }
               NEXT(p_file);
            }
            if (gotcha == YES)
            {
               continue;
            }
         }
         gotcha[i] = YES;
         p_db->job_id = jd[i].job_id;
         (void)sprintf(p_db->str_job_id, "%d", jd[i].job_id);

         /* Touch the message file so FD knows this is a new file */
         (void)sprintf(p_msg_dir, "%d", p_db->job_id);
         if (utime(msg_dir, NULL) == -1)
         {
            if (errno == ENOENT)
            {
               /*
                * If the message file has been removed for whatever
                * reason, recreate the message or else the FD will
                * not know what to do with it.
                */
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Message %d not there, recreating it. (%s %d)\n",
                         p_db->job_id, __FILE__, __LINE__);
               if (create_message(p_db->job_id, p_db->recipient, p_db->soptions) != SUCCESS)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Failed to create message for JID %d. (%s %d)\n",
                            p_db->job_id, __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
            else
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to change modification time of %s : %s (%s %d)\n",
                         msg_dir, strerror(errno),
                         __FILE__, __LINE__);
            }
         }

         return;
      }
   } /* for (i = 0; i < *no_of_job_ids; i++) */

   /*
    * This is a brand new job! Append this to the job_id_data structure.
    * But first check if there is still enough space in the structure.
    */
   if ((*no_of_job_ids != 0) &&
       ((*no_of_job_ids % JOB_ID_DATA_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                        JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)jd - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(jd_fd, ptr, new_size)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "mmap_resize() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;

      /*
       * Do not forget to increase the gotcha list or else we will get
       * a core dump.
       */
      new_size = ((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                 JOB_ID_DATA_STEP_SIZE * 1;

      if ((gotcha = realloc(gotcha, new_size)) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "realloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   for (i = 0; i < *no_of_dir_names; i++)
   {
      if (p_db->dir_no == dnb[i].dir_id)
      {
         jd[*no_of_job_ids].dir_id_pos = i;
         break;
      }
   }
   jd[*no_of_job_ids].priority = p_db->priority;
   jd[*no_of_job_ids].no_of_files = p_db->no_of_files;
   ptr = p_db->files;
   for (i = 0; i < p_db->no_of_files; i++)
   {
      (void)strcpy(jd[*no_of_job_ids].file_list[i], ptr);
      NEXT(ptr);
   }
   jd[*no_of_job_ids].no_of_loptions = p_db->no_of_loptions;
   jd[*no_of_job_ids].no_of_soptions = p_db->no_of_soptions;
   (void)strcpy(jd[*no_of_job_ids].recipient, p_db->recipient);
   if (p_db->loptions != NULL)
   {
      ptr = p_db->loptions;
      for (i = 0; i < p_db->no_of_loptions; i++)
      {
         NEXT(ptr);
      }
      (void)memcpy(jd[*no_of_job_ids].loptions, p_db->loptions,
                   (ptr - p_db->loptions));
   }
   if (p_db->soptions != NULL)
   {
      (void)strcpy(jd[*no_of_job_ids].soptions, p_db->soptions);
   }
   (void)strcpy(jd[*no_of_job_ids].host_alias, p_db->host_alias);
   p_db->job_id = *jid_number;
   (void)sprintf(p_db->str_job_id, "%d", *jid_number);
   jd[*no_of_job_ids].job_id = *jid_number;
   (*jid_number)++;
   if ((*jid_number > 2147483647) || (*jid_number < 0))
   {
      *jid_number = 0;
   }
   (*no_of_job_ids)++;

   /*
    * Generate a message in the message directory.
    */
   if (create_message(p_db->job_id, p_db->recipient, p_db->soptions) != SUCCESS)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to create message for JID %d. (%s %d)\n",
                p_db->job_id, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return;
}
