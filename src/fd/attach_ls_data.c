/*
 *  attach_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_ls_data - attaches to ls data file
 **
 ** SYNOPSIS
 **   int attach_ls_data(void)
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
 **   17.06.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                /* atoi()                             */
#include <string.h>                /* strcpy(), strerror()               */
#include <time.h>                  /* mktime()                           */ 
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"


/* External global variables. */
extern int                        *no_of_listed_files,
                                  rl_fd;
extern char                       *p_work_dir;
extern struct job                 db;
extern struct retrieve_list       *rl;
extern struct fileretrieve_status *fra;


/*########################## attach_ls_data() ###########################*/
int
attach_ls_data(void)
{
   if (rl_fd == -1)
   {
      int         i;
      off_t       rl_size;
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
         return(INCORRECT);
      }
      if (fstat(rl_fd, &stat_buf) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() `%s' : %s",
                    list_file, strerror(errno));
         return(INCORRECT);
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
            return(INCORRECT);
         }
         if (write(rl_fd, "", 1) != 1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to write() to `%s' : %s",
                       list_file, strerror(errno));
            return(INCORRECT);
         }
      }
      else
      {
         rl_size = stat_buf.st_size;
      }
#ifdef HAVE_MMAP
      if ((ptr = mmap(NULL, rl_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL, rl_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, list_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() to `%s' : %s",
                    list_file, strerror(errno));
         return(INCORRECT);
      }
      no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct retrieve_list *)ptr;
      if (stat_buf.st_size == 0)
      {
         *no_of_listed_files = 0;
         *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
         *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
         *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
      }
      if (((db.special_flag & DISTRIBUTED_HELPER_JOB) == 0) &&
          ((db.special_flag & OLD_ERROR_JOB) == 0))
      {
         if (*no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d", *no_of_listed_files);
            *no_of_listed_files = 0;
         }
         else
         {
            off_t must_have_size;

            if (((rl_size - AFD_WORD_OFFSET) % sizeof(struct retrieve_list)) != 0)
            {
               off_t old_calc_size,
                     old_int_calc_size;

               old_calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_retrieve_list)) +
                               8;
               old_int_calc_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                    RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_int_retrieve_list)) +
                                   8;
               if (rl_size == old_calc_size)
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
                     return(INCORRECT);
                  }
                  rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                             RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                            AFD_WORD_OFFSET;
                  if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to lseek() in `%s' : %s",
                                new_list_file, strerror(errno));
                     return(INCORRECT);
                  }
                  if (write(new_rl_fd, "", 1) != 1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to write() to `%s' : %s",
                                new_list_file, strerror(errno));
                     return(INCORRECT);
                  }
#ifdef HAVE_MMAP
                  if ((new_ptr = mmap(NULL, rl_size,
                                      (PROT_READ | PROT_WRITE), MAP_SHARED,
                                      new_rl_fd, 0)) == (caddr_t) -1)
#else
                  if ((new_ptr = mmap_emu(NULL, rl_size,
                                          (PROT_READ | PROT_WRITE), MAP_SHARED,
                                          new_list_file, 0)) == (caddr_t) -1)
#endif
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to mmap() to `%s' : %s",
                                new_list_file, strerror(errno));
                     return(INCORRECT);
                  }
                  no_of_new_listed_files = (int *)new_ptr;
                  *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
                  *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                  *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
                  *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
                  *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
                  *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
                  *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
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
#ifdef HAVE_MMAP
                  if (munmap(ptr, stat_buf.st_size) == -1)
#else
                  if (munmap_emu(ptr) == -1)
#endif
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
               else if (rl_size == old_int_calc_size)
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
                          return(INCORRECT);
                       }
                       rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                  RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                                 AFD_WORD_OFFSET;
                       if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to lseek() in `%s' : %s",
                                     new_list_file, strerror(errno));
                          return(INCORRECT);
                       }
                       if (write(new_rl_fd, "", 1) != 1)
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to write() to `%s' : %s",
                                     new_list_file, strerror(errno));
                          return(INCORRECT);
                       }
#ifdef HAVE_MMAP
                       if ((new_ptr = mmap(NULL, rl_size,
                                           (PROT_READ | PROT_WRITE),
                                           MAP_SHARED, new_rl_fd,
                                           0)) == (caddr_t) -1)
