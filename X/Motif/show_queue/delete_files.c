/*
 *  delete_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_files - deletes selected files from the AFD queue
 **
 ** SYNOPSIS
 **   void delete_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   delete_files() will delete the given files from the AFD queue
 **   and remove them from the display list and qfl structure.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.07.2001 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free()                          */
#include <unistd.h>         /* rmdir()                                   */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>       /* mmap(), munmap()                          */
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "x_common_defs.h"
#include "show_queue.h"
#include "fddefs.h"

/* External global variables */
extern Display                 *display;
extern Widget                  toplevel_w,
                               listbox_w,
                               statusbox_w;
extern int                     toggles_set;
extern unsigned int            total_no_files;
extern double                  total_file_size;
extern char                    *p_work_dir,
                               user[];
extern struct queued_file_list *qfl;
#ifdef _DELETE_LOG
extern struct delete_log       dl;
#endif

/* Global variables */
int                            fsa_fd = -1,
                               fsa_id,
                               no_of_hosts,
                               counter_fd;
#ifndef _NO_MMAP
off_t                          fsa_size;
#endif
struct filetransfer_status     *fsa;
struct afd_status              *p_afd_status;


/*############################ delete_files() ###########################*/
void
delete_files(int no_selected, int *select_list)
{
   int                 deleted,
                       fd,
                       files_deleted = 0,
                       files_not_deleted = 0,
                       gotcha,
                       i,
                       k,
                       *no_msg_queued,
                       skipped;
   off_t               dnb_size,
                       qb_size;
   char                fullname[MAX_PATH_LENGTH],
                       message[MAX_MESSAGE_LENGTH];
   struct stat         stat_buf;
   struct queue_buf    *qb;
   struct dir_name_buf *dnb;

   /* Map to directory name buffer. */
   (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to fstat() <%s> : %s (%s %d)",
                 fullname, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to mmap() to <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      dnb_size = stat_buf.st_size;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
      (void)close(fd);
   }
   else
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Dirname database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(fd);
      return;
   }

   if (toggles_set & SHOW_OUTPUT)
   {
      /* Map to FD queue. */
      (void)sprintf(fullname, "%s%s%s", p_work_dir, FIFO_DIR,
                    MSG_QUEUE_FILE);
      if ((fd = open(fullname, O_RDWR)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() <%s> : %s (%s %d)",
                    fullname, strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to mmap() to <%s> : %s (%s %d)",
                       fullname, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         qb_size = stat_buf.st_size;
         no_msg_queued = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         qb = (struct queue_buf *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Dirname database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      /* Map to the FSA. */
      if (fsa_attach() == INCORRECT)
      {
         (void)fprintf(stderr, "Failed to attach to FSA.\n");
         exit(INCORRECT);
      }

      if (attach_afd_status() < 0)
      {
         (void)fprintf(stderr,
                       "Failed to map to AFD status area. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   for (i = 0; i < no_selected; i++)
   {
      if (qfl[select_list[i] - 1].queue_type == SHOW_OUTPUT)
      {
         /*
          * Ensure that it's still in queue and currently not processed.
          * The way it is done here is not quit correct, since no
          * locking is used here and FD might just take one we are
          * currently deleting. On the other hand locking is to
          * expensive, lets see how things work out.
          */
         gotcha = NO;
         for (k = (*no_msg_queued - 1); k > -1; k--)
         {
            if (strcmp(qb[k].msg_name, qfl[select_list[i] - 1].msg_name) == 0)
            {
               if (qb[k].pid == PENDING)
               {
                  gotcha = YES;
               }
               break;
            }
         }
         if (gotcha == YES)
         {
            int length;

            if (qfl[select_list[i] - 1].in_error_dir == YES)
            {
               length = sprintf(fullname, "%s%s%s/%s/%s/%s",
                                p_work_dir, AFD_FILE_DIR, ERROR_DIR,
                                qfl[select_list[i] - 1].hostname,
                                qfl[select_list[i] - 1].msg_name,
                                qfl[select_list[i] - 1].file_name);
            }
            else
            {
               length = sprintf(fullname, "%s%s/%s/%s",
                                p_work_dir, AFD_FILE_DIR,
                                qfl[select_list[i] - 1].msg_name,
                                qfl[select_list[i] - 1].file_name);
            }
            if (unlink(fullname) != -1)
            {
               int  pos;
               char *ptr;

               if ((pos = get_host_position(fsa, qfl[select_list[i] - 1].hostname,
                                            no_of_hosts)) != INCORRECT)
               {
                  off_t lock_offset;

                  lock_offset = (char *)&fsa[pos] - (char *)fsa;
                  lock_region_w(fsa_fd, (char *)&fsa[pos].total_file_counter - (char *)fsa);
                  fsa[pos].total_file_counter--;
                  fsa[pos].total_file_size -= qfl[select_list[i] - 1].size;
                  unlock_region(fsa_fd, (char *)&fsa[pos].total_file_counter - (char *)fsa);
                  unlock_region(fsa_fd, lock_offset);
               }
               deleted = YES;

               /*
                * Try to delete the unique directory where the file was
                * stored. If we are successful we must tell FD so it can
                * remove this job from its internal queue.
                */
               ptr = fullname + length - 1;
               while (*ptr != '/')
               {
                  ptr--;
               }
               *ptr = '\0';
               if (rmdir(fullname) == -1)
               {
                  if (errno != ENOTEMPTY)
                  {
                     (void)xrec(toplevel_w, INFO_DIALOG,
                                "Failed to rmdir() <%s> : %s (%s %d)",
                                fullname, strerror(errno), __FILE__, __LINE__);
                  }
               }
               else
               {
                  if (p_afd_status->fd == ON)
                  {
                     int  fd;
                     char delete_fifo[MAX_PATH_LENGTH];

                     (void)strcpy(delete_fifo, p_work_dir);
                     (void)strcat(delete_fifo, FIFO_DIR);
                     (void)strcat(delete_fifo, DELETE_JOBS_FIFO);
                     if ((fd = open(delete_fifo, O_RDWR)) == -1)
                     {
                        (void)xrec(toplevel_w, INFO_DIALOG,
                                   "Failed to open() <%s> : %s (%s %d)",
                                   delete_fifo, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     else
                     {
                        size_t length;

                        length = strlen(qfl[select_list[i] - 1].msg_name) + 1;
                        if (write(fd, qfl[select_list[i] - 1].msg_name, length) != length)
                        {
                           (void)xrec(toplevel_w, INFO_DIALOG,
                                      "Failed to write() to <%s> : %s (%s %d)",
                                      delete_fifo, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        (void)close(fd);
                     }
                  }
                  else /* FD not active, we must delete it. */
                  {
                     /*
                      * Delete the message from the FD queue.
                      */
                     if (k != (*no_msg_queued - 1))
                     {
                        size_t move_size;

                        move_size = (*no_msg_queued - 1 - k) * sizeof(struct queue_buf);
                        (void)memmove(&qb[k], &qb[k + 1], move_size);
                     }
                     (*no_msg_queued)--;
                     fsa[pos].jobs_queued--;
                  }
               }
            }
            else
            {
               deleted = NO;
            }
         }
         else
         {
            deleted = NO;
         }
      }
      else if (qfl[select_list[i] - 1].queue_type == SHOW_UNSENT_OUTPUT)
           {
              /* Don't allow user to delete unsent files. */
              deleted = NO;
           }
           else /* It's in one of the input queue's. */
           {
              if (qfl[select_list[i] - 1].hostname[0] == '\0')
              {
                 (void)sprintf(fullname, "%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].file_name);
              }
              else
              {
                 (void)sprintf(fullname, "%s/.%s/%s",
                               dnb[qfl[select_list[i] - 1].dir_id_pos].dir_name,
                               qfl[select_list[i] - 1].hostname,
                               qfl[select_list[i] - 1].file_name);
              }
              if (unlink(fullname) != -1)
              {
                 deleted = YES;
              }
              else
              {
                 deleted = NO;
              }
           }

      if (deleted == YES)
      {
#ifdef _DELETE_LOG
         int    prog_name_length;
         size_t dl_real_size;

         (void)strcpy(dl.file_name, qfl[select_list[i] - 1].file_name);
         if (qfl[select_list[i] - 1].hostname[0] == '\0')
         {
            (void)sprintf(dl.host_name, "%-*s %x",
                          MAX_HOSTNAME_LENGTH, "-", USER_DEL);
         }
         else
         {
            (void)sprintf(dl.host_name, "%-*s %x",
                          MAX_HOSTNAME_LENGTH, qfl[select_list[i] - 1].hostname,
                          USER_DEL);
         }
         *dl.file_size = qfl[select_list[i] - 1].size;
         *dl.job_number = dnb[qfl[select_list[i] - 1].dir_id_pos].dir_id;
         *dl.file_name_length = strlen(qfl[select_list[i] - 1].file_name);
         prog_name_length = sprintf((dl.file_name + *dl.file_name_length + 1),
                                    "%s show_queue", user);
         dl_real_size = *dl.file_name_length + dl.size + prog_name_length;
         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
         {
            (void)fprintf(stderr, "write() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#endif /* _DELETE_LOG */
         files_deleted++;
      }
      else
      {
         files_not_deleted++;
      }
   } /* for (i = 0; i < no_selected; i++) */

   /*
    * Remove all selected files from the structure queued_file_list.
    */
   skipped = 0;
   for (i = (no_selected - 1); i > -1; i--)
   {
      total_file_size -= qfl[select_list[i] - 1].size;
      if ((i == 0) ||
          (((i - 1) > -1) && (select_list[i - 1] != (select_list[i] - 1))))
      {
         if ((select_list[i] - 1) < total_no_files)
         {
            size_t move_size;

            move_size = (total_no_files - (select_list[i + skipped])) *
                        sizeof(struct queued_file_list);
            (void)memmove(&qfl[select_list[i] - 1],
                          &qfl[select_list[i + skipped]],
                          move_size);
         }
         skipped = 0;
      }
      else
      {
         skipped++;
      }
   }
   total_no_files -= no_selected;

   /*
    * Now remove all selected files list widget.
    */
   XmListDeletePositions(listbox_w, select_list, no_selected);

   if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) == -1)
   {
      (void)xrec(toplevel_w, INFO_DIALOG,
                 "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }

   if (toggles_set & SHOW_OUTPUT)
   {
      if (munmap(((char *)qb - AFD_WORD_OFFSET), qb_size) == -1)
      {
         (void)xrec(toplevel_w, INFO_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      (void)fsa_detach();
      (void)detach_afd_status();
   }

   /* Tell user what we have done. */
   show_summary(total_no_files, total_file_size);
   if ((files_deleted > 0) && (files_not_deleted == 0))
   {
      (void)sprintf(message, "Deleted %d files.", files_deleted);
   }
   else if ((files_deleted > 0) && (files_not_deleted > 0))
        {
           (void)sprintf(message, "Deleted %d files (%d gone).",
                         files_deleted, files_not_deleted);
        }
        else
        {
           (void)sprintf(message, "All %d files already gone.",
                         files_not_deleted);
        }
   show_message(statusbox_w, message);

   return;
}
