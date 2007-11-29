/*
 *  get_remote_file_names_sftp.c - Part of AFD, an automatic file distribution
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
 **   get_remote_file_names_sftp - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int get_remote_file_names_sftp(off_t *file_size_to_retrieve)
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
 **   01.05.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <time.h>                  /* time(), mktime(), strftime()       */ 
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "sftpdefs.h"
#include "fddefs.h"


/* External global variables */
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
static time_t                     current_time;

/* Local function prototypes. */
static int                        check_list(char *, struct stat *, off_t *);


/*#################### get_remote_file_names_sftp() #####################*/
int
get_remote_file_names_sftp(off_t *file_size_to_retrieve)
{
   int              files_to_retrieve = 0,
                    gotcha,
                    i, j,
                    nfg,           /* Number of file mask. */
                    status;
   char             filename[MAX_FILENAME_LENGTH],
                    *p_mask;
   struct file_mask *fml = NULL;
   struct tm        *p_tm;
   struct stat      stat_buf;

   /* Get all file masks for this directory. */
   if (read_file_mask(fra[db.fra_pos].dir_alias, &nfg, &fml) == INCORRECT)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to get the file masks.");
      if (fml != NULL)
      {
         free(fml);
      }
      sftp_quit();
      exit(INCORRECT);
   }

   if (fra[db.fra_pos].ignore_file_time != 0)
   {
      /* Note: FTP returns GMT so we need to convert this to GMT! */
      current_time = time(NULL);
      p_tm = gmtime(&current_time);
      current_time = mktime(p_tm);
   }

   /*
    * Get a directory listing from the remote site so we can see
    * what files are there.
    */
   *file_size_to_retrieve = 0;
   if ((status = sftp_open_dir("", fsa->debug)) == SUCCESS)
   {
      if (fsa->debug > NORMAL_MODE)
      {
         trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                      "Opened remote directory.");
      }
      while ((status = sftp_readdir(filename, &stat_buf)) == SUCCESS)
      {
         if ((((filename[0] == '.') && (filename[1] == '\0')) ||
              ((filename[0] == '.') && (filename[1] == '.') &&
               (filename[2] == '\0'))) ||
             (((fra[db.fra_pos].dir_flag & ACCEPT_DOT_FILES) == 0) &&
              (filename[0] == '.')))
         {
            continue;
         }
         else
         {
            if (fsa->debug > NORMAL_MODE)
            {
               time_t mtime;
               char   dstr[26];

               mtime = stat_buf.st_mtime;
               p_tm = gmtime(&mtime);
               (void)strftime(dstr, 26, "%a %h %d %H:%M:%S %Y", p_tm);
               trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
#if SIZEOF_OFF_T == 4
                            "%s  `%s' size=%ld",
#else
                            "%s  `%s' size=%lld",
#endif
                            dstr, filename, (pri_off_t)stat_buf.st_size);
            }
            gotcha = NO;
            for (i = 0; i < nfg; i++)
            {
               p_mask = fml[i].file_list;
               for (j = 0; j < fml[i].fc; j++)
               {
                  if (((status = pmatch(p_mask, filename, NULL)) == 0) &&
                      (check_list(filename, &stat_buf, file_size_to_retrieve) == 0))
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
         }
      }
      if (status == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "Failed to read remote directory.");
         sftp_quit();
         exit(LIST_ERROR);
      }
      if (sftp_close_dir() == INCORRECT)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                   "Failed to close remote directory.");
      }
      else
      {
         if (fsa->debug > NORMAL_MODE)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, NULL,
                         "Closed remote directory.");
         }
      }
   }
   else
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, msg_str,
                "Failed to open remote directory for reading.");
      sftp_quit();
      exit(LIST_ERROR);
   }

   /* Free file mask list. */
   for (i = 0; i < nfg; i++)
   {
      free(fml[i].file_list);
   }
   free(fml);

   /*
    * Remove all files from the remote_list structure that are not
    * in the current buffer.
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
               sftp_quit();
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
check_list(char *file, struct stat *p_stat_buf, off_t *file_size_to_retrieve)
{
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
            sftp_quit();
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
                 sftp_quit();
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

      if (fra[db.fra_pos].ignore_file_time != 0)
      {
         rl[*no_of_listed_files].file_mtime = p_stat_buf->st_mtime;
         rl[*no_of_listed_files].got_date = YES;
      }
      else
      {
         rl[*no_of_listed_files].got_date = NO;
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
            sftp_quit();
            exit(INCORRECT);
         }
         if (fstat(rl_fd, &stat_buf) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to fstat() `%s' : %s",
                       list_file, strerror(errno));
            sftp_quit();
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
               sftp_quit();
               exit(INCORRECT);
            }
            if (write(rl_fd, "", 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to `%s' : %s",
                          list_file, strerror(errno));
               sftp_quit();
               exit(INCORRECT);
            }
         }
         else
         {
            rl_size = stat_buf.st_size;
         }
         if ((ptr = mmap(NULL, rl_size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to `%s' : %s",
                       list_file, strerror(errno));
            sftp_quit();
            exit(INCORRECT);
         }
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
                  off_t old_calc_size,
                        old_int_calc_size;

                  old_calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                   RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_retrieve_list)) +
                                  8;
                  old_int_calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                       RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_int_retrieve_list)) +
                                      8;
                  if (stat_buf.st_size == old_calc_size)
                  {
                     int                      new_rl_fd,
                                              *no_of_new_listed_files,
                                              no_of_old_listed_files;
                     char                     new_list_file[MAX_PATH_LENGTH],
                                              *new_ptr;
                     struct old_retrieve_list *orl;
                     struct retrieve_list     *nrl;
                     struct tm                bd_time;

                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Converting old retrieve list %s", list_file);
                     no_of_old_listed_files = *no_of_listed_files;
                     ptr -= AFD_WORD_OFFSET;
                     ptr += 8;
                     orl = (struct old_retrieve_list *)ptr;

                     (void)sprintf(new_list_file, "%s%s%s%s/.%s",
                                   p_work_dir, AFD_FILE_DIR,
                                   INCOMING_DIR, LS_DATA_DIR,
                                   fra[db.fra_pos].dir_alias);
                     if ((new_rl_fd = open(new_list_file,
                                           O_RDWR | O_CREAT | O_TRUNC,
                                           FILE_MODE)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to open() `%s' : %s",
                                   new_list_file, strerror(errno));
                        sftp_quit();
                        exit(INCORRECT);
                     }
                     rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                               AFD_WORD_OFFSET;
                     if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to lseek() in `%s' : %s",
                                   new_list_file, strerror(errno));
                        sftp_quit();
                        exit(INCORRECT);
                     }
                     if (write(new_rl_fd, "", 1) != 1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to write() to `%s' : %s",
                                   new_list_file, strerror(errno));
                        sftp_quit();
                        exit(INCORRECT);
                     }
                     if ((new_ptr = mmap(NULL, rl_size,
                                         (PROT_READ | PROT_WRITE), MAP_SHARED,
                                         new_rl_fd, 0)) == (caddr_t) -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to mmap() to `%s' : %s",
                                   new_list_file, strerror(errno));
                        sftp_quit();
                        exit(INCORRECT);
                     }
                     no_of_new_listed_files = (int *)new_ptr;
                     *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                     new_ptr += AFD_WORD_OFFSET;
                     nrl = (struct retrieve_list *)new_ptr;
                     *no_of_new_listed_files = no_of_old_listed_files;

                     for (i = 0; i < no_of_old_listed_files; i++)
                     {
                        (void)strcpy(nrl[i].file_name, orl[i].file_name);
                        nrl[i].retrieved = orl[i].retrieved;
                        nrl[i].in_list = orl[i].in_list;
                        nrl[i].size = orl[i].size;
                        if (orl[i].date[0] == '\0')
                        {
                           nrl[i].file_mtime = -1;
                           nrl[i].got_date = NO;
                        }
                        else
                        {
                           bd_time.tm_sec  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2] = '\0';
                           bd_time.tm_min  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4] = '\0';
                           bd_time.tm_hour = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6] = '\0';
                           bd_time.tm_mday = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8] = '\0';
                           bd_time.tm_mon = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10]) - 1;
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10] = '\0';
                           bd_time.tm_year = atoi(orl[i].date) - 1900;
                           bd_time.tm_isdst = 0;
                           nrl[i].file_mtime = mktime(&bd_time);
                           nrl[i].got_date = YES;
                        }
                     }
                     ptr -= 8;
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
                     if (unlink(list_file) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to unlink() %s : %s",
                                   list_file, strerror(errno));
                     }
                     if (rename(new_list_file, list_file) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to rename() %s to %s : %s",
                                   new_list_file, list_file, strerror(errno));
                     }
                     rl_fd = new_rl_fd;
                     rl = nrl;
                     ptr = new_ptr;
                     no_of_listed_files = no_of_new_listed_files;
                  }
                  else if (stat_buf.st_size == old_int_calc_size)
                       {
                          int                          new_rl_fd,
                                                       *no_of_new_listed_files,
                                                       no_of_old_listed_files;
                          char                         new_list_file[MAX_PATH_LENGTH],
                                                       *new_ptr;
                          struct old_int_retrieve_list *orl;
                          struct retrieve_list         *nrl;
                          struct tm                    bd_time;

                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Converting old retrieve list %s", list_file);
                          no_of_old_listed_files = *no_of_listed_files;
                          ptr -= AFD_WORD_OFFSET;
                          ptr += 8;
                          orl = (struct old_int_retrieve_list *)ptr;

                          (void)sprintf(new_list_file, "%s%s%s%s/.%s",
                                        p_work_dir, AFD_FILE_DIR,
                                        INCOMING_DIR, LS_DATA_DIR,
                                        fra[db.fra_pos].dir_alias);
                          if ((new_rl_fd = open(new_list_file,
                                                O_RDWR | O_CREAT | O_TRUNC,
                                                FILE_MODE)) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to open() `%s' : %s",
                                        new_list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                     RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                                    AFD_WORD_OFFSET;
                          if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to lseek() in `%s' : %s",
                                        new_list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          if (write(new_rl_fd, "", 1) != 1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to write() to `%s' : %s",
                                        new_list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          if ((new_ptr = mmap(NULL, rl_size,
                                              (PROT_READ | PROT_WRITE),
                                              MAP_SHARED, new_rl_fd,
                                              0)) == (caddr_t) -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to mmap() to `%s' : %s",
                                        new_list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          no_of_new_listed_files = (int *)new_ptr;
                          *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                          new_ptr += AFD_WORD_OFFSET;
                          nrl = (struct retrieve_list *)new_ptr;
                          *no_of_new_listed_files = no_of_old_listed_files;

                          for (i = 0; i < no_of_old_listed_files; i++)
                          {
                             (void)strcpy(nrl[i].file_name, orl[i].file_name);
                             nrl[i].retrieved = orl[i].retrieved;
                             nrl[i].in_list = orl[i].in_list;
                             nrl[i].size = orl[i].size;
                             if (orl[i].date[0] == '\0')
                             {
                                nrl[i].file_mtime = -1;
                                nrl[i].got_date = NO;
                             }
                             else
                             {
                                bd_time.tm_sec  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2] = '\0';
                                bd_time.tm_min  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4] = '\0';
                                bd_time.tm_hour = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6] = '\0';
                                bd_time.tm_mday = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8] = '\0';
                                bd_time.tm_mon = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10]) - 1;
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10] = '\0';
                                bd_time.tm_year = atoi(orl[i].date) - 1900;
                                bd_time.tm_isdst = 0;
                                nrl[i].file_mtime = mktime(&bd_time);
                                nrl[i].got_date = YES;
                             }
                          }
                          ptr -= 8;
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
                          if (unlink(list_file) == -1)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Failed to unlink() %s : %s",
                                        list_file, strerror(errno));
                          }
                          if (rename(new_list_file, list_file) == -1)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to rename() %s to %s : %s",
                                        new_list_file, list_file,
                                        strerror(errno));
                          }
                          rl_fd = new_rl_fd;
                          rl = nrl;
                          ptr = new_ptr;
                          no_of_listed_files = no_of_new_listed_files;
                       }
                       else
                       {
                          off_t calc_size;

                          calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                       RETRIEVE_LIST_STEP_SIZE *
                                       sizeof(struct retrieve_list)) +
                                      AFD_WORD_OFFSET;
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                     "Hmm, LS data file %s has incorrect size (%ld != %ld, %ld, %ld), removing it.",
#else
                                     "Hmm, LS data file %s has incorrect size (%lld != %lld, %lld, %lld), removing it.",
#endif
                                     list_file, (pri_off_t)stat_buf.st_size,
                                     (pri_off_t)calc_size,
                                     (pri_off_t)old_calc_size,
                                     (pri_off_t)old_int_calc_size);
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
                          if ((rl_fd = open(list_file,
                                            O_RDWR | O_CREAT | O_TRUNC,
                                            FILE_MODE)) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to open() `%s' : %s",
                                        list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          rl_size = (RETRIEVE_LIST_STEP_SIZE *
                                     sizeof(struct retrieve_list)) +
                                    AFD_WORD_OFFSET;
                          if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to lseek() in `%s' : %s",
                                        list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          if (write(rl_fd, "", 1) != 1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to write() to `%s' : %s",
                                        list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          if ((ptr = mmap(NULL, rl_size,
                                          (PROT_READ | PROT_WRITE), MAP_SHARED,
                                          rl_fd, 0)) == (caddr_t) -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to mmap() to `%s' : %s",
                                        list_file, strerror(errno));
                             sftp_quit();
                             exit(INCORRECT);
                          }
                          no_of_listed_files = (int *)ptr;
                          *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                          ptr += AFD_WORD_OFFSET;
                          rl = (struct retrieve_list *)ptr;
                          *no_of_listed_files = 0;
                       }
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
            rl[i].in_list = YES;
            if ((fra[db.fra_pos].stupid_mode == GET_ONCE_ONLY) &&
                (rl[i].retrieved == YES))
            {
               return(1);
            }
            if (rl[i].file_mtime != p_stat_buf->st_mtime)
            {
               rl[i].file_mtime = p_stat_buf->st_mtime;
               rl[i].retrieved = NO;
            }
            rl[i].got_date = YES;
            if (rl[i].size != p_stat_buf->st_size)
            {
               rl[i].size = p_stat_buf->st_size;
               rl[i].retrieved = NO;
            }
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
         if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            sftp_quit();
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
      rl[*no_of_listed_files].file_mtime = p_stat_buf->st_mtime;
      rl[*no_of_listed_files].got_date = YES;
   }

   (void)strcpy(rl[*no_of_listed_files].file_name, file);
   rl[*no_of_listed_files].retrieved = NO;
   rl[*no_of_listed_files].in_list = YES;
   rl[*no_of_listed_files].size = p_stat_buf->st_size;
   *file_size_to_retrieve += p_stat_buf->st_size;

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
