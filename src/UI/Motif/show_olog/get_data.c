/*
 *  get_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   12.04.2002 Separator definable via SEPARATOR_CHAR.
 **   10.04.2004 Added TLS/SSL support.
 **
 */
DESCR__E_M3

#include <stdio.h>        /* sprintf()                                   */
#include <string.h>       /* strerror()                                  */
#include <stdlib.h>       /* malloc(), realloc(), free(), strtoul(), abs()*/
#include <time.h>         /* time()                                      */
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif
#include <unistd.h>       /* close()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>     /* mmap(), munmap()                            */
#ifndef MAP_FILE          /* Required for BSD          */
#define MAP_FILE 0        /* All others do not need it */
#endif
#endif
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/Text.h>
#include <Xm/LabelP.h>
#include "logdefs.h"
#include "afd_ctrl.h"
#include "show_olog.h"

/* External global variables */
extern Display          *display;
extern Window           main_window;
extern XtAppContext     app;
extern Widget           listbox_w,
                        scrollbar_w,
                        statusbox_w,
                        special_button_w,
                        summarybox_w,
                        appshell;
extern int              continues_toggle_set,
                        file_name_toggle_set,
                        radio_set,
                        gt_lt_sign,
                        max_output_log_files,
                        no_of_search_hosts,
                        special_button_flag,
                        file_name_length,
                        no_of_log_files;
extern XT_PTR_TYPE      toggles_set;
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
static int              interval_id_set = NO,
                        last_file_no,
                        log_fd = -1;
static time_t           local_start_time,
                        local_end_time,
                        first_date_found,
                        start;
static off_t            log_offset;
static ino_t            log_inode;
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
static XtIntervalId     interval_id_log;

/* Local function prototypes */
static char   *search_time(char *, time_t, time_t, time_t, size_t);
static void   check_log_updates(Widget),
              display_data(int, time_t, time_t),
              end_log_updates(void),
              extract_data(char *, int, int),
              no_criteria(register char *, char *, int, char *, off_t),
              file_name_only(register char *, char *, int, char *, off_t),
              file_size_only(register char *, char *, int, char *, off_t),
              file_name_and_size(register char *, char *, int, char *, off_t),
              recipient_only(register char *, char *, int, char *, off_t),
              file_name_and_recipient(register char *, char *, int, char *, off_t),
              file_size_and_recipient(register char *, char *, int, char *, off_t),
              file_name_size_recipient(register char *, char *, int, char *, off_t);

#define REALLOC_OFFSET_BUFFER()                                   \
        {                                                         \
           if ((item_counter == 0) || (item_counter > prev_item_counter)) \
           {                                                      \
              int new_int_size,                                   \
                  new_char_size,                                  \
                  new_off_t_size;                                 \
                                                                  \
              new_char_size = item_counter + LINES_BUFFERED + 1;  \
              new_int_size = new_char_size * sizeof(int);         \
              new_off_t_size = new_char_size * sizeof(off_t);     \
              prev_item_counter = item_counter;                   \
                                                                  \
              if (((il[file_no].offset = realloc(il[file_no].offset, new_int_size)) == NULL) ||            \
                  ((il[file_no].line_offset = realloc(il[file_no].line_offset, new_off_t_size)) == NULL) ||\
                  ((il[file_no].archived = realloc(il[file_no].archived, new_char_size)) == NULL))         \
              {                                                   \
                 (void)xrec(appshell, FATAL_DIALOG,               \
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
              while (*tmp_ptr != SEPARATOR_CHAR)        \
              {                                         \
                 tmp_ptr++;                             \
              }                                         \
              if (*(tmp_ptr + 1) != SEPARATOR_CHAR)     \
              {                                         \
                 ptr = tmp_ptr + 1;                     \
                 ptr_is_remote = YES;                   \
              }                                         \
              else                                      \
              {                                         \
                 ptr_is_remote = NO;                    \
              }                                         \
           }                                            \
           else                                         \
           {                                            \
              ptr_is_remote = NO;                       \
           }                                            \
        }
#define INSERT_TIME_TYPE(protocol)                         \
        {                                                  \
           (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
           time_when_transmitted = (time_t)strtol(ptr_start_line, NULL, 16);\
           if (first_date_found == -1)                     \
           {                                               \
              first_date_found = time_when_transmitted;    \
           }                                               \
           p_ts = localtime(&time_when_transmitted);       \
           CONVERT_TIME();                                 \
           (void)memcpy(p_type, (protocol), 5);            \
        }
#define COMMON_BLOCK()              \
        {                           \
           ptr++;                   \
           while (*ptr != SEPARATOR_CHAR)\
           {                        \
              ptr++;                \
           }                        \
           tmp_ptr = ptr - 1;       \
           j = 0;                   \
           while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
           {                        \
              *(p_tt - j) = *tmp_ptr;\
              tmp_ptr--; j++;       \
           }                        \
           if (j == MAX_DISPLAYED_TRANSFER_TIME)\
           {                        \
              tmp_ptr = ptr - 4;    \
              j = 0;                \
              while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))\
              {                     \
                 *(p_tt - j) = *tmp_ptr;\
                 tmp_ptr--; j++;    \
              }                     \
              if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))\
              {                     \
                 *(p_tt - j) = '>'; \
                 while (*tmp_ptr != SEPARATOR_CHAR)\
                 {                  \
                    tmp_ptr--;      \
                 }                  \
              }                     \
              else                  \
              {                     \
                 while (j < MAX_DISPLAYED_TRANSFER_TIME)\
                 {                  \
                    *(p_tt - j) = ' ';\
                    j++;            \
                 }                  \
              }                     \
           }                        \
           tmp_ptr++;               \
                                    \
           ptr++;                   \
           il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);\
                                    \
           if ((search_directory_name[0] != '\0') ||\
               ((current_search_host != -1) &&\
                (search_user[current_search_host][0] != '\0')))\
           {                        \
              int  count = 0;       \
              char job_id_str[15];  \
                                    \
              while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) && (count < 15))\
              {                     \
                 job_id_str[count] = *ptr;\
                 count++; ptr++;    \
              }                     \
              job_id_str[count] = '\0';\
              id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);\
                                    \
              if ((current_search_host != -1) &&\
                  (search_user[current_search_host][0] != '\0'))\
              {                     \
                 char *at_ptr = search_user[current_search_host];\
                                    \
                 id.user[0] = '\0'; \
                 id.mail_destination[0] = '\0';\
                 get_info(GOT_JOB_ID_USER_ONLY);\
                                    \
                 while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))\
                 {                  \
                    at_ptr++;       \
                 }                  \
                 if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))\
                 {                  \
                    if (sfilter(search_user[current_search_host],\
                                id.mail_destination, ' ') != 0)\
                    {               \
                       IGNORE_ENTRY();\
                    }               \
                 }                  \
                 else               \
                 {                  \
                    if (sfilter(search_user[current_search_host], id.user, ' ') != 0)\
                    {               \
                       IGNORE_ENTRY();\
                    }               \
                 }                  \
              }                     \
              if (search_directory_name[0] != '\0')\
              {                     \
                 id.dir[0] = '\0';  \
                 get_info(GOT_JOB_ID_DIR_ONLY);\
                 count = strlen(id.dir);\
                 id.dir[count] = SEPARATOR_CHAR;\
                 id.dir[count + 1] = '\0';\
                                    \
                 if (sfilter(search_directory_name, id.dir, SEPARATOR_CHAR) != 0)\
                 {                  \
                    IGNORE_ENTRY(); \
                 }                  \
              }                     \
           }                        \
           else                     \
           {                        \
              while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))\
              {                     \
                 ptr++;             \
              }                     \
           }                        \
           trans_time += strtod(tmp_ptr, NULL);\
           ptr++;                   \
           while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))\
           {                        \
              ptr++;                \
           }                        \
                                    \
           if (*ptr == SEPARATOR_CHAR)\
           {                        \
              int  sub_dir_counter = 0;\
              char archive_status = 'Y';\
                                    \
              ptr++;                \
              while (*ptr != '\n')  \
              {                     \
                 if ((*ptr == '/') && (*(ptr - 1) != '\\'))\
                 {                  \
                    sub_dir_counter++;\
                    if (sub_dir_counter == 3)\
                    {               \
                       int  cc = 0; \
                       char long_no[MAX_INT_LENGTH];\
                                    \
                       ptr += 1;    \
                       while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))\
                       {            \
                          long_no[cc] = *ptr;\
                          cc++; ptr++;\
                       }            \
                       if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))\
                       {            \
                          time_t delete_time;\
                                    \
                          long_no[cc] = '\0';\
                          delete_time = (time_t)strtol(long_no, (char **)NULL, 16);\
                          if (now > (delete_time + ARCHIVE_STEP_TIME))\
                          {         \
                             archive_status = 'D';\
                          }         \
                          else if (now > (delete_time - 5))\
                               {    \
                                  archive_status = '?';\
                               }    \
                       }            \
                    }               \
                 }                  \
                 ptr++;             \
              }                     \
              while (*ptr != '\n')  \
              {                     \
                 ptr++;             \
              }                     \
              *p_archive_flag = archive_status;\
              il[file_no].archived[item_counter] = 1;\
           }                        \
           else                     \
           {                        \
              *p_archive_flag = 'N';\
           }                        \
           item_counter++;          \
                                    \
           str_list[i] = XmStringCreateLocalized(line);\
                                    \
           ptr++;                   \
        }

