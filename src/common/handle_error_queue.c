/*
 *  handle_error_queue.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2007 Deutscher Wetterdienst (DWD),
 *                Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_error_queue - set of functions to handle the AFD error queue
 **
 ** SYNOPSIS
 **   int  attach_error_queue(void)
 **   int  detach_error_queue(void)
 **   void add_to_error_queue(unsigned int               job_id,
 **                           struct filetransfer_status *fsa,
 **                           int                        error_id)
 **   int  check_error_queue(unsigned int job_id, int queue_threshold)
 **   int  remove_from_error_queue(unsigned int               job_id,
 **                                struct filetransfer_status *fsa)
 **   int  print_error_queue(FILE *fp)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.11.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <errno.h>

/* External global definitions. */
extern char               *p_work_dir;

/* Local definitions. */
#define ERROR_QUE_BUF_SIZE 2
struct error_queue
       {
          unsigned int job_id;
          unsigned int no_to_be_queued; /* Number to be queued. */
          unsigned int host_id;
          unsigned int special_flag; /* Not used. */
       };

/* Local variables. */
static int                eq_fd = -1,
                          *no_of_error_ids;
static size_t             eq_size;
static struct error_queue *eq = NULL;

/* Local function prototypes. */
static void               check_error_queue_space(void);


/*####################### attach_error_queue() ##########################*/
int
attach_error_queue(void)
{
   if (eq_fd == -1)
   {
      char fullname[MAX_PATH_LENGTH],
           *ptr;

      eq_size = (ERROR_QUE_BUF_SIZE * sizeof(struct error_queue)) + AFD_WORD_OFFSET;
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR, ERROR_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &eq_fd, eq_size, NULL,
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() `%s' : %s", fullname, strerror(errno));
         if (eq_fd != -1)
         {
            (void)close(eq_fd);
            eq_fd = -1;
         }
         return(INCORRECT);
      }
      no_of_error_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      eq = (struct error_queue *)ptr;
   }

   return(SUCCESS);
}


/*####################### detach_error_queue() ##########################*/
int
detach_error_queue(void)
{
   if (eq_fd > 0)
   {
      if (close(eq_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      eq_fd = -1;
   }

   if (eq != NULL)
   {
      /* Detach from FSA */
#ifdef HAVE_MMAP
      if (munmap(((char *)eq - AFD_WORD_OFFSET), eq_size) == -1)
#else
      if (munmap_emu((void *)((char *)eq - AFD_WORD_OFFSET)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to munmap() from error queue : %s", strerror(errno));
         return(INCORRECT);
      }
      eq = NULL;
   }

   return(SUCCESS);
}


/*####################### add_to_error_queue() ##########################*/
void
add_to_error_queue(unsigned int               job_id,
                   struct filetransfer_status *fsa,
                   int                        error_id)
{
   int detach,
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return;
      }
   }
   else
   {
      detach = NO;
   }

   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         fsa->host_status |= ERROR_QUEUE_SET;
         if (detach == YES)
         {
            (void)detach_error_queue();
         }

         return;
      }
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   check_error_queue_space();
   eq[*no_of_error_ids].job_id = job_id;
   eq[*no_of_error_ids].no_to_be_queued = 0;
   eq[*no_of_error_ids].host_id = fsa->host_id;
   eq[*no_of_error_ids].special_flag = 0;
   (*no_of_error_ids)++;
   fsa->host_status |= ERROR_QUEUE_SET;
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif
   event_log(0L, EC_HOST, ET_EXT, EA_START_ERROR_QUEUE, "%s%c%x%c%x",
             fsa->host_alias, SEPARATOR_CHAR, job_id, SEPARATOR_CHAR, error_id);

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return;
}


/*######################## check_error_queue() ##########################*/
int
check_error_queue(unsigned int job_id, int queue_threshold)
{
   int detach,
       i,
       ret = 0;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         if ((queue_threshold == -1) ||
             (eq[i].no_to_be_queued >= queue_threshold))
         {
            ret = 1;
         }
         else
         {
            eq[i].no_to_be_queued++;
         }
         break;
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }
   return(ret);
}


/*##################### remove_from_error_queue() #######################*/
int
remove_from_error_queue(unsigned int job_id, struct filetransfer_status *fsa)
{
   int detach,
       gotcha = NO,   /* Are there other ID's from the same host? */
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         if (i != (*no_of_error_ids - 1))
         {
            size_t move_size;

            move_size = (*no_of_error_ids - 1 - i) * sizeof(struct error_queue);
            (void)memmove(&eq[i], &eq[i + 1], move_size);
         }
         (*no_of_error_ids)--;

         if (gotcha == NO)
         {
            int j;

            for (j = i; j < *no_of_error_ids; j++)
            {
               if (eq[j].host_id == fsa->host_id)
               {
                  gotcha = YES;
                  break;
               }
            }
            if ((gotcha == NO) && (fsa->host_status & ERROR_QUEUE_SET))
            {
               fsa->host_status ^= ERROR_QUEUE_SET;
               event_log(0L, EC_HOST, ET_EXT, EA_STOP_ERROR_QUEUE, "%s%c%x",
                         fsa->host_alias, SEPARATOR_CHAR, job_id);
            }
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          "%s: Removed job %x from error queue.",
                          fsa->host_dsp_name, job_id);
            }
         }

         check_error_queue_space();
#ifdef LOCK_DEBUG
         unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
         unlock_region(eq_fd, 1);
#endif

         if (detach == YES)
         {
            (void)detach_error_queue();
         }
         return(SUCCESS);
      }
      else
      {
         if (eq[i].host_id == fsa->host_id)
         {
            gotcha = YES;
         }
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }
   return(INCORRECT);
}


/*######################## print_error_queue() ##########################*/
int
print_error_queue(FILE *fp)
{
   int detach,
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

   for (i = 0; i < *no_of_error_ids; i++)
   {
      (void)fprintf(fp, "%x %u %x %u\n",
                    eq[i].job_id, eq[i].no_to_be_queued, eq[i].host_id,
                    eq[i].special_flag);
   }

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++ check_error_queue_space() ++++++++++++++++++++++*/
static void
check_error_queue_space(void)
{
   if ((*no_of_error_ids != 0) &&
       ((*no_of_error_ids % ERROR_QUE_BUF_SIZE) == 0))
   {
      char *ptr;

      eq_size = (((*no_of_error_ids / ERROR_QUE_BUF_SIZE) + 1) *
                ERROR_QUE_BUF_SIZE * sizeof(struct error_queue)) +
                AFD_WORD_OFFSET;
      ptr = (char *)eq - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(eq_fd, ptr, eq_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_of_error_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      eq = (struct error_queue *)ptr;
   }
   return;
}
