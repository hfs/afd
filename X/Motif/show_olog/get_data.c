/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_data - searches output log files for data
 **
 ** SYNOPSIS
 **   void get_data(void)
 **
 ** DESCRIPTION
 **   This function searches for the selected data in the output
 **   log file of the AFD. The following things can be selected:
 **   start & end time, file name, file length, directory and
 **   recipient. Only selected data will be shown in the list
 **   widget.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.04.1997 H.Kiehl Created
 **   14.01.1998 H.Kiehl Support for remote file name.
 **   13.02.1999 H.Kiehl Multiple recipients.
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtod()       */
#include <time.h>         /* time()                                      */
#include <unistd.h>       /* close()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _NO_MMAP
#include <sys/mman.h>     /* mmap(), munmap()                            */
#ifndef MAP_FILE          /* Required for BSD          */
#define MAP_FILE 0        /* All others do not need it */
#endif
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include "logdefs.h"
#include "afd_ctrl.h"
#include "show_olog.h"

/* External global variables */
extern Display          *display;
extern Widget           listbox_w,
                        scrollbar_w,
                        statusbox_w,
                        special_button_w,
                        summarybox_w,
                        toplevel_w;
extern int              file_name_toggle_set,
                        toggles_set,
                        radio_set,
                        gt_lt_sign,
                        no_of_search_hosts,
                        special_button_flag,
                        file_name_length,
                        no_of_log_files;
extern size_t           search_file_size;
extern time_t           start_time_val,
                        end_time_val;
extern char             *p_work_dir,
                        search_file_name[],
                        search_directory_name[],
                        **search_recipient,
                        **search_user,
                        summary_str[],
                        total_summary_str[];
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Local global variables. */
static unsigned int     total_no_files;
static time_t           local_start_time,
                        local_end_time,
                        first_date_found;
static double           file_size,
                        trans_time;
static char             *p_file_name,
                        *p_host_name,
                        *p_type,
                        *p_file_size,
                        *p_tt,
                        *p_archive_flag,
                        log_file[MAX_PATH_LENGTH],
                        *p_log_file,
                        line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 1];
static XmStringTable    str_list;

/* Local function prototypes */
static char   *search_time(char *, time_t, time_t, time_t, size_t);
static void   display_data(int, time_t, time_t),
              extract_data(char *, int),
              no_criteria(register char *, char *, int, char *),
              file_name_only(register char *, char *, int, char *),
              file_size_only(register char *, char *, int, char *),
              file_name_and_size(register char *, char *, int, char *),
              recipient_only(register char *, char *, int, char *),
              file_name_and_recipient(register char *, char *, int, char *),
              file_size_and_recipient(register char *, char *, int, char *),
              file_name_size_recipient(register char *, char *, int, char *);

#define REALLOC_OFFSET_BUFFER()                                   \
        {                                                         \
           if (((loops + 1) * LINES_BUFFERED - item_counter) <= LINES_BUFFERED) \
           {                                                      \
              int new_int_size,                                   \
                  new_char_size;                                  \
                                                                  \
              new_char_size = (loops + 1) * LINES_BUFFERED;       \
              new_int_size = new_char_size * sizeof(int);         \
                                                                  \
              if (((il[file_no].offset = realloc(il[file_no].offset, new_int_size)) == NULL) ||           \
                  ((il[file_no].line_offset = realloc(il[file_no].line_offset, new_int_size)) == NULL) || \
                  ((il[file_no].archived = realloc(il[file_no].archived, new_char_size)) == NULL))        \
              {                                                   \
                 (void)xrec(toplevel_w, FATAL_DIALOG,             \
                            "realloc() error : %s (%s %d)",       \
                            strerror(errno), __FILE__, __LINE__); \
                 return;                                          \
              }                                                   \
           }                                                      \
        }
#define IGNORE_ENTRY()            \
        {                         \
           while (*ptr != '\n')   \
              ptr++;              \
           ptr++; i--;            \
           continue;              \
        }
#define CONVERT_TIME()                                \
        {                                             \
           line[0] = ((p_ts->tm_mon + 1) / 10) + '0'; \
           line[1] = ((p_ts->tm_mon + 1) % 10) + '0'; \
           line[2] = '.';                             \
           line[3] = (p_ts->tm_mday / 10) + '0';      \
           line[4] = (p_ts->tm_mday % 10) + '0';      \
           line[5] = '.';                             \
           line[7] = (p_ts->tm_hour / 10) + '0';      \
           line[8] = (p_ts->tm_hour % 10) + '0';      \
           line[9] = ':';                             \
           line[10] = (p_ts->tm_min / 10) + '0';      \
           line[11] = (p_ts->tm_min % 10) + '0';      \
           line[12] = ':';                            \
           line[13] = (p_ts->tm_sec / 10) + '0';      \
           line[14] = (p_ts->tm_sec % 10) + '0';      \
        }
#define SET_FILE_NAME_POINTER()                         \
        {                                               \
           ptr += 11 + MAX_HOSTNAME_LENGTH + 3;         \
           if (file_name_toggle_set == REMOTE_FILENAME) \
           {                                            \
              tmp_ptr = ptr;                            \
              while (*tmp_ptr != ' ')                   \
              {                                         \
                 tmp_ptr++;                             \
              }                                         \
              if (*(tmp_ptr + 1) == '/')                \
              {                                         \
                 ptr = tmp_ptr + 2;                     \
              }                                         \
           }                                            \
        }
