/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2001 Deutscher Wetterdienst (DWD),
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
 **   get_info - retrieves information out of the AMG history file
 **
 ** SYNOPSIS
 **   void get_info(int item)
 **   int  get_sum_data(int    item,
 **                     time_t *date,
 **                     double *file_size,
 **                     double *trans_time)
 **
 ** DESCRIPTION
 **   This function get_info() searches the AMG history file for the
 **   job number of the selected file item. It then fills the structures
 **   item_list and info_data with data from the AMG history file.
 **
 ** RETURN VALUES
 **   None. The function will exit() with INCORRECT when it fails to
 **   allocate memory or fails to seek in the AMG history file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.04.1997 H.Kiehl Created
 **   14.01.1998 H.Kiehl Support for remote file name.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* strtoul()                       */
#include <unistd.h>                   /* close()                         */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "show_olog.h"
#include "afd_ctrl.h"

/* External global variables. */
extern Widget              toplevel_w;
extern int                 no_of_log_files,
                           sys_log_fd;
extern char                *p_work_dir;
extern struct item_list    *il;
extern struct info_data    id;

/* Local variables. */
static int                 *no_of_job_ids;
static struct job_id_data  *jd = NULL;
static struct dir_name_buf *dnb = NULL;

/* Local function prototypes. */
static int                 get_all(int);
static void                get_job_data(struct job_id_data *);