#else
                       if ((new_ptr = mmap_emu(NULL, rl_size,
                                               (PROT_READ | PROT_WRITE),
                                               MAP_SHARED, new_list_file,
                                               0)) == (caddr_t) -1)
#endif
                       {
                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "Failed to mmap() to `%s' : %s",
                                     new_list_file, strerror(errno));
                          return(INCORRECT);
                       }
                       no_of_new_listed_files = (int *)new_ptr;
                       *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
                       *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                       *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
                       *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
                       *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
                       *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
                       *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
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
#ifdef HAVE_MMAP
                       if (munmap(ptr, stat_buf.st_size) == -1)
#else
                       if (munmap_emu(ptr) == -1)
#endif
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
                       if (*(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) != CURRENT_RL_VERSION)
                       {
                          if ((ptr = convert_ls_data(rl_fd,
                                                     list_file,
                                                     &rl_size,
                                                     *no_of_listed_files,
                                                     ptr,
                                                     *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1),
                                                     CURRENT_RL_VERSION)) == NULL)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to convert AFD ls data file %s.",
                                        list_file);
                             return(INCORRECT);
                          }
                          else
                          {
                             no_of_listed_files = (int *)ptr;
                             ptr += AFD_WORD_OFFSET;
                             rl = (struct retrieve_list *)ptr;
                          }
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
#ifdef HAVE_MMAP
                          if (munmap(ptr, rl_size) == -1)
#else
                          if (munmap_emu(ptr) == -1)
#endif
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
                             return(INCORRECT);
                          }
                          rl_size = (RETRIEVE_LIST_STEP_SIZE *
                                     sizeof(struct retrieve_list)) +
                                    AFD_WORD_OFFSET;
                          if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to lseek() in `%s' : %s",
                                        list_file, strerror(errno));
                             return(INCORRECT);
                          }
                          if (write(rl_fd, "", 1) != 1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to write() to `%s' : %s",
                                        list_file, strerror(errno));
                             return(INCORRECT);
                          }
#ifdef HAVE_MMAP
                          if ((ptr = mmap(NULL, rl_size,
                                          (PROT_READ | PROT_WRITE), MAP_SHARED,
                                          rl_fd, 0)) == (caddr_t) -1)
#else
                          if ((ptr = mmap_emu(NULL, rl_size,
                                              (PROT_READ | PROT_WRITE), MAP_SHARED,
                                              list_file, 0)) == (caddr_t) -1)
#endif
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to mmap() to `%s' : %s",
                                        list_file, strerror(errno));
                             return(INCORRECT);
                          }
                          no_of_listed_files = (int *)ptr;
                          *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
                          *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                          *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
                          *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
                          *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
                          *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
                          *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
                          ptr += AFD_WORD_OFFSET;
                          rl = (struct retrieve_list *)ptr;
                          *no_of_listed_files = 0;
                       }
                    }
            }
            else
            {
               if (*(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) != CURRENT_RL_VERSION)
               {
                  if ((ptr = convert_ls_data(rl_fd, list_file, &rl_size,
                                             *no_of_listed_files, ptr,
                                             *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1),
                                             CURRENT_RL_VERSION)) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Failed to convert AFD ls data file %s.",
                                list_file);
                     return(INCORRECT);
                  }
                  else
                  {
                     no_of_listed_files = (int *)ptr;
                     ptr += AFD_WORD_OFFSET;
                     rl = (struct retrieve_list *)ptr;
                  }
               }
            }

            /*
             * Check if the file does have the correct step size. If
             * not resize it to the correct size.
             */
            must_have_size = (((*no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                              RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                             AFD_WORD_OFFSET;
            if (rl_size < must_have_size)
            {
               ptr = (char *)rl - AFD_WORD_OFFSET;
               if ((ptr = mmap_resize(rl_fd, ptr, must_have_size)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "mmap_resize() error : %s", strerror(errno));
                  return(INCORRECT);
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

            for (i = 0; i < *no_of_listed_files; i++)
            {
               rl[i].in_list = NO;
            }
         }
      }
      else
      {
         if (stat_buf.st_size == 0)
         {
            return(INCORRECT);
         }
      }
   }

   return(SUCCESS);
}
