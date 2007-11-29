/*
 *  get_remote_file_names_http.c - Part of AFD, an automatic file distribution
 *                                 program.
 *  Copyright (c) 2006, 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names_http - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_http(off_t *file_size_to_retrieve)
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
 **   09.08.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime()                   */ 
#include <ctype.h>                 /* isdigit()                          */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "httpdefs.h"
#include "fddefs.h"


#define STORE_HTML_STRING(html_str, max_str_length)                \
        {                                                          \
           int i = 0;                                              \
                                                                   \
           while ((*ptr != '<') && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (i < (max_str_length)))        \
           {                                                       \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*(ptr + 1))                            \
                    {                                              \
                       case 'a': (html_str)[i++] = 228;            \
                                 break;                            \
                       case 'A': (html_str)[i++] = 196;            \
                                 break;                            \
                       case 'o': (html_str)[i++] = 246;            \
                                 break;                            \
                       case 'O': (html_str)[i++] = 214;            \
                                 break;                            \
                       case 'u': (html_str)[i++] = 252;            \
                                 break;                            \
                       case 'U': (html_str)[i++] = 220;            \
                                 break;                            \
                       case 's': (html_str)[i++] = 223;            \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              (html_str)[i] = *ptr;                                \
              i++; ptr++;                                          \
           }                                                       \
           (html_str)[i] = '\0';                                   \
        }
#define STORE_HTML_DATE()                                          \
        {                                                          \
           int i = 0,                                              \
               space_counter = 0;                                  \
                                                                   \
           while ((*ptr != '<') && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (i < MAX_FILENAME_LENGTH))     \
           {                                                       \
              if (*ptr == ' ')                                     \
              {                                                    \
                 if (space_counter == 1)                           \
                 {                                                 \
                    while (*ptr == ' ')                            \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    break;                                         \
                 }                                                 \
                 space_counter++;                                  \
              }                                                    \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*(ptr + 1))                            \
                    {                                              \
                       case 'a': date_str[i++] = 228;              \
                                 break;                            \
                       case 'A': date_str[i++] = 196;              \
                                 break;                            \
                       case 'o': date_str[i++] = 246;              \
                                 break;                            \
                       case 'O': date_str[i++] = 214;              \
                                 break;                            \
                       case 'u': date_str[i++] = 252;              \
                                 break;                            \
                       case 'U': date_str[i++] = 220;              \
                                 break;                            \
                       case 's': date_str[i++] = 223;              \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              date_str[i] = *ptr;                                  \
              i++; ptr++;                                          \
           }                                                       \
           date_str[i] = '\0';                                     \
        }

/* External global variables. */
extern int                        exitflag,
                                  *no_of_listed_files,
                                  rl_fd,
                                  timeout_flag;
extern char                       msg_str[],
                                  *p_work_dir;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        nfg;           /* Number of file mask. */
static time_t                     current_time;
static struct file_mask           *fml = NULL;

/* Local function prototypes. */
static int                        check_list(char *, time_t, off_t, off_t, off_t *),
                                  check_name(char *),
                                  eval_html_dir_list(char *, int *, off_t *);
static off_t                      convert_size(char *, off_t *);
static void                       remove_ls_data(void);

/* #define MALLOC_INSTEAD_OF_MMAP_TEST */