#define CHECK_LIST_LIMIT()                                          \
        {                                                           \
           if ((perm.list_limit > 0) && (item_counter > perm.list_limit))\
           {                                                        \
              char msg_buffer[40];                                  \
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
              if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)\
              {                                            \
                 current_search_host = ii;                 \
                 break;                                    \
              }                                            \
           }                                               \
           if (current_search_host != -1)                  \
           {                                               \
              ptr += 11 + MAX_HOSTNAME_LENGTH + 3;         \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
              if (*ptr != SEPARATOR_CHAR)                  \
              {                                            \
                 /* Ignore  the remote file name */        \
                 while (*ptr != SEPARATOR_CHAR)            \
                 {                                         \
                    ptr++;                                 \
                 }                                         \
                 ptr++;                                    \
              }                                            \
              else                                         \
              {                                            \
                 ptr++;                                    \
              }                                            \
                                                           \
              j = 0;                                       \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++; j++;                               \
              }                                            \
              if (j < 9)                                   \
              {                                            \
                 tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
              }                                            \
              else                                         \
              {                                            \
                 if (j > 20)                               \
                 {                                         \
                    tmp_file_size = strtod("INF", NULL);   \
                 }                                         \
                 else                                      \
                 {                                         \
                    char tmp_buf[23];                      \
                                                           \
                    tmp_buf[0] = '0'; tmp_buf[1] = 'x';    \
                    (void)memcpy(&tmp_buf[2], ptr - j, j); \
                    tmp_buf[2 + j] = '\0';                 \
                    tmp_file_size = strtod(tmp_buf, NULL); \
                 }                                         \
              }                                            \
              if ((gt_lt_sign == EQUAL_SIGN) &&            \
                  (tmp_file_size == search_file_size))     \
              {                                            \
                 (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                 (void)memcpy(p_type, (id_string), 5);     \
                                                           \
                 /* Write file size. */                    \
                 print_file_size(p_file_size, (off_t)tmp_file_size);\
              }                                            \
              else if ((gt_lt_sign == LESS_THEN_SIGN) &&   \
                       (tmp_file_size < search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
                   }                                       \
              else if ((gt_lt_sign == GREATER_THEN_SIGN) &&\
                       (tmp_file_size > search_file_size)) \
                   {                                       \
                      (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                      (void)memcpy(p_type, (id_string), 5);\
                                                           \
                      /* Write file size. */               \
                      print_file_size(p_file_size, (off_t)tmp_file_size);\
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

#define FILE_NAME_AND_RECIPIENT(toggle_id, id_string)      \
        {                                                  \
            if (toggles_set & (toggle_id))                 \
            {                                              \
               int ii;                                     \
                                                           \
               for (ii = 0; ii < no_of_search_hosts; ii++) \
               {                                           \
                  if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)\
                  {                                        \
                     current_search_host = ii;             \
                     break;                                \
                  }                                        \
               }                                           \
               if (current_search_host != -1)              \
               {                                           \
                  SET_FILE_NAME_POINTER();                 \
                  if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0) \
                  {                                        \
                     il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);\
                     INSERT_TIME_TYPE((id_string));        \
                     j = 0;                                \
                     while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))\
                     {                                     \
                        *(p_file_name + j) = *ptr;         \
                        ptr++; j++;                        \
                     }                                     \
                  }                                        \
                  else                                     \
                  {                                        \
                     IGNORE_ENTRY();                       \
                  }                                        \
               }                                           \
               else                                        \
               {                                           \
                  IGNORE_ENTRY();                          \
               }                                           \
            }                                              \
            else                                           \
            {                                              \
               IGNORE_ENTRY();                             \
            }                                              \
        }

