/*
 *  get_dc_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dc_data - gets all data out of the DIR_CONFIG for this host
 **
 ** SYNOPSIS
 **   void get_dc_data(void)
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
 **   27.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <time.h>             /* strftime(), gmtime()                    */
#include <stdlib.h>           /* free()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>         /* mmap(), munmap()                        */
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include "view_dc.h"

/* Global variables */
int                   *current_jid_list,
                      no_of_current_jobs;

/* External global variables */
extern int            glyph_height,
                      glyph_width,
                      sys_log_fd,
                      view_passwd;
extern char           host_name[],
                      *p_work_dir;
extern Display        *display;
extern XtAppContext   app;
extern Widget         toplevel_w,
                      text_w;
extern Dimension      button_height;

/* Local global variables */
static int            job_counter,
                      max_x,
                      max_y;
static XmTextPosition *position_array,
                      text_position;

/* Local function prototypes */
static void           show_data(struct job_id_data *, char *);


/*############################ get_dc_data() ############################*/
void
get_dc_data(void)
{
   int                 dnb_fd,
                       i,
                       j,
                       jd_fd,
                       max_vertical_lines,
                       *no_of_job_ids;
   size_t              jid_size = 0,
                       dnb_size = 0;
   char                job_id_data_file[MAX_PATH_LENGTH];
   struct stat         stat_buf;
   struct job_id_data  *jd = NULL;
   struct dir_name_buf *dnb = NULL;

   current_jid_list = NULL;
   no_of_current_jobs = 0;
   max_x = max_y = 0;
   text_position = 0L;
   position_array = NULL;
   job_counter = 0;

   if (get_current_jid_list() == INCORRECT)
   {
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
   }

   /* Map to JID database. */
   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((jd_fd = open(job_id_data_file, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() %s : %s (%s %d)",
                 job_id_data_file, strerror(errno), __FILE__, __LINE__);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to fstat() %s : %s (%s %d)",
                 job_id_data_file, strerror(errno), __FILE__, __LINE__);
      (void)close(jd_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      jid_size = stat_buf.st_size;
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to mmap() to %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Job ID database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(jd_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (close(jd_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Map to directory name buffer. */
   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to open() %s : %s (%s %d)",
                 job_id_data_file, strerror(errno), __FILE__, __LINE__);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "munmap() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (fstat(dnb_fd, &stat_buf) == -1)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Failed to fstat() %s : %s (%s %d)",
                 job_id_data_file, strerror(errno), __FILE__, __LINE__);
      (void)close(dnb_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "munmap() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      dnb_size = stat_buf.st_size;
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to mmap() to %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         if (jid_size > 0)
         {
            if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
            {
               (void)xrec(toplevel_w, ERROR_DIALOG,
                          "munmap() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
            }
         }
         return;
      }
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   else
   {
      (void)xrec(toplevel_w, ERROR_DIALOG,
                 "Job ID database file is empty. (%s %d)",
                 __FILE__, __LINE__);
      (void)close(dnb_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "munmap() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (close(dnb_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Go through current job list and search the JID structure for
    * the host we are looking for.
    */
   for (i = 0; i < no_of_current_jobs; i++)
   {
      for (j = 0; j < *no_of_job_ids; j++)
      {
         if (jd[j].job_id == current_jid_list[i])
         {
            if (strcmp(jd[j].host_alias, host_name) == 0)
            {
               show_data(&jd[j], dnb[jd[j].dir_id_pos].dir_name);
            }
            break;
         }
      }
   }

   if (job_counter > 0)
   {
      char *separator_line;

      if ((separator_line = malloc(max_x + 3)) == NULL)
      {
         (void)xrec(toplevel_w, WARN_DIALOG,
                    "malloc() error for separator line : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         XmTextPosition offset = 0L;

         separator_line[0] = '\n';
         (void)memset(&separator_line[1], '-', max_x);
         separator_line[max_x] = '\n';
         separator_line[max_x + 1] = '\0';

         for (i = 0; i < (job_counter - 1); i++)
         {
            XmTextInsert(text_w, position_array[i] + offset, separator_line);
            offset += max_x + 1;
            max_y++;
         }

         free(separator_line);
      }
      free((void *)position_array);
   }

   /* Resize window to fit text. */
   max_vertical_lines = (8 * (DisplayHeight(display, DefaultScreen(display)) /
                        glyph_height)) / 10;
   if (max_y > max_vertical_lines)
   {
      max_y = max_vertical_lines;
   }
   XResizeWindow(display, XtWindow(toplevel_w),
                 glyph_width * (max_x + 3 + 3),
                 (glyph_height * (max_y + 5)) + button_height);
   XtVaSetValues(text_w,
                 XmNcolumns, max_x,
                 XmNrows,    max_y,
                 NULL);

   /* Free all memory that was allocated or mapped. */
   if (current_jid_list != NULL)
   {
      free(current_jid_list);
   }
   if (dnb_size > 0)
   {
      if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) < 0)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }
   if (jid_size > 0)
   {
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ show_data() ++++++++++++++++++++++++++++++*/
static void
show_data(struct job_id_data *p_jd, char *dir_name)
{
   int  count,
        i,
        length;
   char text[8192],
        value[MAX_PATH_LENGTH];

   if ((job_counter % 10) == 0)
   {
      size_t new_size;

      new_size = ((job_counter / 10) + 1) * 10 * sizeof(XmTextPosition);
      if ((position_array = realloc(position_array, new_size)) == NULL)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "realloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
   }

   length = count = sprintf(text, "Directory  : %s\n", dir_name);
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   /* Show file filters. */
   count = sprintf(text + length, "Filter     : %s\n", p_jd->file_list[0]);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;
   for (i = 1; i < p_jd->no_of_files; i++)
   {
      count = sprintf(text + length, "             %s\n", p_jd->file_list[i]);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
   }

   /* Print recipient */
   (void)strcpy(value, p_jd->recipient);
   if (view_passwd != YES)
   {
      char *ptr = value;

      /*
       * The user may not see the password. Lets cut it out and
       * replace it with DEFAULT_VIEW_PASSWD.
       */
      while (*ptr != ':')
      {
         ptr++;
      }
      ptr++;
      while ((*ptr != ':') && (*ptr != '@'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
      if (*ptr == ':')
      {
         char *p_end = ptr + 1,
              tmp_buffer[MAX_RECIPIENT_LENGTH];

         ptr++;
         while (*ptr != '@')
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         (void)strcpy(tmp_buffer, ptr);
         *p_end = '\0';
         (void)strcat(value, "XXXXX");
         (void)strcat(value, tmp_buffer);
      }
   }
   count = sprintf(text + length, "Recipient  : %s\n", value);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   /* Show all AMG options */
   if (p_jd->no_of_loptions > 0)
   {
      char *ptr = p_jd->loptions;

      count = sprintf(text + length, "AMG-options: %s\n", ptr);
      NEXT(ptr);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      for (i = 1; i < p_jd->no_of_loptions; i++)
      {
         count = sprintf(text + length, "             %s\n", ptr);
         NEXT(ptr);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
      }
   }

   /* Show all FD options */
   if (p_jd->no_of_soptions > 0)
   {
      int  counter = 0;
      char *ptr = p_jd->soptions;

      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         value[counter] = *ptr;
         ptr++; counter++;
      }
      value[counter] = '\0';
      ptr++;
      count = sprintf(text + length, "FD-options : %s\n", value);
      length += count;
      if (count > max_x)
      {
         max_x = count;
      }
      max_y++;
      for (i = 1; i < p_jd->no_of_soptions; i++)
      {
         counter = 0;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            value[counter] = *ptr;
            ptr++; counter++;
         }
         value[counter] = '\0';
         count = sprintf(text + length, "             %s\n", value);
         length += count;
         if (count > max_x)
         {
            max_x = count;
         }
         max_y++;
         if (ptr == '\0')
         {
            break;
         }
         else
         {
            ptr++;
         }
      }
   }

   /* Priority */
   count = sprintf(text + length, "Priority   : %c\n", p_jd->priority);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   /* Job ID */
   count = sprintf(text + length, "Job-ID     : %d", p_jd->job_id);
   length += count;
   if (count > max_x)
   {
      max_x = count;
   }
   max_y++;

   XmTextInsert(text_w, text_position, text);
   text_position += length;
   position_array[job_counter] = text_position;
   job_counter++;

   return;
}