/*#################### get_remote_file_names_http() #####################*/
int
get_remote_file_names_http(off_t *file_size_to_retrieve)
{
   int       files_to_retrieve = 0,
             i,
             j,
             status;
   time_t    now;
   struct tm *p_tm;

   /* Get all file masks for this directory. */
   if (read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to get the file masks.");
      if (fml != NULL)
      {
         free(fml);
      }
      http_quit();
      exit(INCORRECT);
   }

   if (fra[db.fra_pos].ignore_file_time != 0)
   {
      /* Note: FTP returns GMT so we need to convert this to GMT! */
      current_time = time(NULL);
      now = current_time;
      p_tm = gmtime(&current_time);
      current_time = mktime(p_tm);
   }
   else
   {
      now = 0;
   }

   /*
    * First determine if user wants to try and get a filename
    * listing. This can be done by setting the diretory option
    * 'do not get dir list' in DIR_CONFIG.
    */
   *file_size_to_retrieve = 0;
   if ((fra[db.fra_pos].dir_flag & DONT_GET_DIR_LIST) == 0)
   {
      off_t bytes_buffered = 0,
            content_length;
      char  *listbuffer = NULL;

      if (((status = http_get(db.hostname, db.target_dir, "",
                              &content_length, 0)) != SUCCESS) &&
          (status != CHUNKED))
      {
         remove_ls_data();
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "Failed to open remote directory %s (%d).",
                   db.target_dir, status);
         http_quit();
         exit(eval_timeout(OPEN_REMOTE_ERROR));
      }
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "Opened HTTP connection for directory %s.",
                      db.target_dir);
      }

      if (status == SUCCESS)
      {
         if (content_length > MAX_HTTP_DIR_BUFFER)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "Directory length buffer is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                       "Directory length buffer is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                       MAX_HTTP_DIR_BUFFER, (pri_off_t)content_length);
            http_quit();
            exit(ALLOC_ERROR);
         }
         if ((listbuffer = malloc(content_length + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "Failed to malloc() %ld bytes : %s",
#else
                       "Failed to malloc() %lld bytes : %s",
#endif
                       (pri_off_t)(content_length + 1), strerror(errno));
            http_quit();
            exit(ALLOC_ERROR);
         }
         do
         {
            if ((status = http_read(&listbuffer[bytes_buffered],
                                    fsa->block_size)) == INCORRECT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                         "Failed to read from remote directory listing for %s",
                         db.target_dir);
               free(listbuffer);
               http_quit();
               exit(eval_timeout(READ_REMOTE_ERROR));
            }
            else if (status > 0)
                 {
                    bytes_buffered += status;
                 }
         } while (status != 0);
      }
      else /* status == CHUNKED */
      {
         int  chunksize;
         char *chunkbuffer = NULL;

         if ((chunkbuffer = malloc(fsa->block_size + 4)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() %d bytes : %s",
                       fsa->block_size + 4, strerror(errno));
            http_quit();
            exit(ALLOC_ERROR);
         }
         chunksize = fsa->block_size + 4;
         do
         {
            if ((status = http_chunk_read(&chunkbuffer,
                                          &chunksize)) == INCORRECT)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                         "Failed to read from remote directory listing for %s",
                         db.target_dir);
               free(chunkbuffer);
               http_quit();
               exit(eval_timeout(READ_REMOTE_ERROR));
            }
            else if (status > 0)
                 {
                    if (listbuffer == NULL)
                    {
                       if ((listbuffer = malloc(status)) == NULL)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() %d bytes : %s",
                                     status, strerror(errno));
                          free(chunkbuffer);
                          http_quit();
                          exit(ALLOC_ERROR);
                       }
                    }
                    else
                    {
                       if (bytes_buffered > MAX_HTTP_DIR_BUFFER)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                     "Directory length buffer is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                                     "Directory length buffer is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                                     MAX_HTTP_DIR_BUFFER,
                                     (pri_off_t)content_length);
                          http_quit();
                          free(listbuffer);
                          free(chunkbuffer);
                          exit(ALLOC_ERROR);
                       }
                       if ((listbuffer = realloc(listbuffer,
                                                 bytes_buffered + status)) == NULL)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                     "Failed to realloc() %ld bytes : %s",
#else
                                     "Failed to realloc() %lld bytes : %s",