#define FILE_SIZE_ONLY(id_string)                          \
        {                                                  \
           ptr += 11 + MAX_HOSTNAME_LENGTH + 3;            \
           while (*ptr != SEPARATOR_CHAR)                  \
           {                                               \
              ptr++;                                       \
           }                                               \
           ptr++;                                          \
           if (*ptr != SEPARATOR_CHAR)                     \
           {                                               \
              /* Ignore  the remote file name */           \
              while (*ptr != SEPARATOR_CHAR)               \
              {                                            \
                 ptr++;                                    \
              }                                            \
              ptr++;                                       \
           }                                               \
           else                                            \
           {                                               \
              ptr++;                                       \
           }                                               \
                                                           \
           j = 0;                                          \
           while (*ptr != SEPARATOR_CHAR)                  \
           {                                               \
              ptr++; j++;                                  \
           }                                               \
           if (j < 9)                                      \
           {                                               \
              tmp_file_size = (double)strtoul(ptr - j, NULL, 16);\
           }                                               \
           else                                            \
           {                                               \
              if (j > 20)                                  \
              {                                            \
                 tmp_file_size = strtod("INF", NULL);      \
              }                                            \
              else                                         \
              {                                            \
                 char tmp_buf[23];                         \
                                                           \
                 tmp_buf[0] = '0'; tmp_buf[1] = 'x';       \
                 (void)memcpy(&tmp_buf[2], ptr - j, j);    \
                 tmp_buf[2 + j] = '\0';                    \
                 tmp_file_size = strtod(tmp_buf, NULL);    \
              }                                            \
           }                                               \
           if ((gt_lt_sign == EQUAL_SIGN) &&               \
               (tmp_file_size == search_file_size))        \
           {                                               \
              (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
              (void)memcpy(p_type, (id_string), 5);        \
                                                           \
              /* Write file size. */                       \
              print_file_size(p_file_size, (off_t)tmp_file_size);\
           }                                               \
           else if ((gt_lt_sign == LESS_THEN_SIGN) &&      \
                    (tmp_file_size < search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
           else if ((gt_lt_sign == GREATER_THEN_SIGN) &&   \
                    (tmp_file_size > search_file_size))    \
                {                                          \
                   (void)memset(line, ' ', MAX_OUTPUT_LINE_LENGTH + file_name_length);\
                   (void)memcpy(p_type, (id_string), 5);   \
                                                           \
                   /* Write file size. */                  \
                   print_file_size(p_file_size, (off_t)tmp_file_size);\
                }                                          \
                else                                       \
                {                                          \
                   IGNORE_ENTRY();                         \
                }                                          \
        }

#define _MACRO_DEBUG

/*############################### get_data() ############################*/
void
get_data(void)
{
#ifdef _OUTPUT_LOG
   int          i,
                j,
                start_file_no = -1,
                end_file_no = -1;
   time_t       end;
   double       total_file_size,
                total_trans_time;
   char         status_message[MAX_MESSAGE_LENGTH];
   struct stat  stat_buf;
   XmString     xstr;

   /* At start always reset these values. */
   if (interval_id_set == YES)
   {
      XtRemoveTimeOut(interval_id_log);
      interval_id_set = NO;
   }
   if (log_fd != -1)
   {
      if (close(log_fd) == -1)
      {
         (void)xrec(appshell, FATAL_DIALOG, "close() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      else
      {
         log_fd = -1;
      }
   }

   /* Prepare log file name. */
   p_log_file = log_file;
   no_of_log_files = max_output_log_files;
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
   no_of_log_files = start_file_no - end_file_no + 1;

   if ((str_list = (XmStringTable)XtMalloc(LINES_BUFFERED * sizeof(XmString))) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Allocate memory for item list */
   if (il == NULL)
   {
      if ((il = malloc(max_output_log_files * sizeof(struct item_list))) == NULL)
      {
         (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      for (i = 0; i < max_output_log_files; i++)
      {
         il[i].fp = NULL;
         il[i].no_of_items = 0;
         il[i].line_offset = NULL;
         il[i].offset = NULL;
         il[i].archived = NULL;
      }
   }
   else
   {
      for (i = 0; i < max_output_log_files; i++)
      {
         if (il[i].fp != NULL)
         {
            if (fclose(il[i].fp) == EOF)
            {
               (void)fprintf(stderr, "fclose() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            il[i].fp = NULL;
         }
         il[i].no_of_items = 0;
         if (il[i].line_offset != NULL)
         {
            free(il[i].line_offset);
            il[i].line_offset = NULL;
         }
         if (il[i].offset != NULL)
         {
            free(il[i].offset);
            il[i].offset = NULL;
         }
         if (il[i].archived != NULL)
         {
            free(il[i].archived);
            il[i].archived = NULL;
         }
      }
   }

   /* Initialise all pointer in line. */
   p_file_name    = line + 16;
   p_host_name    = p_file_name + file_name_length + 1;
   p_type         = p_host_name + MAX_HOSTNAME_LENGTH + 1;
   p_file_size    = p_type + 6 + 1;
   p_tt           = p_file_size + MAX_DISPLAYED_FILE_SIZE + MAX_DISPLAYED_TRANSFER_TIME;
   p_archive_flag = p_tt + 2;
   *(line + MAX_OUTPUT_LINE_LENGTH + file_name_length) = '\0';

   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   summary_str[0] = ' ';
   summary_str[1] = '\0';
   SHOW_SUMMARY_DATA();
   (void)strcpy(status_message, "Searching  -");
   SHOW_MESSAGE();
   CHECK_INTERRUPT();

   start = time(NULL);
   file_size = trans_time = 0.0;
   total_no_files = 0;
   first_date_found = -1;
   j = 0;
   for (i = start_file_no;
        ((i >= end_file_no) && (special_button_flag != STOP_BUTTON_PRESSED));
        i--, j++)
   {
      (void)sprintf(p_log_file, "%d", i);
      (void)extract_data(log_file, j, i);
      total_file_size += file_size;
      total_trans_time += trans_time;
      if ((perm.list_limit > 0) && (total_no_files >= perm.list_limit))
      {
         break;
      }
   }
   end = time(NULL);

   if ((continues_toggle_set == NO) || (log_fd == -1) ||
       ((end > end_time_val) && (end_time_val != -1)))
   {
      set_sensitive();
      if (total_no_files != 0)
      {
#if SIZEOF_TIME_T == 4
         (void)sprintf(status_message, "Search time: %lds", end - start);
#else
         (void)sprintf(status_message, "Search time: %llds", end - start);
#endif
      }
      else
      {
#if SIZEOF_TIME_T == 4
         (void)sprintf(status_message, "No data found. Search time: %lds",
#else
         (void)sprintf(status_message, "No data found. Search time: %llds",
#endif
                       end - start);
      }
      SHOW_MESSAGE();

      special_button_flag = SEARCH_BUTTON;
      xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
      XtFree((char *)str_list);
   }
   else
   {
      interval_id_set = YES;
      interval_id_log = XtAppAddTimeOut(app, LOG_CHECK_INTERVAL,
                                        (XtTimerCallbackProc)check_log_updates,
                                        listbox_w);
   }

#endif /* _OUTPUT_LOG */
   return;
}


/*+++++++++++++++++++++++++++++ extract_data() ++++++++++++++++++++++++++*/
static void
extract_data(char *current_log_file, int file_no, int log_no)
{
   int           fd;
   time_t        time_val,
                 earliest_entry,
                 latest_entry;
   register char *ptr,
                 *ptr_start;
   char          *src,
                 *tmp_ptr,
                 *ptr_end;
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
         (void)xrec(appshell, WARN_DIALOG, "Failed to stat() %s : %s (%s %d)",
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
      (void)xrec(appshell, FATAL_DIALOG, "Failed to open() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      return;
   }
   if ((il[file_no].fp = fdopen(fd, "r")) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "fdopen() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }
#ifdef HAVE_MMAP
   if ((src = mmap(0, stat_buf.st_size, PROT_READ,
                   (MAP_FILE | MAP_SHARED), fd, 0)) == (caddr_t) -1)
   {
      (void)xrec(appshell, FATAL_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
#else
   if ((src = malloc(stat_buf.st_size)) == NULL)
   {
      (void)xrec(appshell, FATAL_DIALOG, "malloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return;
   }
   if (read(fd, src, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)xrec(appshell, FATAL_DIALOG,
                 "Failed to read() from %s : %s (%s %d)",
                 current_log_file, strerror(errno), __FILE__, __LINE__);
      free(src);
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
   time_val = (time_t)strtol(ptr, NULL, 16);
   latest_entry = time_val;

   /* Get earliest entry. */
   ptr = src;
   time_val = (time_t)strtol(ptr, NULL, 16);
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
#ifdef HAVE_MMAP
      if (munmap(src, stat_buf.st_size) < 0)
      {
         (void)xrec(appshell, ERROR_DIALOG, "munmap() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
      }
#else
      free(src);
#endif
      return;
   }

   if ((log_no == 0) && ((end_time_val == -1) || (time(NULL) < end_time_val)))
   {
      /*
       * NOTE: We need to have this opened twice so that the function
       *       called with XtAppAddTimeOut() has its own file descriptor
       *       position within this file.
       */  
      if ((log_fd = open(current_log_file, O_RDONLY)) == -1)
      {
         (void)xrec(appshell, FATAL_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (lseek(log_fd, stat_buf.st_size, SEEK_SET) == (off_t)-1)
      {
         (void)xrec(appshell, FATAL_DIALOG,
                    "Failed to lssek() in %s : %s (%s %d)",
                    current_log_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      log_offset = stat_buf.st_size;
      log_inode = stat_buf.st_ino;
      last_file_no = file_no;
   }

   /*
    * So, start and end are found. Now lets do the real search,
    * ie search for specific file names, recipient, etc.
    */
   ptr = ptr_start;
   if ((search_file_name[0] == '\0') && (search_file_size == -1) &&
       (no_of_search_hosts == 0))
   {
      no_criteria(ptr_start, ptr_end, file_no, src, 0);
   }
   else if ((search_file_name[0] != '\0') && (search_file_size == -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_size_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size != -1) &&
            (no_of_search_hosts == 0))
        {
           file_name_and_size(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           recipient_only(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size == -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_and_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] == '\0') && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_size_and_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
   else if ((search_file_name[0] != '\0') && (search_file_size != -1) &&
            (no_of_search_hosts != 0))
        {
           file_name_size_recipient(ptr_start, ptr_end, file_no, src, 0);
        }
        else
        {
           (void)xrec(appshell, FATAL_DIALOG,
                      "What's this!? Impossible! (%s %d)", __FILE__, __LINE__);
        }

   /* Free all memory we have allocated. */
#ifdef HAVE_MMAP
   if (munmap(src, stat_buf.st_size) < 0)
   {
      (void)xrec(appshell, ERROR_DIALOG, "munmap() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
   }
#else
   free(src);
#endif

   return;
}


/*++++++++++++++++++++++++++ check_log_updates() ++++++++++++++++++++++++*/
static void
check_log_updates(Widget w)
{
   static int rotate;

   interval_id_set = NO;
   if (special_button_flag == STOP_BUTTON_PRESSED)
   {
      end_log_updates();
      return;
   }

   if ((end_time_val == -1) || (time(NULL) < end_time_val))
   {
      struct stat stat_buf;

      if (fstat(log_fd, &stat_buf) == -1)
      {
         (void)xrec(appshell, FATAL_DIALOG, "fstat() error: %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      }
      if (log_inode != stat_buf.st_ino)
      {
         /*
          * This seems from the point of programming the simplest. It
          * is not efficient, but since the logs are rotated daily once
          * seems an acceptable solution.
          */
         XmListDeleteAllItems(listbox_w);
         get_data();
         XmListSetBottomPos(listbox_w, 0);
         return;
      }
      if (stat_buf.st_size > log_offset)
      {
         off_t diff_size;
         char  *ptr_start,
               *ptr_end;

         diff_size = stat_buf.st_size - log_offset;
         if ((ptr_start = malloc(diff_size)) == NULL)
         {
            (void)xrec(appshell, FATAL_DIALOG,
#if SIZEOF_OFF_T == 4
                       "malloc() error [%ld bytes] : %s (%s %d)",
#else
                       "malloc() error [%lld bytes] : %s (%s %d)",
#endif
                       diff_size, strerror(errno), __FILE__, __LINE__);
         }
         if (read(log_fd, ptr_start, diff_size) != diff_size)
         {
            (void)xrec(appshell, FATAL_DIALOG, "read() error: %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         }
         ptr_end = ptr_start + diff_size;

         if ((search_file_name[0] == '\0') && (search_file_size == -1) &&
             (no_of_search_hosts == 0))
         {
            no_criteria(ptr_start, ptr_end, last_file_no,
                        ptr_start, log_offset);
         }
         else if ((search_file_name[0] != '\0') &&
                  (search_file_size == -1) && (no_of_search_hosts == 0))
              {
                 file_name_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((search_file_name[0] == '\0') &&
                  (search_file_size != -1) && (no_of_search_hosts == 0))
              {
                 file_size_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((search_file_name[0] != '\0') &&
                  (search_file_size != -1) && (no_of_search_hosts == 0))
              {
                 file_name_and_size(ptr_start, ptr_end, last_file_no,
                                    ptr_start, log_offset);
              }
         else if ((search_file_name[0] == '\0') &&
                  (search_file_size == -1) && (no_of_search_hosts != 0))
              {
                 recipient_only(ptr_start, ptr_end, last_file_no, ptr_start,
                                log_offset);
              }
         else if ((search_file_name[0] != '\0') &&
                  (search_file_size == -1) && (no_of_search_hosts != 0))
              {
                 file_name_and_recipient(ptr_start, ptr_end, last_file_no,
                                         ptr_start, log_offset);
              }
         else if ((search_file_name[0] == '\0') &&
                  (search_file_size != -1) && (no_of_search_hosts != 0))
              {
                 file_size_and_recipient(ptr_start, ptr_end, last_file_no,
                                         ptr_start, log_offset);
              }
         else if ((search_file_name[0] != '\0') &&
                  (search_file_size != -1) && (no_of_search_hosts != 0))
              {
                 file_name_size_recipient(ptr_start, ptr_end, last_file_no,
                                          ptr_start, log_offset);
              }
              else
              {
                 (void)xrec(appshell, FATAL_DIALOG,
                            "What's this!? Impossible! (%s %d)",
                            __FILE__, __LINE__);
              }

         free(ptr_start);
         log_offset = stat_buf.st_size;
         XmListSetBottomPos(listbox_w, 0);
      }
      else
      {
         char status_message[13];

         (void)memset(status_message, ' ', 12);
         status_message[12] = '\0';
         status_message[rotate] = '.';
         rotate++;
         if (rotate == 12)
         {
            rotate = 0;
         }
         SHOW_MESSAGE();
      }
      interval_id_set = YES;
      interval_id_log = XtAppAddTimeOut(app, LOG_CHECK_INTERVAL,
                                        (XtTimerCallbackProc)check_log_updates,
                                        listbox_w);
   }
   else
   {
      end_log_updates();
   }
   return;
}


/*-------------------------- end_log_updates() --------------------------*/
static void
end_log_updates(void)
{
   time_t   diff_time;
   char     *ptr,
            status_message[MAX_MESSAGE_LENGTH];
   XmString xstr;

   if (total_no_files != 0)
   {
      ptr = status_message + sprintf(status_message, "Search+Wait time: ");
   }
   else
   {
      ptr = status_message + sprintf(status_message,
                                     "No data found. Search+Wait time: ");
   }
   diff_time = time(NULL) - start;
   if (diff_time > 3600)
   {
      int hours, left, min;

      hours = diff_time / 3600;
      left  = diff_time % 3600;
      min   = left / 60;
      left  = left % 60;
      (void)sprintf(ptr, "%dh %dm %ds", hours, min, left);
   }
   else if (diff_time > 60)
        {
           int left, min;

           min  = diff_time / 60;
           left = diff_time % 60;
           (void)sprintf(ptr, "%dm %ds", min, left);
        }
        else
        {
#if SIZEOF_TIME_T == 4
           (void)sprintf(ptr, "%lds", diff_time);
#else
           (void)sprintf(ptr, "%llds", diff_time);
#endif
        }

   SHOW_MESSAGE();

   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   XtFree((char *)str_list);

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
          *bs_ptr;
   time_t time_val;

   if ((search_time_val == -1) || (latest_entry < search_time_val))
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
      if (abs(search_time_val - earliest_entry) >
          abs(latest_entry - search_time_val))
      {
         /* Start search from end. */
         bs_ptr = src + size - 2;
         do
         {
            ptr = bs_ptr;
            ptr -= 11 + MAX_HOSTNAME_LENGTH + 3;
            while (*ptr != '\n')
            {
               ptr--;
            }
            bs_ptr = ptr - 1;
            ptr++;
            time_val = (time_t)strtol(ptr, NULL, 16);
         } while ((time_val >= search_time_val) && (ptr > src));
         while (*ptr != '\n')
         {
            ptr++;
         }
      }
      else /* Start search from beginning. */
      {
         ptr = src;
         do
         {
            ptr += 11 + MAX_HOSTNAME_LENGTH + 3;
            while (*ptr != '\n')
            {
               ptr++;
            }
            ptr++;
            time_val = (time_t)strtol(ptr, NULL, 16);
         } while ((time_val < search_time_val) && (ptr < (src + size)));
         while (*ptr != '\n')
         {
            ptr--;
         }
      }
      return(ptr + 1);
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
            char          *p_start_log_file,
            off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       time_when_transmitted = 0L,
                prev_time_val = 0L,
                now;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   /* The easiest case! */
   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    INSERT_TIME_TYPE(HTTP_ID_STR);
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    INSERT_TIME_TYPE(SCP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_SCP_SUPPORT */
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    INSERT_TIME_TYPE(FTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    INSERT_TIME_TYPE(HTTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    INSERT_TIME_TYPE(SMTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* WITH_SSL */
              else
              {
                 /* This some unknown type! */
                 INSERT_TIME_TYPE(UNKNOWN_ID_STR);
              }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         SET_FILE_NAME_POINTER();
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               SET_FILE_NAME_POINTER();
               if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
               {
                  il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                  INSERT_TIME_TYPE(FTP_ID_STR);
                  j = 0;
                  while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(FILE_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(HTTP_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(SMTP_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(SCP_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(WMO_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(MAP_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(FTPS_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(HTTPS_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(SMTPS_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
#endif /* WITH_SSL */
              else
              {
                 SET_FILE_NAME_POINTER();
                 if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                 {
                    il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                    INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                    j = 0;
                    while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    FILE_SIZE_ONLY(HTTP_ID_STR);
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    FILE_SIZE_ONLY(SCP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_SCP_SUPPORT */
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    FILE_SIZE_ONLY(FTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    FILE_SIZE_ONLY(HTTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    FILE_SIZE_ONLY(SMTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* WITH_SSL */
              else
              {
                 FILE_SIZE_ONLY(UNKNOWN_ID_STR);
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         time_when_transmitted = (time_t)strtol(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = localtime(&time_when_transmitted);
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
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
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

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
                   char          *p_start_log_file,
                   off_t         offset)
{
   register int i,
                j;
   int          current_search_host = -1,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            if (toggles_set & SHOW_FTP)
            {
               SET_FILE_NAME_POINTER();
               if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* WITH_SSL */
              else
              {
                 SET_FILE_NAME_POINTER();
                 if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                 {
                    IGNORE_ENTRY();
                 }
              }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Get file size and check if it fits. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
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
         time_when_transmitted = (time_t)strtol(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = localtime(&time_when_transmitted);
         CONVERT_TIME();
         if (type == FTP)
         {
            (void)memcpy(p_type, FTP_ID_STR, 5);
         }
         else if (type == LOC)
              {
                 (void)memcpy(p_type, FILE_ID_STR, 5);
              }
         else if (type == HTTP)
              {
                 (void)memcpy(p_type, HTTP_ID_STR, 5);
              }
         else if (type == SMTP)
              {
                 (void)memcpy(p_type, SMTP_ID_STR, 5);
              }
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 (void)memcpy(p_type, SCP_ID_STR, 5);
              }
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 (void)memcpy(p_type, WMO_ID_STR, 5);
              }
#endif
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 (void)memcpy(p_type, MAP_ID_STR, 5);
              }
#endif
              else
              {
                 (void)memcpy(p_type, UNKNOWN_ID_STR, 5);
              }
         j = 0;
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

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
               char          *p_start_log_file,
               off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

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
                  if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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
         else if (type == LOC)
              {
                 if (toggles_set & SHOW_FILE)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(HTTP_ID_STR);
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
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(SCP_ID_STR);
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
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(FTPS_ID_STR);
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
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(HTTPS_ID_STR);
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
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       INSERT_TIME_TYPE(SMTPS_ID_STR);
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
#endif /* WITH_SSL */
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
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

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         SET_FILE_NAME_POINTER();
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }

         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
#ifndef _MACRO_DEBUG
         COMMON_BLOCK();
#else
         ptr++;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         tmp_ptr = ptr - 1;
         j = 0;
         while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
         {
            *(p_tt - j) = *tmp_ptr;
            tmp_ptr--; j++;
         }
         if (j == MAX_DISPLAYED_TRANSFER_TIME)
         {
            tmp_ptr = ptr - 4;
            j = 0;
            while ((*tmp_ptr != SEPARATOR_CHAR) && (j < MAX_DISPLAYED_TRANSFER_TIME))
            {
               *(p_tt - j) = *tmp_ptr;
               tmp_ptr--; j++;
            }
            if ((j == MAX_DISPLAYED_TRANSFER_TIME) && (*tmp_ptr != SEPARATOR_CHAR))
            {
               *(p_tt - j) = '>';
               while (*tmp_ptr != SEPARATOR_CHAR)
               {
                  tmp_ptr--;
               }
            }
            else
            {
               while (j < MAX_DISPLAYED_TRANSFER_TIME)
               {
                  *(p_tt - j) = ' ';
                  j++;
               }
            }
         }
         tmp_ptr++;

         ptr++;
         il[file_no].offset[item_counter] = (int)(ptr - p_start_log_file + offset);

         if ((search_directory_name[0] != '\0') ||
             ((current_search_host != -1) &&
              (search_user[current_search_host][0] != '\0')))
         {
            int  count = 0;
            char job_id_str[15];

            while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR) && (count < 15))
            {
               job_id_str[count] = *ptr;
               count++; ptr++;
            }
            job_id_str[count] = '\0';
            id.job_no = (unsigned int)strtoul(job_id_str, NULL, 16);

            if ((current_search_host != -1) &&
                (search_user[current_search_host][0] != '\0'))
            {
               char *at_ptr = search_user[current_search_host];

               id.user[0] = '\0';
               id.mail_destination[0] = '\0';
               get_info(GOT_JOB_ID_USER_ONLY);

               while ((*at_ptr != ' ') && (*at_ptr != '@') && (*at_ptr != '\0'))
               {
                  at_ptr++;
               }
               if ((*at_ptr == '@') && (id.mail_destination[0] != '\0'))
               {
                  if (sfilter(search_user[current_search_host],
                              id.mail_destination, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
               else
               {
                  if (sfilter(search_user[current_search_host], id.user, ' ') != 0)
                  {
                     IGNORE_ENTRY();
                  }
               }
            }
            if (search_directory_name[0] != '\0')\
            {
               id.dir[0] = '\0';
               get_info(GOT_JOB_ID_DIR_ONLY);
               count = strlen(id.dir);
               id.dir[count] = SEPARATOR_CHAR;
               id.dir[count + 1] = '\0';

               if (sfilter(search_directory_name, id.dir, SEPARATOR_CHAR) != 0)
               {
                  IGNORE_ENTRY();
               }
            }
         }
         else
         {
            while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))
            {
               ptr++;
            }
         }
         trans_time += strtod(tmp_ptr, NULL);
         ptr++;
         while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))
         {
            ptr++;
         }

         if (*ptr == SEPARATOR_CHAR)
         {
            int  sub_dir_counter = 0;
            char archive_status = 'Y';

            ptr++;
            while (*ptr != '\n')
            {
               if ((*ptr == '/') && (*(ptr - 1) != '\\'))
               {
                  sub_dir_counter++;
                  if (sub_dir_counter == 3)
                  {
                     int  cc = 0;
                     char long_no[MAX_INT_LENGTH];

                     ptr += 1;
                     while ((*ptr != '_') && (*ptr != '\n') && (cc < MAX_INT_LENGTH))
                     {
                        long_no[cc] = *ptr;
                        cc++; ptr++;
                     }
                     if ((*ptr != '\n') && (cc > 0) && (cc < MAX_INT_LENGTH))
                     {
                        time_t delete_time;

                        long_no[cc] = '\0';
                        delete_time = (time_t)strtol(long_no, (char **)NULL, 16);
                        if (now > (delete_time + ARCHIVE_STEP_TIME))
                        {
                           archive_status = 'D';
                        }
                        else if (now > (delete_time - 5))
                             {
                                archive_status = '?';
                             }
                     }
                  }
               }
               ptr++;
            }
            while (*ptr != '\n')
            {
               ptr++;
            }
            *p_archive_flag = archive_status;
            il[file_no].archived[item_counter] = 1;
         }
         else
         {
            *p_archive_flag = 'N';
         }
         item_counter++;

         str_list[i] = XmStringCreateLocalized(line);

         ptr++;
#endif /* _MACRO_DEBUG */
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_name_and_recipient() ---------------------*/
static void
file_name_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file,
                        off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

         current_search_host = -1;
         ptr_start_line = ptr;

         type = (int)(*(ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 1) - 48);
         if (type == FTP)
         {
            FILE_NAME_AND_RECIPIENT(SHOW_FTP, FTP_ID_STR);
         }
         else if (type == LOC)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_FILE, FILE_ID_STR);
              }
         else if (type == HTTP)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_HTTP, HTTP_ID_STR);
              }
         else if (type == SMTP)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_SMTP, SMTP_ID_STR);
              }
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_SCP, SCP_ID_STR);
              }
#endif /* _WITH_SCP_SUPPORT */
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_FTPS, FTPS_ID_STR);
              }
         else if (type == HTTPS)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_HTTPS, HTTPS_ID_STR);
              }
         else if (type == SMTPS)
              {
                 FILE_NAME_AND_RECIPIENT(SHOW_SMTPS, SMTPS_ID_STR);
              }
#endif
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                    {
                       current_search_host = ii;
                       break;
                    }
                 }
                 if (current_search_host != -1)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) == 0)
                    {
                       il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
                       INSERT_TIME_TYPE(UNKNOWN_ID_STR);
                       j = 0;
                       while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
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
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

   il[file_no].no_of_items = item_counter;

   return;
}


/*----------------------- file_size_and_recipient() ---------------------*/
static void
file_size_and_recipient(register char *ptr,
                        char          *ptr_end,
                        int           file_no,
                        char          *p_start_log_file,
                        off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    FILE_SIZE_AND_RECIPIENT(HTTP_ID_STR);
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    FILE_SIZE_AND_RECIPIENT(SCP_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* _WITH_SCP_SUPPORT */
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    FILE_SIZE_AND_RECIPIENT(FTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    FILE_SIZE_AND_RECIPIENT(HTTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    FILE_SIZE_AND_RECIPIENT(SMTPS_ID_STR);
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }
#endif /* WITH_SSL */
              else
              {
                 FILE_SIZE_AND_RECIPIENT(UNKNOWN_ID_STR);
              }

         ptr = ptr_start_line + 11 + MAX_HOSTNAME_LENGTH + 3;
         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);
         time_when_transmitted = (time_t)strtol(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = localtime(&time_when_transmitted);
         CONVERT_TIME();
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         (void)memcpy(p_host_name, ptr_start_line + 11, MAX_HOSTNAME_LENGTH);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* File size is already stored. */
         while (*ptr != SEPARATOR_CHAR)
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
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

   il[file_no].no_of_items = item_counter;

   return;
}


/*---------------------- file_name_size_recipient() ---------------------*/
static void
file_name_size_recipient(register char *ptr,
                         char          *ptr_end,
                         int           file_no,
                         char          *p_start_log_file,
                         off_t         offset)
{
   register int i,
                j;
   int          current_search_host,
                type,
                item_counter = il[file_no].no_of_items,
                prev_item_counter = il[file_no].no_of_items,
                ptr_is_remote,
                loops = 0,
                unmanaged;
   time_t       now,
                prev_time_val = 0L,
                time_when_transmitted = 0L;
   double       tmp_file_size;
   char         *tmp_ptr,
                *ptr_start_line;
   struct tm    *p_ts;

   if (item_counter == 0)
   {
      XtUnmanageChild(listbox_w);
      unmanaged = YES;
   }
   else
   {
      unmanaged = NO;
   }
   do
   {
      /* Allocate memory for offset to job ID. */
      REALLOC_OFFSET_BUFFER();

      for (i = 0; ((i < LINES_BUFFERED) && (ptr < ptr_end)); i++)
      {
         if (((i % 200) == 0) &&
             ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL))
         {
            prev_time_val = now;
            CHECK_INTERRUPT();
            if (special_button_flag != STOP_BUTTON)
            {
               loops--;
               break;
            }
         }

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
                  if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                  {
                     current_search_host = ii;
                     break;
                  }
               }
               if (current_search_host != -1)
               {
                  SET_FILE_NAME_POINTER();
                  if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
         else if (type == HTTP)
              {
                 if (toggles_set & SHOW_HTTP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 if (toggles_set & SHOW_SCP)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 if (toggles_set & SHOW_WMO)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 if (toggles_set & SHOW_FTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
         else if (type == HTTPS)
              {
                 if (toggles_set & SHOW_HTTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
         else if (type == SMTPS)
              {
                 if (toggles_set & SHOW_SMTPS)
                 {
                    int ii;

                    for (ii = 0; ii < no_of_search_hosts; ii++)
                    {
                       if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                       {
                          current_search_host = ii;
                          break;
                       }
                    }
                    if (current_search_host != -1)
                    {
                       SET_FILE_NAME_POINTER();
                       if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
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
#endif /* WITH_SSL */
              else
              {
                 int ii;

                 for (ii = 0; ii < no_of_search_hosts; ii++)
                 {
                    if (sfilter(search_recipient[ii], ptr_start_line + 11, ' ') == 0)
                    {
                       current_search_host = ii;
                       break;
                    }
                 }
                 if (current_search_host != -1)
                 {
                    SET_FILE_NAME_POINTER();
                    if (sfilter(search_file_name, ptr, SEPARATOR_CHAR) != 0)
                    {
                       IGNORE_ENTRY();
                    }
                 }
                 else
                 {
                    IGNORE_ENTRY();
                 }
              }

         il[file_no].line_offset[item_counter] = (off_t)(ptr_start_line - p_start_log_file + offset);

         /* If necessary, ignore rest of file name. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Get file size and check if it fits. */
         j = 0;
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++; j++;
         }
         if (j < 9)
         {
            tmp_file_size = (double)strtoul(ptr - j, NULL, 16);
         }
         else
         {
            if (j > 20)
            {
               tmp_file_size = strtod("INF", NULL);
            }
            else
            {
               char tmp_buf[23];

               tmp_buf[0] = '0'; tmp_buf[1] = 'x';
               (void)memcpy(&tmp_buf[2], ptr - j, j);
               tmp_buf[2 + j] = '\0';
               tmp_file_size = strtod(tmp_buf, NULL);
            }
         }
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
         time_when_transmitted = (time_t)strtol(ptr_start_line, NULL, 16);
         if (first_date_found == -1)
         {
            first_date_found = time_when_transmitted;
         }
         p_ts = localtime(&time_when_transmitted);
         CONVERT_TIME();
         if (type == FTP)
         {
            (void)memcpy(p_type, FTP_ID_STR, 5);
         }
         else if (type == LOC)
              {
                 (void)memcpy(p_type, FILE_ID_STR, 5);
              }
         else if (type == HTTP)
              {
                 (void)memcpy(p_type, HTTP_ID_STR, 5);
              }
         else if (type == SMTP)
              {
                 (void)memcpy(p_type, SMTP_ID_STR, 5);
              }
#ifdef _WITH_SCP_SUPPORT
         else if (type == SCP)
              {
                 (void)memcpy(p_type, SCP_ID_STR, 5);
              }
#endif
#ifdef _WITH_WMO_SUPPORT
         else if (type == WMO)
              {
                 (void)memcpy(p_type, WMO_ID_STR, 5);
              }
#endif
#ifdef _WITH_MAP_SUPPORT
         else if (type == MAP)
              {
                 (void)memcpy(p_type, MAP_ID_STR, 5);
              }
#endif
#ifdef WITH_SSL
         else if (type == FTPS)
              {
                 (void)memcpy(p_type, FTPS_ID_STR, 5);
              }
         else if (type == HTTPS)
              {
                 (void)memcpy(p_type, HTTPS_ID_STR, 5);
              }
         else if (type == SMTPS)
              {
                 (void)memcpy(p_type, SMTPS_ID_STR, 5);
              }
#endif
              else
              {
                 (void)memcpy(p_type, UNKNOWN_ID_STR, 5);
              }
         if (file_name_toggle_set == REMOTE_FILENAME)
         {
            tmp_ptr = ptr;
            while (*tmp_ptr != SEPARATOR_CHAR)
            {
               tmp_ptr++;
            }
            if (*(tmp_ptr + 1) != SEPARATOR_CHAR)
            {
               ptr = tmp_ptr + 1;
               ptr_is_remote = YES;
            }
            else
            {
               ptr_is_remote = NO;
            }
         }
         else
         {
            ptr_is_remote = NO;
         }
         j = 0;
         while ((*ptr != SEPARATOR_CHAR) && (j < file_name_length))
         {
            *(p_file_name + j) = *ptr;
            ptr++; j++;
         }
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         ptr++;
         if (ptr_is_remote == NO)
         {
            /* Ignore  the remote file name */
            while (*ptr != SEPARATOR_CHAR)
            {
               ptr++;
            }
            ptr++;
         }

         /* Write file size. */
         while (*ptr != SEPARATOR_CHAR)
         {
            ptr++;
         }
         print_file_size(p_file_size, (off_t)tmp_file_size);

         /* Write transfer duration, job ID and archive directory. */
         /* Also check if we have to check for directory name.     */
         COMMON_BLOCK();
         file_size += tmp_file_size;
      }

      loops++;

      /* Display what we have in buffer. */
      display_data(i, first_date_found, time_when_transmitted);

      /* Check if user has done anything. */
      if ((time(&now) - prev_time_val) > CHECK_TIME_INTERVAL)
      {
         prev_time_val = now;
         CHECK_INTERRUPT();
      }

      CHECK_LIST_LIMIT();
   } while ((ptr < ptr_end) && (special_button_flag == STOP_BUTTON));

   if (unmanaged == YES)
   {
      XtManageChild(listbox_w);
   }

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
   XmString     xstr;
   XExposeEvent xeev;
   Dimension    w, h;

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

   calculate_summary(summary_str, first_date_found, time_when_transmitted,
                     total_no_files, file_size, trans_time);
   (void)strcpy(total_summary_str, summary_str);

   xeev.type = Expose;
   xeev.display = display;
   xeev.window = main_window;
   xeev.x = 0; xeev.y = 0;
   XtVaGetValues(summarybox_w, XmNwidth, &w, XmNheight, &h, NULL);
   xeev.width = w; xeev.height = h;
   xstr = XmStringCreateLtoR(summary_str, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(summarybox_w, XmNlabelString, xstr, NULL);
   (XtClass(summarybox_w))->core_class.expose(summarybox_w, (XEvent *)&xeev, NULL);
   XmStringFree(xstr);
   xstr = XmStringCreateLtoR(status_message, XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(statusbox_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}