/*############################### get_info() ############################*/
void
get_info(int item)
{
   int i;

   if ((item != GOT_JOB_ID) && (item != GOT_JOB_ID_DIR_ONLY) &&
       (item != GOT_JOB_ID_USER_ONLY))
   {
      id.job_no = get_all(item - 1);
   }

   /*
    * Go through job ID database and find the job ID.
    */
   if (jd == NULL)
   {
      int         fd;
      char        job_id_data_file[MAX_PATH_LENGTH];
      struct stat stat_buf;

      /* Map to job ID data file. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
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
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
         no_of_job_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(fd);
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(fd);
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
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
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(fd);
            return;
         }
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
   }

   /* Search through all history files. */
   for (i = 0; i < *no_of_job_ids; i++)
   {
      if (id.job_no == jd[i].job_id)
      {
         if (item == GOT_JOB_ID_DIR_ONLY)
         {
            (void)strcpy(id.dir, dnb[jd[i].dir_id_pos].dir_name);
         }
         else if (item == GOT_JOB_ID_USER_ONLY)
              {
                 char *ptr = jd[i].recipient;

                 while ((*ptr != '/') && (*ptr != '\0'))
                 {
                    if (*ptr == '\\')
                    {
                       ptr++;
                    }
                    ptr++;
                 }
                 if ((*ptr == '/') && (*(ptr + 1) == '/'))
                 {
                    int count = 0;

                    ptr += 2;
                    while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                    {
                       if (*ptr == '\\')
                       {
                          ptr++;
                       }
                       id.user[count] = id.mail_destination[count] = *ptr;
                       ptr++; count++;
                    }
                    id.user[count] = ' '; /* for sfilter() */
                    id.user[count + 1] = '\0';

                    /*
                     * Need to check if server= option is set so we can
                     * get the full mail address.
                     */
                    if (*ptr == ':') /* Omit the password. */
                    {
                       while ((*ptr != '@') && (*ptr != '\0'))
                       {
                          if (*ptr == '\\')
                          {
                             ptr++;
                          }
                          ptr++;
                       }
                    }
                    if (*ptr == '@')
                    {
                       id.mail_destination[count] = *ptr;
                       ptr++; count++;
                       while ((*ptr != ';') && (*ptr != ':') &&
                              (*ptr != '/') && (*ptr != '\0'))
                       {
                          if (*ptr == '\\')
                          {
                             ptr++;
                          }
                          id.mail_destination[count] = *ptr;
                          ptr++; count++;
                       }
                       while ((*ptr != ';') && (*ptr != '\0'))
                       {
                          if (*ptr == '\\')
                          {
                             ptr++;
                          }
                          ptr++;
                       }
                       if ((*ptr == ';') && (*(ptr + 1) == 's') &&
                           (*(ptr + 2) == 'e') && (*(ptr + 3) == 'r') &&
                           (*(ptr + 4) == 'v') && (*(ptr + 5) == 'e') &&
                           (*(ptr + 6) == 'r') && (*(ptr + 7) == '='))
                       {
                          id.mail_destination[count] = ' '; /* for sfilter() */
                          id.mail_destination[count + 1] = '\0';
                       }
                       else
                       {
                          id.mail_destination[0] = '\0';
                       }
                    }
                    else
                    {
                       id.mail_destination[0] = '\0';
                    }
                 }
              }
              else
              {
                 get_job_data(&jd[i]);
              }

         return;
      }
   }

   return;
}


/*############################ get_sum_data() ###########################*/
int
get_sum_data(int item, time_t *date, double *file_size, double *trans_time)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the date, file size and transfer time. */
   if (pos > -1)
   {
      int  i = 0;
      char *ptr,
           *p_start,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos]),
                SEEK_SET) == -1)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(toplevel_w, WARN_DIALOG,
                    "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = buffer;
      while ((*ptr != ' ') && (i < 11))
      {
         ptr++; i++;
      }
      *ptr = '\0';
      *date = atol(buffer);

      /* Ignore file name */
      ptr = &buffer[11 + MAX_HOSTNAME_LENGTH + 3];
      while (*ptr != ' ')
      {
         ptr++;
      }
      if (*(ptr + 1) == '/')
      {
         ptr++;
         while (*ptr != ' ')
         {
            ptr++;
         }
      }
      ptr++;

      /* Get file size */
      p_start = ptr;
      while (*ptr != ' ')
      {
         ptr++;
      }
      *ptr = '\0';
      *file_size = strtod(p_start, NULL);
      ptr++;

      /* Get transfer time */
      p_start = ptr;
      while (*ptr != ' ')
      {
         ptr++;
      }
      *ptr = '\0';
      *trans_time = strtod(p_start, NULL);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ get_all() +++++++++++++++++++++++++++++*/
/*                                ---------                              */
/* Description: Retrieves the full local file name, remote file name (if */
/*              it exists), job number and if available the archive      */
/*              directory out of the log file.                           */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_all(int item)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the job ID and file name. */
   if (pos > -1)
   {
      int  i = 0;
      char *ptr,
           *p_job_id,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(toplevel_w, WARN_DIALOG,
                    "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }

      /* Store the date. */
      ptr = buffer;
      while ((*ptr != ' ') && (i < 11))
      {
         ptr++; i++;
      }
      *ptr = '\0';
      id.date_send = atol(buffer);

      /* Store local file name. */
      ptr = &buffer[11 + MAX_HOSTNAME_LENGTH + 3];
      i = 0;
      while (*ptr != ' ')
      {
         id.local_file_name[i] = *ptr;
         i++; ptr++;
      }
      id.local_file_name[i] = '\0';
      ptr++;

      /* Store remote file name. */
      i = 0;
      if (*ptr == '/')
      {
         ptr++;
         while (*ptr != ' ')
         {
            id.remote_file_name[i] = *ptr;
            i++; ptr++;
         }
         ptr++;
      }
      id.remote_file_name[i] = '\0';

      /* Away with the file size. */
      i = 0;
      while (*ptr != ' ')
      {
         if (i < (MAX_INT_LENGTH + MAX_INT_LENGTH))
         {
            id.file_size[i] = *ptr;
            i++;
         }
         ptr++;
      }
      id.file_size[i] = '\0';
      ptr++;

      /* Away with the transfer time. */
      i = 0;
      while (*ptr != ' ')
      {
         if (i < (MAX_INT_LENGTH + MAX_INT_LENGTH))
         {
            id.trans_time[i] = *ptr;
            i++;
         }
         ptr++;
      }
      id.trans_time[i] = '\0';
      ptr++;

      /* Get the job ID. */
      p_job_id = ptr;
      while ((*ptr != '\n') && (*ptr != ' '))
      {
         ptr++;
      }
      if (*ptr == ' ')
      {
         *ptr = '\0';
         ptr++;

         i = 0;
         while (*ptr != '\n')
         {
            id.archive_dir[i] = *ptr;
            i++; ptr++;
         }
         id.archive_dir[i] = '\0';
      }
      else
      {
         *ptr = '\0';
      }

      return((unsigned int)strtoul(p_job_id, NULL, 10));
   }
   else
   {
      return(0);
   }
}


/*++++++++++++++++++++++++++++ get_job_data() +++++++++++++++++++++++++++*/
/*                             --------------                            */
/* Descriptions: This function gets all data that was in the             */
/*               AMG history file and copies them into the global        */
/*               structure 'info_data'.                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_job_data(struct job_id_data *p_jd)
{
   register int   i;
   register char  *p_file,
                  *p_tmp;

   (void)strcpy(id.dir, dnb[p_jd->dir_id_pos].dir_name);

   get_dir_options(p_jd->dir_id_pos, &id.d_o);

   id.priority = p_jd->priority;
   id.no_of_files = p_jd->no_of_files;
   p_file = p_jd->file_list;
   if ((id.files = malloc((id.no_of_files * sizeof(char *)))) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Save all file/filter names */
   for (i = 0; i < id.no_of_files; i++)
   {
      id.files[i] = p_file;
      NEXT(p_file);
   }

   id.no_of_loptions = p_jd->no_of_loptions;

   /* Save all AMG (local) options. */
   if (id.no_of_loptions > 0)
   {
#ifdef _WITH_DYNAMIC_MEMORY
      RT_ARRAY(id.loptions, id.no_of_loptions, MAX_OPTION_LENGTH, char);
#endif
      p_tmp = p_jd->loptions;
      for (i = 0; i < id.no_of_loptions; i++)
      {
         (void)strcpy(id.loptions[i], p_tmp);
         NEXT(p_tmp);
      }
   }

   id.no_of_soptions = p_jd->no_of_soptions;

   /* Save all FD (standart) options. */
   if (id.no_of_soptions > 0)
   {
      size_t size;

      size = strlen(p_jd->soptions);
      if ((id.soptions = calloc(size + 1, sizeof(char))) == NULL)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      (void)memcpy(id.soptions, p_jd->soptions, size);
      id.soptions[size] = '\0';
   }
   else
   {
      id.soptions = NULL;
   }

   (void)strcpy(id.recipient, p_jd->recipient);

   return;
}