#endif
                                     (pri_off_t)(bytes_buffered + status),
                                     strerror(errno));
                          free(chunkbuffer);
                          http_quit();
                          exit(ALLOC_ERROR);
                       }
                    }
                    (void)memcpy(&listbuffer[bytes_buffered], chunkbuffer,
                                 status);
                    bytes_buffered += status;
                 }
         } while (status != 0);

         if ((listbuffer = realloc(listbuffer, bytes_buffered + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "Failed to realloc() %ld bytes : %s",
#else
                       "Failed to realloc() %lld bytes : %s",
#endif
                       (pri_off_t)(bytes_buffered + 1), strerror(errno));
            free(chunkbuffer);
            http_quit();
            exit(ALLOC_ERROR);
         }

         free(chunkbuffer);
      }

      if (bytes_buffered > 0)
      {
         listbuffer[bytes_buffered] = '\0';
         if (eval_html_dir_list(listbuffer, &files_to_retrieve,
                                file_size_to_retrieve) == INCORRECT)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                      "Failed to evaluate HTML directory listing.");
         }
      }
      if (listbuffer != NULL)
      {
         free(listbuffer);
      }
   }
   else /* Just copy the file mask list. */
   {
      time_t file_mtime;
      off_t  file_size;
      char   *p_mask,
             tmp_mask[MAX_FILENAME_LENGTH];

      if (now == 0)
      {
         now = time(NULL);
      }

      for (i = 0; i < nfg; i++)
      {
         p_mask = fml[i].file_list;
         for (j = 0; j < fml[i].fc; j++)
         {
            /*
             * We cannot just take the mask as is. We must check if we
             * need to expand the mask and then use the expansion.
             */
            expand_filter(p_mask, tmp_mask, now);
            if ((status = http_head(db.hostname, db.target_dir, tmp_mask,
                                    &file_size, &file_mtime)) == SUCCESS)
            {
               off_t exact_size;

               if (fsa->debug > NORMAL_MODE)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                               "Date for %s is %ld, size = %ld bytes.",
# else
                               "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                               "Date for %s is %ld, size = %lld bytes.",
# else
                               "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                               tmp_mask, (pri_time_t)file_mtime,
                               (pri_off_t)file_size);
               }
               if (file_size == -1)
               {
                  exact_size = 0;
               }
               else
               {
                  exact_size = 1;
               }
               if (check_list(tmp_mask, file_mtime, exact_size, file_size,
                              file_size_to_retrieve) == 0)
               {
                  files_to_retrieve++;
               }
            }
            NEXT(p_mask);
         }
      }
   }

   /* Free file mask list. */
   for (i = 0; i < nfg; i++)
   {
      free(fml[i].file_list);
   }
   free(fml);

   /*
    * Remove all files from the remote_list structure that are not
    * in the current directory listing.
    */
   if ((files_to_retrieve > 0) && (fra[db.fra_pos].stupid_mode != YES) &&
       (fra[db.fra_pos].remove == NO))
   {
      int    files_removed = 0,
             i;
      size_t move_size;

      for (i = 0; i < (*no_of_listed_files - files_removed); i++)
      {
         if (rl[i].in_list == NO)
         {
            int j = i;

            while ((rl[j].in_list == NO) &&
                   (j < (*no_of_listed_files - files_removed)))
            {
               j++;
            }
            if (j != (*no_of_listed_files - files_removed))
            {
               move_size = (*no_of_listed_files - files_removed - j) *
                           sizeof(struct retrieve_list);
               (void)memmove(&rl[i], &rl[j], move_size);
            }
            files_removed += (j - i);
         }
      }

      if (files_removed > 0)
      {
         int    current_no_of_listed_files = *no_of_listed_files;
         size_t new_size,
                old_size;

         *no_of_listed_files -= files_removed;
         if (*no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
            *no_of_listed_files = 0;
         }
         if (*no_of_listed_files == 0)
         {
            new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;
         }
         else
         {
            new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                        RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;
         }
         old_size = (((current_no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                     RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;

         if (old_size != new_size)
         {
            char   *ptr;

            ptr = (char *)rl - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap_resize() error : %s", strerror(errno));
               http_quit();
               exit(INCORRECT);
            }
            no_of_listed_files = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            rl = (struct retrieve_list *)ptr;
            if (*no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
               *no_of_listed_files = 0;
            }
         }
      }
   }

   return(files_to_retrieve);
}


/*++++++++++++++++++++++++ eval_html_dir_list() +++++++++++++++++++++++++*/
static int
eval_html_dir_list(char  *html_buffer,
                   int   *files_to_retrieve,
                   off_t *file_size_to_retrieve)
{
   char *ptr;

   if ((ptr = posi(html_buffer, "<h1>")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
      return(INCORRECT);
   }
   else
   {
      time_t file_mtime;
      off_t  exact_size,
             file_size;
      char   date_str[MAX_FILENAME_LENGTH],
             file_name[MAX_FILENAME_LENGTH],
             size_str[MAX_FILENAME_LENGTH];

      while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
      {
         ptr++;
      }
      while ((*ptr == '\n') || (*ptr == '\r'))
      {
         ptr++;
      }
      if (*ptr == '<')
      {
         /* Table type listing. */
         if ((*(ptr + 1) == 't') && (*(ptr + 6) == '>'))
         {
            /* Ignore the two heading lines. */
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }

            if ((*ptr == '<') && (*(ptr + 1) == 't') && (*(ptr + 2) == 'r') &&
                (*(ptr + 3) == '>') && (*(ptr + 4) == '<') &&
                (*(ptr + 5) == 't') && (*(ptr + 6) == 'd'))
            {
               /* Read line by line. */
               do
               {
                  ptr += 6;
                  while ((*ptr != '>') &&
                         (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == '>')
                  {
                     ptr++;
                     while (*ptr == '<')
                     {
                        ptr++;
                        while ((*ptr != '>') && (*ptr != '\n') &&
                               (*ptr != '\r') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == '>')
                        {
                           ptr++;
                        }
                     }
                     if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        /* Store file name. */
                        STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                        if (check_name(file_name) == YES)
                        {
                           while (*ptr == '<')
                           {
                              ptr++;
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                              }
                           }
                           if ((*ptr != '\n') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                           {
                              while (*ptr == ' ')
                              {
                                 ptr++;
                              }

                              /* Store date string. */
                              STORE_HTML_STRING(date_str, MAX_FILENAME_LENGTH);
                              file_mtime = datestr2unixtime(date_str);

                              while (*ptr == '<')
                              {
                                 ptr++;
                                 while ((*ptr != '>') && (*ptr != '\n') &&
                                        (*ptr != '\r') && (*ptr != '\0'))
                                 {
                                    ptr++;
                                 }
                                 if (*ptr == '>')
                                 {
                                    ptr++;
                                 }
                              }
                              if ((*ptr != '\n') && (*ptr != '\r') &&
                                  (*ptr != '\0'))
                              {
                                 /* Store size string. */
                                 STORE_HTML_STRING(size_str,
                                                   MAX_FILENAME_LENGTH);
                                 exact_size = convert_size(size_str, &file_size);
                              }
                              else
                              {
                                 exact_size = -1;
                                 file_size = -1;
                              }
                           }
                           else
                           {
                              file_mtime = -1;
                              exact_size = -1;
                              file_size = -1;
                           }
                        }
                        else
                        {
                           file_name[0] = '\0';
                        }
                     }
                     else
                     {
                        file_name[0] = '\0';
                        file_mtime = -1;
                        exact_size = -1;
                        file_size = -1;
                     }
                  }

                  if (file_name[0] != '\0')
                  {
                     if (check_list(file_name, file_mtime, exact_size,
                                    file_size, file_size_to_retrieve) == 0)
                     {
                        (*files_to_retrieve)++;
                     }
                  }

                  /* Go to end of line. */
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
               } while ((*ptr == '<') && (*(ptr + 1) == 't') &&
                        (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                        (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                        (*(ptr + 6) == 'd'));
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
               return(INCORRECT);
            }
         }
              /* Pre type listing. */
         else if ((*(ptr + 1) == 'p') && (*(ptr + 4) == '>'))
              {
                 /* Ignore heading line. */
                 while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 while ((*ptr == '\n') || (*ptr == '\r'))
                 {
                    ptr++;
                 }

                 while (*ptr == '<')
                 {
                    while (*ptr == '<')
                    {
                       ptr++;
                       while ((*ptr != '>') && (*ptr != '\n') &&
                              (*ptr != '\r') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       if (*ptr == '>')
                       {
                          ptr++;
                          while (*ptr == ' ')
                          {
                             ptr++;
                          }
                       }
                    }

                    if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       /* Store file name. */
                       STORE_HTML_STRING(file_name, MAX_FILENAME_LENGTH);

                       if (check_name(file_name) == YES)
                       {
                          if (*ptr == '<')
                          {
                             while (*ptr == '<')
                             {
                                ptr++;
                                while ((*ptr != '>') && (*ptr != '\n') &&
                                       (*ptr != '\r') && (*ptr != '\0'))
                                {
                                   ptr++;
                                }
                                if (*ptr == '>')
                                {
                                   ptr++;
                                   while (*ptr == ' ')
                                   {
                                      ptr++;
                                   }
                                }
                             }
                          }
                          if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                          {
                             while (*ptr == ' ')
                             {
                                ptr++;
                             }

                             /* Store date string. */
                             STORE_HTML_DATE();
                             file_mtime = datestr2unixtime(date_str);

                             if (*ptr == '<')
                             {
                                while (*ptr == '<')
                                {
                                   ptr++;
                                   while ((*ptr != '>') && (*ptr != '\n') &&
                                          (*ptr != '\r') && (*ptr != '\0'))
                                   {
                                      ptr++;
                                   }
                                   if (*ptr == '>')
                                   {
                                      ptr++;
                                      while (*ptr == ' ')
                                      {
                                         ptr++;
                                      }
                                   }
                                }
                             }
                             if ((*ptr != '\n') && (*ptr != '\r') &&
                                 (*ptr != '\0'))
                             {
                                /* Store size string. */
                                STORE_HTML_STRING(size_str,
                                                  MAX_FILENAME_LENGTH);
                                exact_size = convert_size(size_str, &file_size);
                             }
                             else
                             {
                                exact_size = -1;
                                file_size = -1;
                             }
                          }
                          else
                          {
                             file_mtime = -1;
                             exact_size = -1;
                             file_size = -1;
                          }
                       }
                       else
                       {
                          file_name[0] = '\0';
                       }
                    }
                    else
                    {
                       file_name[0] = '\0';
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                       break;
                    }

                     if (file_name[0] != '\0')
                     {
                        if (check_list(file_name, file_mtime, exact_size,
                                       file_size, file_size_to_retrieve) == 0)
                        {
                           (*files_to_retrieve)++;
                        }
                     }

                    /* Go to end of line. */
                    while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }
                 }
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                           "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
                 return(INCORRECT);
              }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   "Unknown HTML directory listing. Please send author a link so that this can be implemented.");
         return(INCORRECT);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char   *file,
           time_t file_mtime,
           off_t  exact_size,
           off_t  file_size,
           off_t  *file_size_to_retrieve)
{
   int status;

   if ((fra[db.fra_pos].stupid_mode == YES) ||
       (fra[db.fra_pos].remove == YES))
   {
      if (rl == NULL)
      {
         size_t rl_size;
         char   *ptr;

         rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                   AFD_WORD_OFFSET;
         if ((ptr = malloc(rl_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
         no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
         *no_of_listed_files = 0;
      }
      else if ((*no_of_listed_files != 0) &&
               ((*no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
           {
              char   *ptr;
              size_t rl_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                               AFD_WORD_OFFSET;

              ptr = (char *)rl - AFD_WORD_OFFSET;
              if ((ptr = realloc(ptr, rl_size)) == NULL)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "realloc() error : %s", strerror(errno));
                 http_quit();
                 exit(INCORRECT);
              }
              no_of_listed_files = (int *)ptr;
              ptr += AFD_WORD_OFFSET;
              rl = (struct retrieve_list *)ptr;
              if (*no_of_listed_files < 0)
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            "Hmmm, no_of_listed_files = %d",
                            *no_of_listed_files);
                 *no_of_listed_files = 0;
              }
           }

      if ((file_mtime == -1) && (fra[db.fra_pos].ignore_file_time != 0))
      {
         if ((status = http_head(db.hostname, db.target_dir, file,
                                 &file_size, &file_mtime)) == SUCCESS)
         {
            exact_size = 1;
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                            "Date for %s is %ld, size = %ld bytes.",
# else
                            "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                            "Date for %s is %ld, size = %lld bytes.",
# else
                            "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                            file, (pri_time_t)file_mtime,
                            (pri_off_t)file_size);
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                      "Failed to get date and size of file %s (%d).",
                      file, status);
            if (timeout_flag == ON)
            {
               http_quit();
               exit(DATE_ERROR);
            }
         }
      }
      rl[*no_of_listed_files].size = file_size;
      rl[*no_of_listed_files].file_mtime = file_mtime;
      if (file_mtime == -1)
      {
         rl[*no_of_listed_files].got_date = NO;
      }
      else
      {
         rl[*no_of_listed_files].got_date = YES;
      }
   }
   else
   {
      int i;

      if (rl_fd == -1)
      {
         size_t      rl_size;
         char        list_file[MAX_PATH_LENGTH],
                     *ptr;
         struct stat stat_buf;

         (void)sprintf(list_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                       INCOMING_DIR, LS_DATA_DIR, fra[db.fra_pos].dir_alias);
         if ((rl_fd = open(list_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() `%s' : %s",
                       list_file, strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
         if (fstat(rl_fd, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to fstat() `%s' : %s",
                       list_file, strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
         if (stat_buf.st_size == 0)
         {
            rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                      AFD_WORD_OFFSET;
            if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to lseek() in `%s' : %s",
                          list_file, strerror(errno));
               http_quit();
               exit(INCORRECT);
            }
            if (write(rl_fd, "", 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to `%s' : %s",
                          list_file, strerror(errno));
               http_quit();
               exit(INCORRECT);
            }
         }
         else
         {
            rl_size = stat_buf.st_size;
         }
#ifdef MALLOC_INSTEAD_OF_MMAP_TEST
         if ((ptr = malloc(rl_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to malloc() : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
#else
         if ((ptr = mmap(NULL, rl_size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to `%s' : %s",
                       list_file, strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
#endif
         no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
         if (stat_buf.st_size == 0)
         {
            *no_of_listed_files = 0;
            *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
         }
         else
         {
            if (*no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
               *no_of_listed_files = 0;
            }
            else
            {
               if (((stat_buf.st_size - AFD_WORD_OFFSET) % sizeof(struct retrieve_list)) != 0)
               {
                  off_t calc_size;

                  calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                               RETRIEVE_LIST_STEP_SIZE *
                               sizeof(struct retrieve_list)) + AFD_WORD_OFFSET;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                             "Hmm, LS data file %s has incorrect size (%ld != %ld), removing it.",
#else
                             "Hmm, LS data file %s has incorrect size (%lld != %lld), removing it.",
#endif
                             list_file, (pri_off_t)stat_buf.st_size,
                             (pri_off_t)calc_size);
                  ptr -= AFD_WORD_OFFSET;
                  if (munmap(ptr, stat_buf.st_size) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to munmap() %s : %s",
                                list_file, strerror(errno));
                  }
                  if (close(rl_fd) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Failed to close() %s : %s",
                                list_file, strerror(errno));
                  }
                  if ((rl_fd = open(list_file, O_RDWR | O_CREAT | O_TRUNC,
                                    FILE_MODE)) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to open() `%s' : %s",
                                list_file, strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  rl_size = (RETRIEVE_LIST_STEP_SIZE *
                             sizeof(struct retrieve_list)) + AFD_WORD_OFFSET;
                  if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to lseek() in `%s' : %s",
                                list_file, strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  if (write(rl_fd, "", 1) != 1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to write() to `%s' : %s",
                                list_file, strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  if ((ptr = mmap(NULL, rl_size, (PROT_READ | PROT_WRITE),
                                  MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mmap() to `%s' : %s",
                                list_file, strerror(errno));
                     http_quit();
                     exit(INCORRECT);
                  }
                  no_of_listed_files = (int *)ptr;
                  *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                  ptr += AFD_WORD_OFFSET;
                  rl = (struct retrieve_list *)ptr;
                  *no_of_listed_files = 0;
               }
               for (i = 0; i < *no_of_listed_files; i++)
               {
                  rl[i].in_list = NO;
               }
            }
         }
      }

      /* Check if this file is in the list. */
      for (i = 0; i < *no_of_listed_files; i++)
      {
         if (CHECK_STRCMP(rl[i].file_name, file) == 0)
         {
            if ((fra[db.fra_pos].stupid_mode == GET_ONCE_ONLY) &&
                (rl[i].retrieved == YES))
            {
               rl[i].in_list = YES;
               return(1);
            }

            /* Try to get remote date. */
            if ((file_mtime == -1) || (file_size == -1) || (exact_size != 1))
            {
               if ((status = http_head(db.hostname, db.target_dir, file,
                                       &file_size, &file_mtime)) == SUCCESS)
               {
                  exact_size = 1;
                  if (fsa->debug > NORMAL_MODE)
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                  "Date for %s is %ld, size = %ld bytes.",
# else
                                  "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                                  "Date for %s is %ld, size = %lld bytes.",
# else
                                  "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                                  file, (pri_time_t)file_mtime, 
                                  (pri_off_t)file_size);
                  }
               }
               else
               {
                  trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                            __FILE__, __LINE__, msg_str,
                            "Failed to get date and size of file %s (%d).",
                            file, status);
                  if (timeout_flag == ON)
                  {
                     http_quit();
                     exit(DATE_ERROR);
                  }
                  rl[i].got_date = NO;
               }
            }
            if (file_mtime == -1)
            {
               rl[i].got_date = NO;
               rl[i].retrieved = NO;
               rl[i].file_mtime = file_mtime;
            }
            else
            {
               rl[i].got_date = YES;
               if (rl[i].file_mtime != file_mtime)
               {
                  rl[i].file_mtime = file_mtime;
                  rl[i].retrieved = NO;
               }
            }
            if (file_size == -1)
            {
               rl[i].size = file_size;
               rl[i].retrieved = NO;
            }
            else
            {
               if (rl[i].size != file_size)
               {
                  rl[i].size = file_size;
                  rl[i].retrieved = NO;
               }
            }

            rl[i].in_list = YES;
            if (rl[i].retrieved == NO)
            {
               if ((rl[i].size > 0) &&
                   ((fra[db.fra_pos].ignore_size == 0) ||
                    ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                     (fra[db.fra_pos].ignore_size == rl[i].size)) ||
                    ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                     (fra[db.fra_pos].ignore_size < rl[i].size)) ||
                    ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                     (fra[db.fra_pos].ignore_size > rl[i].size))))
               {
                  if ((rl[i].got_date == NO) ||
                      (fra[db.fra_pos].ignore_file_time == 0))
                  {
                     *file_size_to_retrieve += rl[i].size;
                  }
                  else
                  {
                     time_t diff_time;

                     diff_time = current_time - rl[i].file_mtime;
                     if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                          (fra[db.fra_pos].ignore_file_time == diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                          (fra[db.fra_pos].ignore_file_time < diff_time)) ||
                         ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                          (fra[db.fra_pos].ignore_file_time > diff_time)))
                     {
                        *file_size_to_retrieve += rl[i].size;
                     }
                     else
                     {
                        return(1);
                     }
                  }
                  return(0);
               }
               else
               {
                  return(1);
               }
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */

      /* Add this file to the list. */
      if ((*no_of_listed_files != 0) &&
          ((*no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
      {
         char   *ptr;
         size_t new_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                            RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                            AFD_WORD_OFFSET;

         ptr = (char *)rl - AFD_WORD_OFFSET;
#ifdef MALLOC_INSTEAD_OF_MMAP_TEST
         if ((ptr = realloc(ptr, new_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "realloc() error : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
#else
         if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            http_quit();
            exit(INCORRECT);
         }
#endif
         no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
         if (*no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
            *no_of_listed_files = 0;
         }
      }

      if ((file_mtime == -1) || (file_size == -1) || (exact_size != 1))
      {
         if ((status = http_head(db.hostname, db.target_dir, file,
                                 &file_size, &file_mtime)) == SUCCESS)
         {
            if (fsa->debug > NORMAL_MODE)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, msg_str,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                            "Date for %s is %ld, size = %ld bytes.",
# else
                            "Date for %s is %lld, size = %ld bytes.",
# endif
#else
# if SIZEOF_TIME_T == 4
                            "Date for %s is %ld, size = %lld bytes.",
# else
                            "Date for %s is %lld, size = %lld bytes.",
# endif
#endif
                            file, file_mtime, file_size);
            }
         }
         else
         {
            trans_log((timeout_flag == ON) ? ERROR_SIGN : DEBUG_SIGN,
                      __FILE__, __LINE__, msg_str,
                      "Failed to get date and size of file %s (%d).",
                      file, status);
            if (timeout_flag == ON)
            {
               http_quit();
               exit(DATE_ERROR);
            }
         }
      }
      rl[*no_of_listed_files].file_mtime = file_mtime;
      rl[*no_of_listed_files].size = file_size;
      if (file_mtime == -1)
      {
         rl[*no_of_listed_files].got_date = NO;
      }
      else
      {
         rl[*no_of_listed_files].got_date = YES;
      }
   }

   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].retrieved = NO;
   rl[*no_of_listed_files].in_list = YES;
   *file_size_to_retrieve += file_size;

   if ((fra[db.fra_pos].ignore_size == 0) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
        (fra[db.fra_pos].ignore_size == rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
        (fra[db.fra_pos].ignore_size < rl[*no_of_listed_files].size)) ||
       ((fra[db.fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
        (fra[db.fra_pos].ignore_size > rl[*no_of_listed_files].size)))
   {
      if ((rl[*no_of_listed_files].got_date == NO) ||
          (fra[db.fra_pos].ignore_file_time == 0))
      {
         (*no_of_listed_files)++;
      }
      else
      {
         time_t diff_time;

         diff_time = current_time - rl[*no_of_listed_files].file_mtime;
         if (((fra[db.fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
              (fra[db.fra_pos].ignore_file_time == diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
              (fra[db.fra_pos].ignore_file_time < diff_time)) ||
             ((fra[db.fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
              (fra[db.fra_pos].ignore_file_time > diff_time)))
         {
            (*no_of_listed_files)++;
         }
         else
         {
            *file_size_to_retrieve -= rl[*no_of_listed_files].size;
            return(1);
         }
      }
      return(0);
   }
   else
   {
      *file_size_to_retrieve -= rl[*no_of_listed_files].size;
      return(1);
   }
}


/*++++++++++++++++++++++++++ remove_ls_data() +++++++++++++++++++++++++++*/
static void
remove_ls_data(void)
{
   if ((fra[db.fra_pos].stupid_mode != YES) &&
       (fra[db.fra_pos].remove == NO))
   {
      char        list_file[MAX_PATH_LENGTH];
      struct stat stat_buf;

      (void)sprintf(list_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                    INCOMING_DIR, LS_DATA_DIR, fra[db.fra_pos].dir_alias);
      if (stat(list_file, &stat_buf) == 0)
      {
         if ((unlink(list_file) == -1) && (errno != ENOENT))
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to unlink() `%s' : %s",
                       list_file, strerror(errno));
         }
      }
      else
      {
         if (errno != ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to stat() `%s' : %s",
                       list_file, strerror(errno));
         }
      }
   }
   return;
}


/*---------------------------- check_name() -----------------------------*/
static int
check_name(char *file_name)
{
   int  gotcha = NO,
        i,
        j,
        status;
   char *p_mask;

   if ((file_name[0] != '.') ||
       (fra[db.fra_pos].dir_flag & ACCEPT_DOT_FILES))
   {
      for (i = 0; i < nfg; i++)
      {
         p_mask = fml[i].file_list;
         for (j = 0; j < fml[i].fc; j++)
         {
            if ((status = pmatch(p_mask, file_name, NULL)) == 0)
            {
               gotcha = YES;
               break;
            }
            else if (status == 1)
                 {
                    /* This file is definitly NOT wanted! */
                    /* Lets skip the rest of this group.  */
                    break;
                 }
            NEXT(p_mask);
         }
         if (gotcha == YES)
         {
            break;
         }
      }
   }

   return(gotcha);
}


/*------------------------- convert_size() ------------------------------*/
static off_t
convert_size(char *size_str, off_t *size)
{
   off_t exact_size;
   char  *ptr,
         *ptr_start;

   ptr = size_str;
   while (*ptr == ' ')
   {
      ptr++;
   }
   ptr_start = ptr;

   while (isdigit((int)*ptr))
   {
      ptr++;
   }
   if (*ptr == '.')
   {
      ptr++;
      while (isdigit((int)*ptr))
      {
         ptr++;
      }
   }
   if (ptr != ptr_start)
   {
      switch (*ptr)
      {
         case 'K': /* Kilobytes. */
            exact_size = KILOBYTE;
            break;
         case 'M': /* Megabytes. */
            exact_size = MEGABYTE;
            break;
         case 'G': /* Gigabytes. */
            exact_size = GIGABYTE;
            break;
         case 'T': /* Terabytes. */
            exact_size = TERABYTE;
            break;
         case 'P': /* Petabytes. */
            exact_size = PETABYTE;
            break;
         case 'E': /* Exabytes. */
            exact_size = EXABYTE;
            break;
         default :
            exact_size = 1;
            break;
      }
      *size = (off_t)(strtod(ptr_start, (char **)NULL) * exact_size);
   }
   else
   {
      *size = -1;
      exact_size = -1;
   }

   return(exact_size);
}