#define INSERT_TIME_TYPE(protocol)                         \
        {                                                  \
           (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
           (void)memcpy(time_buf, ptr_start_line, 10);     \
           time_buf[10] = '\0';                            \
           time_when_transmitted = (time_t)atol(time_buf); \
           if (first_date_found == -1)                     \
           {                                               \
              first_date_found = time_when_transmitted;    \
           }                                               \
           p_ts = gmtime(&time_when_transmitted);          \
           CONVERT_TIME();                                 \
           (void)memcpy(p_type, (protocol), 4);            \
        }
#define COMMON_BLOCK()                                                 \
        {                                                              \
           ptr++;                                                      \
           while (*ptr != ' ')                                         \
           {                                                           \
              ptr++;                                                   \
           }                                                           \
           tmp_ptr = ptr - 1;                                          \
           j = 0;                                                      \
           while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_TRANSFER_TIME)) \
           {                                                           \
              *(p_tt - j) = *tmp_ptr;                                  \
              tmp_ptr--; j++;                                          \
           }                                                           \
           if (j == MAX_DISPLAYED_TRANSFER_TIME)                       \
           {                                                           \
              tmp_ptr = ptr - 4;                                       \
              j = 0;                                                   \
              while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_TRANSFER_TIME)) \
              {                                                        \
                 *(p_tt - j) = *tmp_ptr;                               \
                 tmp_ptr--; j++;                                       \
              }                                                        \
              if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != ' ')) \
              {                                                        \
                 *(p_tt - j) = '>';                                    \
                 while (*tmp_ptr != ' ')                               \
                 {                                                     \
                    tmp_ptr--;                                         \
                 }                                                     \
              }                                                        \
              else                                                     \
              {                                                        \
                 while (j < MAX_DISPLAYED_TRANSFER_TIME)               \
                 {                                                     \
                    *(p_tt - j) = ' ';                                 \
                    j++;                                               \
                 }                                                     \
              }                                                        \
           }                                                           \
           tmp_ptr++;                                                  \
                                                                       \
           ptr++;                                                      \
           il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file);\
                                                                       \
           if ((search_directory_name[0] != '\0') ||                   \
               ((current_search_host != -1) &&                         \
                (search_user[current_search_host][0] != '\0')))        \
           {                                                           \
              int  count = 0;                                          \
              char job_id_str[15];                                     \
                                                                       \
              while ((*ptr != '\n') && (*ptr != ' ') && (count < 15))  \
              {                                                        \
                 job_id_str[count] = *ptr;                             \
                 count++; ptr++;                                       \
              }                                                        \
              job_id_str[count] = '\0';                                \
              id.job_no = (unsigned int)strtoul(job_id_str, NULL, 10); \
                                                                       \
              if ((current_search_host != -1) &&                       \
                  (search_user[current_search_host][0] != '\0'))       \
              {                                                        \
                 id.user[0] = '\0';                                    \
                 get_info(GOT_JOB_ID_USER_ONLY);                       \
                                                                       \
                 if (sfilter(search_user[current_search_host], id.user) != 0) \
                 {                                                     \
                    IGNORE_ENTRY();                                    \
                 }                                                     \
              }                                                        \
              if (search_directory_name[0] != '\0')                    \
              {                                                        \
                 id.dir[0] = '\0';                                     \
                 get_info(GOT_JOB_ID_DIR_ONLY);                        \
                 count = strlen(id.dir);                               \
                 id.dir[count] = ' ';                                  \
                 id.dir[count + 1] = '\0';                             \
                                                                       \
                 if (sfilter(search_directory_name, id.dir) != 0)      \
                 {                                                     \
                    IGNORE_ENTRY();                                    \
                 }                                                     \
              }                                                        \
           }                                                           \
           else                                                        \
           {                                                           \
              while ((*ptr != '\n') && (*ptr != ' '))                  \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
           }                                                           \
           trans_time += strtod(tmp_ptr, NULL);                        \
                                                                       \
           if (*ptr == ' ')                                            \
           {                                                           \
              *p_archive_flag = 'Y';                                   \
              il[file_no].archived[item_counter] = 1;                  \
              while (*ptr != '\n')                                     \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
           }                                                           \
           else                                                        \
           {                                                           \
              *p_archive_flag = 'N';                                   \
           }                                                           \
           item_counter++;                                             \
                                                                       \
           str_list[i] = XmStringCreateLocalized(line);                \
                                                                       \
           ptr++;                                                      \
        }

#define CHECK_LIST_LIMIT()                                          \
        {                                                           \
           if ((perm.list_limit > 0) && (item_counter > perm.list_limit)) \
           {                                                        \
              char msg_buffer[50];                                  \
                                                                    \
              (void)sprintf(msg_buffer, "List limit (%d) reached!", \
                            perm.list_limit);                       \
              show_message(statusbox_w, msg_buffer);                \
              break;                                                \
           }                                                        \
        }

