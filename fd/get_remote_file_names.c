/*
 *  get_remote_file_names.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2000 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_remote_file_names - searches message directory for any changes
 **
 ** SYNOPSIS
 **   int get_remote_file_names(off_t *file_size_to_retrieve)
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
 **   20.08.2000 H.Kiehl Created
 **   15.07.2002 H.Kiehl Option to ignore files which have a certain size.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "ftpdefs.h"
#include "fddefs.h"

#define REMOTE_LIST_STEP_SIZE 10

/* External global variables */
extern int                        exitflag,
                                  *no_of_listed_files,
                                  rl_fd,
                                  timeout_flag,
                                  trans_db_log_fd;
extern char                       *p_work_dir;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;

/* Local function prototypes. */
static int                        check_list(char *, off_t *);
static void                       remove_ls_data(void);


/*####################### get_remote_file_names() #######################*/
int
get_remote_file_names(off_t *file_size_to_retrieve)
{
   int              files_to_retrieve = 0,
                    gotcha,
                    i, j,
                    nfg,           /* Number of file mask. */
                    status;
   char             *nlist = NULL,
                    *p_end,
                    *p_list,
                    *p_mask;
   struct file_mask *fml;

   /*
    * Get a directory listing from the remote site so we can see
    * what files are there.
    */
   if ((status = ftp_list(db.mode_flag, NLIST_CMD|BUFFERED_LIST,
                          &nlist)) != SUCCESS)
   {
      if (status == 550)
      {
         remove_ls_data();
         trans_log(INFO_SIGN, __FILE__, __LINE__,
                   "Failed to send NLST command (%d).", status);
         (void)ftp_quit();
         exitflag = 0;
         exit(TRANSFER_SUCCESS);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__,
                   "Failed to send NLST command (%d).", status);
         (void)ftp_quit();
         exit(LIST_ERROR);
      }
   }
   else
   {
      if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Send NLST command.");
      }
   }

   /*
    * Some systems return 550 for the NLST command when no files
    * are found, others return 125 (ie. success) but do not return
    * any data. So check here if this is the second case.
    */
   if (nlist == NULL)
   {
      remove_ls_data();
      trans_log(INFO_SIGN, __FILE__, __LINE__,
                "No files found (%d).", status);
      (void)ftp_quit();
      exitflag = 0;
      exit(TRANSFER_SUCCESS);
   }

   /* Get all file masks for this directory. */
   if (read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to get the file masks.");
      (void)ftp_quit();
      exit(INCORRECT);
   }

   /* Reduce the list to what is really required. */
   *file_size_to_retrieve = 0;
   p_list = nlist;
   do
   {
      p_end = p_list;
      while ((*p_end != '\n') && (*p_end != '\r') && (*p_end != '\0'))
      {
         p_end++;
      }
      if (*p_end != '\0')
      {
         *p_end = '\0';
         gotcha = NO;
         for (i = 0; i < nfg; i++)
         {
            p_mask = fml[i].file_list;
            for (j = 0; j < fml[i].nfm; j++)
            {
               if (((status = pmatch(p_mask, p_list)) == 0) &&
                   (check_list(p_list, file_size_to_retrieve) == 0))
               {
                  gotcha = YES;
                  files_to_retrieve++;
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
         p_list = p_end + 1;
         while ((*p_list == '\r') || (*p_list == '\n'))
         {
            p_list++;
         }
      }
      else
      {
         p_list = p_end;
      }
   } while (*p_list != '\0');

   free(nlist);

   /*
    * Remove all files from the remote_list structure that are not
    * in the current nlist buffer.
    */
   if ((files_to_retrieve > 0) && (fra[db.fra_pos].stupid_mode == NO) &&
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
            new_size = (REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;
         }
         else
         {
            new_size = (((*no_of_listed_files / REMOTE_LIST_STEP_SIZE) + 1) *
                        REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                       AFD_WORD_OFFSET;
         }
         old_size = (((current_no_of_listed_files / REMOTE_LIST_STEP_SIZE) + 1) *
                     REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;

         if (old_size != new_size)
         {
            char   *ptr;

            ptr = (char *)rl - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap_resize() error : %s", strerror(errno));
               (void)ftp_quit();
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


/*+++++++++++++++++++++++++++++ check_list() ++++++++++++++++++++++++++++*/
static int
check_list(char *file, off_t *file_size_to_retrieve)
{
   static int check_date = YES,
              check_size = YES;
   int        status;

   if ((fra[db.fra_pos].stupid_mode == YES) ||
       (fra[db.fra_pos].remove == YES))
   {
      if (rl == NULL)
      {
         size_t rl_size;
         char   *ptr;

         rl_size = (REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                   AFD_WORD_OFFSET;
         if ((ptr = malloc(rl_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            (void)ftp_quit();
            exit(INCORRECT);
         }
         no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
         *no_of_listed_files = 0;
      }
      else if ((*no_of_listed_files != 0) &&
               ((*no_of_listed_files % REMOTE_LIST_STEP_SIZE) == 0))
           {
              char   *ptr;
              size_t rl_size = (((*no_of_listed_files / REMOTE_LIST_STEP_SIZE) + 1) *
                                REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                               AFD_WORD_OFFSET;

              ptr = (char *)rl - AFD_WORD_OFFSET;
              if ((ptr = realloc(ptr, rl_size)) == NULL)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "realloc() error : %s", strerror(errno));
                 (void)ftp_quit();
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

      rl[*no_of_listed_files].date[0] = '\0';
   }
   else
   {
      int  i;
      char date_str[MAX_FTP_DATE_LENGTH];

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
                       "Failed to open() <%s> : %s",
                       list_file, strerror(errno));
            (void)ftp_quit();
            exit(INCORRECT);
         }
         if (fstat(rl_fd, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to fstat() <%s> : %s",
                       list_file, strerror(errno));
            (void)ftp_quit();
            exit(INCORRECT);
         }
         if (stat_buf.st_size == 0)
         {
            rl_size = (REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                      AFD_WORD_OFFSET;
            if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to lseek() in <%s> : %s",
                          list_file, strerror(errno));
               (void)ftp_quit();
               exit(INCORRECT);
            }
            if (write(rl_fd, "", 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to <%s> : %s",
                          list_file, strerror(errno));
               (void)ftp_quit();
               exit(INCORRECT);
            }
         }
         else
         {
            rl_size = stat_buf.st_size;
         }
         if ((ptr = mmap(0, rl_size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to <%s> : %s",
                       list_file, strerror(errno));
            (void)ftp_quit();
            exit(INCORRECT);
         }
         no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
         if (stat_buf.st_size == 0)
         {
            *no_of_listed_files = 0;
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
            /* Try to get remote date. */
            if (check_date == YES)
            {
               if ((status = ftp_date(file, date_str)) == SUCCESS)
               {
                  if (memcmp(date_str, rl[i].date, MAX_FTP_DATE_LENGTH) != 0)
                  {
                     (void)memcpy(rl[i].date, date_str, MAX_FTP_DATE_LENGTH);
                     rl[i].retrieved = NO;
                  }
                  if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Date for %s is %s.", file, date_str);
                  }
               }
               else if ((status == 500) || (status == 502))
                    {
                       check_date = NO;
                       if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                       {
                          trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                       "Date command MDTM not supported [%d]",
                                       status);
                       }
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "Failed to get date of file %s.", file);
                       if (timeout_flag == ON)
                       {
                          (void)ftp_quit();
                          exit(DATE_ERROR);
                       }
                       check_date = NO;
                    }
            }

            /* Try to get remote size. */
            if (check_size == YES)
            {
               off_t size;

               if ((status = ftp_size(file, &size)) == SUCCESS)
               {
                  if (rl[i].size != size)
                  {
                     rl[i].size = size;
                     rl[i].retrieved = NO;
                  }
                  if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                  {
                     trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                  "Size for %s is %d.", file, size);
                  }
               }
               else if ((status == 500) || (status == 502))
                    {
                       check_size = NO;
                       if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                       {
                          trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                       "Size command SIZE not supported [%d]",
                                       status);
                       }
                    }
                    else
                    {
                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                 "Failed to get size of file %s.", file);
                       if (timeout_flag == ON)
                       {
                          (void)ftp_quit();
                          exit(DATE_ERROR);
                       }
                       check_size = NO;
                    }
            }

            rl[i].in_list = YES;
            if (rl[i].retrieved == NO)
            {
               if (rl[i].size > 0)
               {
                  *file_size_to_retrieve += rl[i].size;
               }
               return(0);
            }
            else
            {
               return(1);
            }
         }
      } /* for (i = 0; i < *no_of_listed_files; i++) */

      /* Add this file to the list. */
      if ((*no_of_listed_files != 0) &&
          ((*no_of_listed_files % REMOTE_LIST_STEP_SIZE) == 0))
      {
         char   *ptr;
         size_t new_size = (((*no_of_listed_files / REMOTE_LIST_STEP_SIZE) + 1) *
                            REMOTE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                            AFD_WORD_OFFSET;

         ptr = (char *)rl - AFD_WORD_OFFSET;
         if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            (void)ftp_quit();
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
      if (check_date == YES)
      {
         if ((status = ftp_date(file, date_str)) == SUCCESS)
         {
            (void)memcpy(rl[*no_of_listed_files].date, date_str, MAX_FTP_DATE_LENGTH);
            if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Date for %s is %s.", file, date_str);
            }
         }
         else if ((status == 500) || (status == 502))
              {
                 check_date = NO;
                 rl[*no_of_listed_files].date[0] = '\0';
                 if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
                 {
                    trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                                 "Date command MDTM not supported [%d]",
                                 status);
                 }
              }
              else
              {
                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                           "Failed to get date of file %s.", file);
                 if (timeout_flag == ON)
                 {
                    (void)ftp_quit();
                    exit(DATE_ERROR);
                 }
                 rl[*no_of_listed_files].date[0] = '\0';
                 check_date = NO;
              }
      }
      else
      {
         rl[*no_of_listed_files].date[0] = '\0';
      }
   }

   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].retrieved = NO;
   rl[*no_of_listed_files].in_list = YES;
   if (check_size == YES)
   {
      off_t size;

      if ((status = ftp_size(file, &size)) == SUCCESS)
      {
         rl[*no_of_listed_files].size = size;
         *file_size_to_retrieve += size;
         if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                         "Size for %s is %d.", file, size);
         }
      }
      else if ((status == 500) || (status == 502))
           {
              check_size = NO;
              rl[*no_of_listed_files].size = -1;
              if ((fsa[db.fsa_pos].debug == YES) && (trans_db_log_fd != -1))
              {
                 trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                              "Size command SIZE not supported [%d]", status);
              }
           }
           else
           {
              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                        "Failed to get size of file %s.", file);
              if (timeout_flag == ON)
              {
                 (void)ftp_quit();
                 exit(DATE_ERROR);
              }
              rl[*no_of_listed_files].size = -1;
              check_size = NO;
           }
   }
   else
   {
      rl[*no_of_listed_files].size = -1;
   }

   (*no_of_listed_files)++;
   return(0);
}


/*++++++++++++++++++++++++++ remove_ls_data() +++++++++++++++++++++++++++*/
static void
remove_ls_data()
{
   if ((fra[db.fra_pos].stupid_mode == NO) &&
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
                       "Failed to unlink() <%s> : %s",
                       list_file, strerror(errno));
         }
      }
      else
      {
         if (errno != ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to stat() <%s> : %s",
                       list_file, strerror(errno));
         }
      }
   }
   return;
}