#define FILE_SIZE_AND_RECIPIENT(id_string)                 \
        {                                                  \
           int ii;                                         \
                                                           \
           for (ii = 0; ii < no_of_search_hosts; ii++)     \
           {                                               \
              if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0) \
              {                                            \
                 current_search_host = ii;                 \
                 break;                                    \
              }                                            \
           }                                               \
           if (current_search_host != -1)                  \
           {                                               \
              ptr += 11 + MAX_HOSTNAME_LENGTH + 3;         \
              while (*ptr != ' ')                          \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
              if (*ptr == '/')                             \
              {                                            \
                 /* Ignore  the remote file name */        \
                 while (*ptr != ' ')                       \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 ptr++;                                    \
              }                                            \
                                                           \
              tmp_file_size = strtod(ptr, NULL);           \
              if ((gt_lt_sign == EQUAL_SIGN) &&            \
                  (tmp_file_size == search_file_size))     \
              {                                            \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
                 (void)memcpy(p_type, (id_string), 4);     \
                                                           \
                 /* Write file size. */                    \
                 while (*ptr != ' ')                       \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 tmp_ptr = ptr - 1;                        \
                 j = 0;                                    \
                 while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
                 {                                         \
                    *(p_file_size - j) = *tmp_ptr;         \
                    tmp_ptr--; j++;                        \
                 }                                         \
                 if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
                 {                                         \
                    *(p_file_size - j) = '>';              \
                 }                                         \
              }                                            \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&   \
                       (tmp_file_size < search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
                      (void)memcpy(p_type, (id_string), 4);\
                                                           \
                      /* Write file size. */               \
                      while (*ptr != ' ')                  \
                      {                                    \
                         ptr++;                            \
                      }                                    \
                      tmp_ptr = ptr - 1;                   \
                      j = 0;                               \
                      while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
                      {                                    \
                         *(p_file_size - j) = *tmp_ptr;    \
                         tmp_ptr--; j++;                   \
                      }                                    \
                      if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
                      {                                    \
                         *(p_file_size - j) = '>';         \
                      }                                    \
                   }                                       \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&\
                       (tmp_file_size > search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
                      (void)memcpy(p_type, (id_string), 4);\
                                                           \
                      /* Write file size. */               \
                      while (*ptr != ' ')                  \
                      {                                    \
                         ptr++;                            \
                      }                                    \
                      tmp_ptr = ptr - 1;                   \
                      j = 0;                               \
                      while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
                      {                                    \
                         *(p_file_size - j) = *tmp_ptr;    \
                         tmp_ptr--; j++;                   \
                      }                                    \
                      if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
                      {                                    \
                         *(p_file_size - j) = '>';         \
                      }                                    \
                   }                                       \
                   else                                    \
                   {                                       \
                      IGNORE_ENTRY();                      \
                   }                                       \
           }                                               \
           else                                            \
           {                                               \
              IGNORE_ENTRY();                              \
           }                                               \
        }

#define FILE_NAME_AND_RECIPIENT(toggle_id, id_string)                  \
        {                                                              \
            if (toggles_set & (toggle_id))                             \
            {                                                          \
               int ii;                                                 \
                                                                       \
               for (ii = 0; ii < no_of_search_hosts; ii++)             \
               {                                                       \
                  if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0) \
                  {                                                    \
                     current_search_host = ii;                         \
                     break;                                            \
                  }                                                    \
               }                                                       \
               if (current_search_host != -1)                          \
               {                                                       \
                  SET_FILE_NAME_POINTER();                             \
                  if (sfilter(search_file_name, ptr) == 0)             \
                  {                                                    \
                     il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file); \
                     INSERT_TIME_TYPE((id_string));                    \
                     j = 0;                                            \
                     while ((*ptr != ' ') && (j < file_name_length))   \
                     {                                                 \
                        *(p_file_name + j) = *ptr;                     \
                        ptr++; j++;                                    \
                     }                                                 \
                  }                                                    \
                  else                                                 \
                  {                                                    \
                     IGNORE_ENTRY();                                   \
                  }                                                    \
               }                                                       \
               else                                                    \
               {                                                       \
                  IGNORE_ENTRY();                                      \
               }                                                       \
            }                                                          \
            else                                                       \
            {                                                          \
               IGNORE_ENTRY();                                         \
            }                                                          \
        }

#define FILE_SIZE_ONLY(id_string)                                      \
        {                                                              \
           ptr += 11 + MAX_HOSTNAME_LENGTH + 3;                        \
           while (*ptr != ' ')                                         \
           {                                                           \
              ptr++;                                                   \
           }                                                           \
           ptr++;                                                      \
           if (*ptr == '/')                                            \
           {                                                           \
              /* Ignore  the remote file name */                       \
              while (*ptr != ' ')                                      \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
              ptr++;                                                   \
           }                                                           \
                                                                       \
           tmp_file_size = strtod(ptr, NULL);                          \
           if ((gt_lt_sign == EQUAL_SIGN) &&                           \
               (tmp_file_size == search_file_size))                    \
           {                                                           \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
              (void)memcpy(p_type, (id_string), 4);                    \
                                                                       \
              /* Write file size. */                                   \
              while (*ptr != ' ')                                      \
              {                                                        \
                 ptr++;                                                \
              }                                                        \
              tmp_ptr = ptr - 1;                                       \
              j = 0;                                                   \
              while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
              {                                                        \
                 *(p_file_size - j) = *tmp_ptr;                        \
                 tmp_ptr--; j++;                                       \
              }                                                        \
              if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
              {                                                        \
                 *(p_file_size - j) = '>';                             \
              }                                                        \
           }                                                           \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&                  \
                    (tmp_file_size < search_file_size))                \
                {                                                      \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
                   (void)memcpy(p_type, (id_string), 4);               \
                                                                       \
                   /* Write file size. */                              \
                   while (*ptr != ' ')                                 \
                   {                                                   \
                      ptr++;                                           \
                   }                                                   \
                   tmp_ptr = ptr - 1;                                  \
                   j = 0;                                              \
                   while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
                   {                                                   \
                      *(p_file_size - j) = *tmp_ptr;                   \
                      tmp_ptr--; j++;                                  \
                   }                                                   \
                   if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
                   {                                                   \
                      *(p_file_size - j) = '>';                        \
                   }                                                   \
                }                                                      \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&               \
                    (tmp_file_size > search_file_size))                \
                {                                                      \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length); \
                   (void)memcpy(p_type, (id_string), 4);               \
                                                                       \
                   /* Write file size. */                              \
                   while (*ptr != ' ')                                 \
                   {                                                   \
                      ptr++;                                           \
                   }                                                   \
                   tmp_ptr = ptr - 1;                                  \
                   j = 0;                                              \
                   while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE)) \
                   {                                                   \
                      *(p_file_size - j) = *tmp_ptr;                   \
                      tmp_ptr--; j++;                                  \
                   }                                                   \
                   if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' ')) \
                   {                                                   \
                      *(p_file_size - j) = '>';                        \
                   }                                                   \
                }                                                      \
                else                                                   \
                {                                                      \
                   IGNORE_ENTRY();                                     \
                }                                                      \
        }


/*############################### get_data() ############################*/
void
get_data(void)
{
#ifdef _OUTPUT_LOG
   int          i,
                j,
                start_file_no = -1,
                end_file_no = -1;
   double       total_file_size,
                total_trans_time;
   char         status_message[MAX_MESSAGE_LENGTH];
   struct stat  stat_buf;
   XmString     xstr;

   /* Prepare log file name. */
   p_log_file = log_file;
   no_of_log_files = MAX_OUTPUT_LOG_FILES;
   p_log_file += sprintf(log_file, "%s%s/%s", p_work_dir, LOG_DIR,
                         OUTPUT_BUFFER_FILE);

   local_start_time = start_time_val;
   local_end_time   = end_time_val;

   for (i = 0; i < no_of_log_files; i++)
   {
      (void)sprintf(p_log_file, "%d", i);
      if (stat(log_file, &stat_buf) == 0)
      {
         if (((stat_buf.st_mtime + SWITCH_FILE_TIME) >= local_start_time) ||
             (start_file_no == -1))
         {
            start_file_no = i;
         }
         if (local_end_time == -1)
         {
            if (end_file_no == -1)
            {
               end_file_no = i;
            }
         }
         else if ((stat_buf.st_mtime >= local_end_time) || (end_file_no == -1))
              {
                 end_file_no = i;
              }
      }
   }

   if ((str_list = (XmStringTable)XtMalloc(LINES_BUFFERED * sizeof(XmString))) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Allocate memory for item list */
   no_of_log_files = start_file_no - end_file_no + 1;
   if ((il = (struct item_list *)malloc((no_of_log_files + 1) * sizeof(struct item_list))) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
   for (i = 0; i < no_of_log_files; i++)
   {
      il[i].fp = NULL;
      il[i].no_of_items = 0;
      il[i].offset = NULL;
      il[i].line_offset = NULL;
      il[i].archived = NULL;
   }

   /* Initialise all pointer in line. */
   p_file_name    = line + 16;
   p_host_name    = p_file_name + file_name_length + 1;
   p_type         = p_host_name + MAX_HOSTNAME_LENGTH + 1;
   p_file_size    = p_type + 5 + MAX_DISPLAYED_FILE_SIZE;
   p_tt           = p_file_size + 1 + MAX_DISPLAYED_TRANSFER_TIME;
   p_archive_flag = p_tt + 2;
   *(line + MAX_OUTPUT_LINE_LENGTH + file_name_length) = '\0';

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   summary_str[0] = '\0';
   XmTextSetString(summarybox_w, summary_str);
   (void)strcpy(status_message, "Searching  -");
   SHOW_MESSAGE();
   CHECK_INTERRUPT();

   file_size = trans_time = 0.0;
   total_no_files = 0;
   first_date_found = -1;
   j = 0;
   for (i = start_file_no;
        ((i >= end_file_no) && (special_button_flag != STOP_BUTTON_PRESSED));
        i--, j++)
   {
      (void)sprintf(p_log_file, "%d", i);
      (void)extract_data(log_file, j);
      total_file_size += file_size;
      total_trans_time += trans_time;
   }

   if (total_no_files != 0)
   {
      (void)strcpy(status_message, " ");
   }
   else
   {
      (void)strcpy(status_message, "No data found.");
   }
   SHOW_MESSAGE();

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   XtFree((char *)str_list);

#endif /* _OUTPUT_LOG */
   return;
}


/*+++++++++++++++++++++++++++++ extract_data() ++++++++++++++++++++++++++*/
static void
extract_data(char *current_log_file, int file_no)
{
   int           fd;
   time_t        time_val,
                 earliest_entry,
                 latest_entry;
   register char *ptr,
                 *ptr_start;
   char          *src,
                 *tmp_ptr,
                 *ptr_end,
                 time_buf[MAX_INT_LENGTH];
   struct stat   stat_buf;

   /* Check if file is there and get its size. */
   if (stat(current_log_file, &stat_buf) < 0)
   {
      if (errno == ENOENT)
      {
         /* For some reason the file is not there. So lets */
         /* assume we have found nothing.                  */
         return;
      }
      else
      {
         (void)xrec(toplevel_w, WARN_DIALOG, "Failed to stat() %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
   }

   /* Make sure there is data in the log file. */
   if (stat_buf.st_size == 0)
   {
      return;
   }

   if ((fd = open(current_log_file, O_RDONLY)) == -1)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if ((il[file_no].fp = fdopen(fd, "r")) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "fdopen() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef _NO_MMAP
   if ((src = (char *)malloc(stat_buf.st_size)) == NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
   if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG,
                 "Failed to read() from %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      free(src);
      (void)close(fd);
      return;
   }
#else
   if ((src = mmap(0, stat_buf.st_size, PROT_READ,
                   (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#endif

   /*
    * Now we have the source data in the buffer 'src'. Lets
    * search for the stuff the user wants to see. First
    * collect this data in the buffer 'dst' and only after
    * we have found all data will 'dst' be displayed. The
    * destination buffer 'dst' will be realloced in sizes
    * 10 * MAX_OUTPUT_LINE_LENGTH characters. Hope this will reduce
    * memory usage a little bit.
    */

   /* Get latest entry. */
   tmp_ptr = src + stat_buf.st_size - 2;
   while ((*tmp_ptr != '\n') && (src != (tmp_ptr - 1)))
   {
      tmp_ptr--;
   }
   if (*tmp_ptr == '\n')
   {
      ptr = tmp_ptr + 1;
   }
   else
   {
      ptr = tmp_ptr;
   }
   GET_TIME();
   latest_entry = time_val;

   /* Get earliest entry. */
   ptr = src;
   GET_TIME();
   earliest_entry = time_val;

   if (local_start_time == -1)
   {
      ptr_start = src;

      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
   }
   else
   {
      /*
       * Search for the first entry of 'local_start_time'. Get the very
       * first time entry and see if this is not already higher than
       * 'local_start_time', ie this is our first entry.
       */
      if (earliest_entry >= local_start_time)
      {
         ptr_start = src;
      }
      else
      {
         ptr_start = search_time(src, local_start_time, earliest_entry,
                                 latest_entry, stat_buf.st_size);
      }

      ptr_end = search_time(src, local_end_time, earliest_entry,
                            latest_entry, stat_buf.st_size);
   }

   if (ptr_start == ptr_end)
   {
      /* Free all memory we have allocated. */
#ifdef _NO_MMAP
      free(src);
#else
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#endif
      return;
   }

   /*
    * So, start and end are found. Now lets do the real search,
    * ie search for specific file names, recipient, etc.
    */
   ptr = ptr_start;
   if ((search_file_name[0] == '\0') && (search_file_size == -1) &&
       (no_of_search_hosts == 0))
   {
      no_criteria(ptr_start, ptr_end, file_no, src);
   }
   else if ((search_file_name[0] != '\0') && (search_file_size == -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_size_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_and_size(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           recipient_only(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_and_recipient(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_size_and_recipient(ptr_start, ptr_end, file_no, src);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_size_recipient(ptr_start, ptr_end, file_no, src);
        }
        else
        {
           (void)xrec(toplevel_w, FATAL_DIALOG,
                      "What's this!? Impossible! (%s %d)",
                      __FILE__, __LINE__);
        }

   /* Free all memory we have allocated. */
#ifdef _NO_MMAP
   free(src);
#else
   if (munmap(src, stat_buf.st_size) < 0)
   {
      (void)xrec(toplevel_w, ERROR_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
#endif

   return;
}


/*------------------------------ search_time() --------------------------*/
static char *
search_time(char   *src,
            time_t search_time_val,
            time_t earliest_entry,
            time_t latest_entry,
            size_t size)
{
   char   *ptr,
          *bs_ptr,
          time_buf[MAX_INT_LENGTH];
   time_t time_val;

   if (search_time_val == -1)
   {
      return(src + size);
   }
   else
   {
      /*
       * YUCK! Now we have to search for it! We know the
       * time of the very first entry and the last entry. So
       * lets see if 'search_time_val' is closer to the beginning
       * or end in our buffer. Thats where we will start our
       * search.
       */
      if (latest_entry < search_time_val)
      {
         return(src + size);
      }
      if ((search_time_val - earliest_entry) > (latest_entry - search_time_val))
      {
         /* Start search from end. */
         bs_ptr = src + size - 2;
         do
         {
            ptr = bs_ptr;
            while (*ptr != '\n')
            {
               ptr--;
            }
            bs_ptr = ptr - 1;
            ptr++;
            GET_TIME();
         } while (time_val >= search_time_val);
         while (*ptr != '\n')
         {
            ptr++;
         }
         return(ptr + 1);
      }
      else /* Start search from beginning. */
      {
         ptr = src;
         do
         {
            while (*ptr != '\n')
            {
               ptr++;
            }
            ptr++;
            GET_TIME();
         } while (time_val < search_time_val);
         while (*ptr != '\n')
         {
            ptr--;
         }
         return(ptr + 1);
      }
   }
}


/*------------------------------ no_criteria() --------------------------*/
/*                               -------------                           */
/* Description: Reads everyting from ptr to ptr_end. It only checks if   */
/*              the transfer type is the correct one.                    */
/*-----------------------------------------------------------------------*/
static void
no_criteria(register char *ptr,
            char          *ptr_end,
            int           file_no,
            char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   char         *tmp_ptr,
                *p_size,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   /* The easiest case! */
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               INSERT_TIME_TYPE(FTP_ID_STR);
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    INSERT_TIME_TYPE(SMTP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    INSERT_TIME_TYPE(FILE_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    INSERT_TIME_TYPE(WMO_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    INSERT_TIME_TYPE(MAP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 /* This some unknown type! */
                 INSERT_TIME_TYPE(UNKNOWN_ID_STR);
              }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
         SET_FILE_NAME_POINTER();
         j = 0;
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         p_size = ptr;
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += strtod(p_size, NULL);
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------------- file_name_only() -------------------------*/
/*                             ----------------                          */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              file name search_file_name. It also checks if the        */
/*              transfer type is the correct one.                        */
/*-----------------------------------------------------------------------*/
static void
file_name_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   char         *tmp_ptr,
                *p_size,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               SET_FILE_NAME_POINTER();
               if (sfilter(search_file_name, ptr) == 0)
               {
                  il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                  INSERT_TIME_TYPE(FTP_ID_STR);
                  j = 0;
                  while ((*ptr != ' ') && (j < file_name_length))
                  {
                     *(p_file_name + j) = *ptr;
                     ptr++; j++;
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                       INSERT_TIME_TYPE(SMTP_ID_STR);
                       j = 0;
                       while ((*ptr != ' ') && (j < file_name_length))
                       {
                          *(p_file_name + j) = *ptr;
                          ptr++; j++;
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                       INSERT_TIME_TYPE(FILE_ID_STR);
                       j = 0;
                       while ((*ptr != ' ') && (j < file_name_length))
                       {
                          *(p_file_name + j) = *ptr;
                          ptr++; j++;
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                       INSERT_TIME_TYPE(WMO_ID_STR);
                       j = 0;
                       while ((*ptr != ' ') && (j < file_name_length))
                       {
                          *(p_file_name + j) = *ptr;
                          ptr++; j++;
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                       INSERT_TIME_TYPE(MAP_ID_STR);
                       j = 0;
                       while ((*ptr != ' ') && (j < file_name_length))
                       {
                          *(p_file_name + j) = *ptr;
                          ptr++; j++;
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 SET_FILE_NAME_POINTER();
                 if (sfilter(search_file_name, ptr) == 0)
                 {
                    il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                    INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                    j = 0;
                    while ((*ptr != ' ') && (j < file_name_length))
                    {
                       *(p_file_name + j) = *ptr;
                       ptr++; j++;
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         p_size = ptr;
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += strtod(p_size, NULL);
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------------- file_size_only() -------------------------*/
/*                             ----------------                          */
/* Description: Reads everyting from ptr to ptr_end and search for any   */
/*              file that is <, >, or = search_file_size. It also checks */
/*              if the transfer type is the correct one.                 */
/*-----------------------------------------------------------------------*/
static void
file_size_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               FILE_SIZE_ONLY(FTP_ID_STR);
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    FILE_SIZE_ONLY(SMTP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    FILE_SIZE_ONLY(FILE_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    FILE_SIZE_ONLY(WMO_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    FILE_SIZE_ONLY(MAP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 FILE_SIZE_ONLY(UNKNOWN_ID_STR);
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         il[file_no].line_offset[item_counter] = (int)(ptr - p_start_log_file);
         (void)memcpy(time_buf, ptr_start_line, 10);
         time_buf[10] = '\0';
         time_when_transmitted = (time_t)atol(time_buf);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = gmtime(&time_when_transmitted);
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != ' ')
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) == '/')
            {
               ptr = tmp_ptr + 2;
            }
         }
         j = 0;
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* File size is already stored. */
         while (*ptr != ' ')
         {
            ptr++;
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*-------------------------- file_name_and_size() -----------------------*/
/*                           --------------------                        */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              file name search_file_name that is <, >, or =            */
/*              search_file_size. It also checks if the transfer type is */
/*              the correct one.                                         */
/*-----------------------------------------------------------------------*/
static void
file_name_and_size(register char *ptr,
                   char          *ptr_end,
                   int           file_no,
                   char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               SET_FILE_NAME_POINTER();
               if (sfilter(search_file_name, ptr) != 0)
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 SET_FILE_NAME_POINTER();
                 if (sfilter(search_file_name, ptr) != 0)
                 {
                    IGNORE_ENTRY();
                 }
              }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;

         tmp_file_size = strtod(ptr, NULL);
         if ((gt_lt_sign == EQUAL_SIGN) &&
             (tmp_file_size != search_file_size))
         {
            IGNORE_ENTRY();
         }
         else if ((gt_lt_sign == LESS_THEN_SIGN) &&
                  (tmp_file_size >= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == GREATER_THEN_SIGN) &&
                  (tmp_file_size <= search_file_size))
              {
                 IGNORE_ENTRY();
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);
         (void)memcpy(time_buf, ptr_start_line, 10);
         time_buf[10] = '\0';
         time_when_transmitted = (time_t)atol(time_buf);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = gmtime(&time_when_transmitted);
         CONVERT_TIME();
         if (type == FTP)
         {
            (void)memcpy(p_type, FTP_ID_STR, 4);
         }
         else if (type == SMTP)
              {
                 (void)memcpy(p_type, SMTP_ID_STR, 4);
              }
         else if (type == LOC)
              {
                 (void)memcpy(p_type, FILE_ID_STR, 4);
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 (void)memcpy(p_type, WMO_ID_STR, 4);
              }
#endif
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 (void)memcpy(p_type, MAP_ID_STR, 4);
              }
#endif
              else
              {
                 (void)memcpy(p_type, UNKNOWN_ID_STR, 4);
              }
         j = 0;
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != ' ')
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) == '/')
            {
               ptr = tmp_ptr + 2;
            }
         }
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*--------------------------- recipient_only() --------------------------*/
/*                            ----------------                           */
/* Description: Reads everyting from ptr to ptr_end and search for the   */
/*              recipient 'search_recipient. It also checks if the       */
/*              transfer type is the correct one.                        */
/*-----------------------------------------------------------------------*/
static void
recipient_only(register char *ptr,
               char          *ptr_end,
               int           file_no,
               char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   char         *tmp_ptr,
                *p_size,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         current_search_host = -1;
         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               int ii;

               for (ii = 0; ii < no_of_search_hosts; ii++)
               {
                  if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                  {
                     current_search_host = ii;
                     break;
                  }
               }
               if (current_search_host != -1)
               {
                  INSERT_TIME_TYPE(FTP_ID_STR);
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(SMTP_ID_STR);
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(FILE_ID_STR);
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(WMO_ID_STR);
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(MAP_ID_STR);
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                    {
                       current_search_host = ii;
                       break;
                    }
                 }
                 if (current_search_host != -1)
                 {
                    INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
         SET_FILE_NAME_POINTER();
         j = 0;
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         p_size = ptr;
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += strtod(p_size, NULL);
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_name_and_recipient() ---------------------*/
static void
file_name_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   char         *tmp_ptr,
                *p_size,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         current_search_host = -1;
         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            FILE_NAME_AND_RECIPIENT(SHOW_FTP, FTP_ID_STR);
         }
         else if (type == SMTP)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_SMTP, SMTP_ID_STR);
              }
         else if (type == LOC)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_FILE, FILE_ID_STR);
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_WMO, WMO_ID_STR);
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_MAP, MAP_ID_STR);
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                    {
                       current_search_host = ii;
                       break;
                    }
                 }
                 if (current_search_host != -1)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);
                       INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                       j = 0;
                       while ((*ptr != ' ') && (j < file_name_length))
                       {
                          *(p_file_name + j) = *ptr;
                          ptr++; j++;
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         p_size = ptr;
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += strtod(p_size, NULL);
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_size_and_recipient() ---------------------*/
static void
file_size_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         current_search_host = -1;
         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               FILE_SIZE_AND_RECIPIENT(FTP_ID_STR);
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    FILE_SIZE_AND_RECIPIENT(SMTP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    FILE_SIZE_AND_RECIPIENT(FILE_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    FILE_SIZE_AND_RECIPIENT(WMO_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    FILE_SIZE_AND_RECIPIENT(MAP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 FILE_SIZE_AND_RECIPIENT(UNKNOWN_ID_STR);
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         il[file_no].line_offset[item_counter] = (int)(ptr - p_start_log_file);
         (void)memcpy(time_buf, ptr_start_line, 10);
         time_buf[10] = '\0';
         time_when_transmitted = (time_t)atol(time_buf);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = gmtime(&time_when_transmitted);
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != ' ')
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) == '/')
            {
               ptr = tmp_ptr + 2;
            }
         }
         j = 0;
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;

         /* File size is already stored. */
         while (*ptr != ' ')
         {
            ptr++;
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------- file_name_size_recipient() ---------------------*/
static void
file_name_size_recipient(register char *ptr,
                         char          *ptr_end,
                         int           file_no,
                         char          *p_start_log_file)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = 0,
                loops = 0;
   time_t       time_when_transmitted = 0L;
#ifdef _WITH_CHECK_TIME_INTERVAL
   time_t       next_check_time,
                now;
#endif
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line,
                time_buf[MAX_INT_LENGTH];
   struct tm    *p_ts;

#ifdef _WITH_CHECK_TIME_INTERVAL
   time(&now);
   next_check_time = ((now / CHECK_TIME_INTERVAL) * CHECK_TIME_INTERVAL) +
                     CHECK_TIME_INTERVAL;
#endif

   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
#ifdef _WITH_CHECK_TIME_INTERVAL
         if (time(&now) > next_check_time)
         {
            next_check_time = ((now / CHECK_TIME_INTERVAL) *
                               CHECK_TIME_INTERVAL) + CHECK_TIME_INTERVAL;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }
#endif

         current_search_host = -1;
         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               int ii;

               for (ii = 0; ii < no_of_search_hosts; ii++)
               {
                  if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                  {
                     current_search_host = ii;
                     break;
                  }
               }
               if (current_search_host != -1)
               {
                  SET_FILE_NAME_POINTER();
                  if (sfilter(search_file_name, ptr) != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  IGNORE_ENTRY();
               }
            }
            else
            {
               IGNORE_ENTRY();
            }
         }
         else if (type == SMTP)
              {
                 if (toggles_set & SHOW_SMTP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr) != 0)
                       {
                          IGNORE_ENTRY();
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr) != 0)
                       {
                          IGNORE_ENTRY();
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr) != 0)
                       {
                          IGNORE_ENTRY();
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 if (toggles_set & SHOW_MAP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr) != 0)
                       {
                          IGNORE_ENTRY();
                       }
                    }
                    else
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_MAP_SUPPORT */
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11) == 0)
                    {
                       current_search_host = ii;
                       break;
                    }
                 }
                 if (current_search_host != -1)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }

         il[file_no].line_offset[item_counter] = (int)(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3 - p_start_log_file);

         /* If necessary, ignore rest of file name. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         tmp_file_size = strtod(ptr, NULL);
         if ((gt_lt_sign == EQUAL_SIGN) &&
             (tmp_file_size != search_file_size))
         {
            IGNORE_ENTRY();
         }
         else if ((gt_lt_sign == LESS_THEN_SIGN) &&
                  (tmp_file_size >= search_file_size))
              {
                 IGNORE_ENTRY();
              }
         else if ((gt_lt_sign == GREATER_THEN_SIGN) &&
                  (tmp_file_size <= search_file_size))
              {
                 IGNORE_ENTRY();
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);
         (void)memcpy(time_buf, ptr_start_line, 10);
         time_buf[10] = '\0';
         time_when_transmitted = (time_t)atol(time_buf);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = gmtime(&time_when_transmitted);
         CONVERT_TIME();
         if (type == FTP)
         {
            (void)memcpy(p_type, FTP_ID_STR, 4);
         }
         else if (type == SMTP)
              {
                 (void)memcpy(p_type, SMTP_ID_STR, 4);
              }
         else if (type == LOC)
              {
                 (void)memcpy(p_type, FILE_ID_STR, 4);
              }
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 (void)memcpy(p_type, WMO_ID_STR, 4);
              }
#endif
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 (void)memcpy(p_type, MAP_ID_STR, 4);
              }
#endif
              else
              {
                 (void)memcpy(p_type, UNKNOWN_ID_STR, 4);
              }
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != ' ')
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) == '/')
            {
               ptr = tmp_ptr + 2;
            }
         }
         j = 0;
         while ((*ptr != ' ') && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         while (*ptr != ' ')
         {
            ptr++;
         }
         ptr++;
         if (*ptr == '/')
         {
            /* Ignore  the remote file name */
            while (*ptr != ' ')
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != ' ')
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != ' ') && (j < MAX_DISPLAYED_FILE_SIZE))
         {
            *(p_file_size - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if ((j == MAX_DISPLAYED_FILE_SIZE) && (*tmp_ptr != ' '))
         {
            *(p_file_size - j) = '>';
         }

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      CHECK_INTERRUPT();

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   il[file_no].no_of_items = item_counter;

   return;
}


/*--------------------------- display_data() ----------------------------*/
static void
display_data(int    i,
             time_t first_date_found,
             time_t time_when_transmitted)
{
   register int j;
   static int   rotate;
   char         status_message[MAX_MESSAGE_LENGTH];

   XmListAddItemsUnselected(listbox_w, str_list, i, 0);
   for (j = 0; j < i; j++)
   {
      XmStringFree(str_list[j]);
   }
   total_no_files += i;
   rotate++;
   if (rotate == 0)
   {
      (void)strcpy(status_message, "Searching  -");
   }
   else if (rotate == 1)
        {
           (void)strcpy(status_message, "Searching  \\");
        }
   else if (rotate == 2)
        {
           (void)strcpy(status_message, "Searching  |");
        }
        else
        {
           (void)strcpy(status_message, "Searching  /");
           rotate = -1;
        }
   SHOW_MESSAGE();

   calculate_summary(summary_str, first_date_found, time_when_transmitted,
                     total_no_files, file_size, trans_time);
   XmTextSetString(summarybox_w, summary_str);
   (void)strcpy(total_summary_str, summary_str);

   return;
}
