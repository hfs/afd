/*
 *  convert_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   convert_fra - converts the FRA from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_fra(int           old_fra_fd,
 **                     char          *old_fra_stat,
 **                     off_t         *old_fra_size,
 **                     int           old_no_of_dirs,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure fileretrieve_status (FRA)
 **   This function converts an old FRA to the new one. This one
 **   is currently for converting Version 0 to 1.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.05.2002 H.Kiehl Created
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   14.08.2002 H.Kiehl Option to ignore files which have a certain size.
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf(), sprintf()            */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>                 /* mmap()                          */
#endif
#include <unistd.h>
#include <fcntl.h>                    /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#include <errno.h>
#include "amgdefs.h"

/* External global veriables. */
extern int sys_log_fd;

/* Version 0 */
#define MAX_DIR_ALIAS_LENGTH_0 10
#define MAX_HOSTNAME_LENGTH_0  8
#define MAX_RECIPIENT_LENGTH_0 256
struct fileretrieve_status_0
       {
          char                 dir_alias[MAX_DIR_ALIAS_LENGTH_0 + 1];
          char                 host_alias[MAX_HOSTNAME_LENGTH_0 + 1];
          char                 url[MAX_RECIPIENT_LENGTH_0];
          struct bd_time_entry te;
          unsigned char        dir_status;
          unsigned char        remove;
          unsigned char        stupid_mode;
          unsigned int         protocol;
          unsigned char        delete_files_flag;
          unsigned char        report_unknown_files;
          unsigned char        important_dir;
          unsigned char        time_option;
          char                 force_reread;
          char                 queued;
          char                 priority;
          unsigned long        bytes_received;
          unsigned int         files_received;
          time_t               last_retrieval;
          time_t               next_check_time;
          int                  old_file_time;
          int                  end_character;
          int                  dir_pos;
          int                  fsa_pos;
          int                  no_of_process;
          int                  max_process;
       };

/* Version 1 */
#define MAX_DIR_ALIAS_LENGTH_1 10
#define MAX_HOSTNAME_LENGTH_1  8
#define MAX_RECIPIENT_LENGTH_1 256
struct fileretrieve_status_1
       {
          char                 dir_alias[MAX_DIR_ALIAS_LENGTH_1 + 1];
          char                 host_alias[MAX_HOSTNAME_LENGTH_1 + 1];
          char                 url[MAX_RECIPIENT_LENGTH_1];
          struct bd_time_entry te;
          unsigned char        dir_status;
          unsigned char        remove;
          unsigned char        stupid_mode;
          unsigned int         protocol;
          unsigned char        delete_files_flag;
          unsigned char        report_unknown_files;
          unsigned char        important_dir;
          unsigned char        time_option;
          char                 force_reread;
          char                 queued;
          char                 priority;
          off_t                bytes_received;
          unsigned int         files_received;
          unsigned int         dir_flag;
          unsigned int         files_in_dir;
          unsigned int         files_queued;
          off_t                bytes_in_dir;
          off_t                bytes_in_queue;
          time_t               last_retrieval;
          time_t               next_check_time;
          int                  unknown_file_time;
          int                  queued_file_time;
          int                  end_character;
          int                  dir_pos;
          int                  fsa_pos;
          int                  no_of_process;
          int                  max_process;
       };


/*############################ convert_fra() ############################*/
char *
convert_fra(int           old_fra_fd,
            char          *old_fra_stat,
            off_t         *old_fra_size,
            int           old_no_of_dirs,
            unsigned char old_version,
            unsigned char new_version)
{
   int         i;
   size_t      new_size;
   char        *ptr;
   struct stat stat_buf;

   if ((old_version == 0) && (new_version == 1))
   {
      struct fileretrieve_status_0 *old_fra;
      struct fileretrieve_status_1 *new_fra;

      /* Get the size of the old FRA file. */
      if (fstat(old_fra_fd, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to fstat() %s : %s (%s %d)\n",
                   old_fra_stat, strerror(errno), __FILE__, __LINE__);
         *old_fra_size = -1;
         return(NULL);
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
#ifdef _NO_MMAP
            if ((ptr = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                                MAP_SHARED, old_fra_stat, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                            MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#endif
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to mmap() to %s : %s (%s %d)\n",
                         old_fra_stat, strerror(errno), __FILE__, __LINE__);
               *old_fra_size = -1;
               return(NULL);
            }
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "FRA file %s is empty. (%s %d)\n",
                      old_fra_stat, __FILE__, __LINE__);
            *old_fra_size = -1;
            return(NULL);
         }
      }

      ptr += AFD_WORD_OFFSET;
      old_fra = (struct fileretrieve_status_0 *)ptr;

      new_size = old_no_of_dirs * sizeof(struct fileretrieve_status_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error [%d %d] : %s (%s %d)\n",
                   old_no_of_dirs, new_size, strerror(errno), __FILE__, __LINE__);
         ptr = (char *)old_fra;
         ptr -= AFD_WORD_OFFSET;
#ifdef _NO_MMAP
         if (munmap_emu(ptr) == -1)
#else
         if (munmap(ptr, stat_buf.st_size) == -1)
#endif
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      old_fra_stat, strerror(errno), __FILE__, __LINE__);
         }
         *old_fra_size = -1;
         return(NULL);
      }
      new_fra = (struct fileretrieve_status_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_dirs; i++)
      {
         (void)strcpy(new_fra[i].dir_alias, old_fra[i].dir_alias);
         (void)strcpy(new_fra[i].host_alias, old_fra[i].host_alias);
         (void)strcpy(new_fra[i].url, old_fra[i].url);
         (void)memcpy(&new_fra[i].te, &old_fra[i].te,
                      sizeof(struct bd_time_entry));
         new_fra[i].dir_status           = old_fra[i].dir_status;
         new_fra[i].remove               = old_fra[i].remove;
         new_fra[i].stupid_mode          = old_fra[i].stupid_mode;
         new_fra[i].protocol             = old_fra[i].protocol;
         new_fra[i].delete_files_flag    = old_fra[i].delete_files_flag;
         new_fra[i].report_unknown_files = old_fra[i].report_unknown_files;
         new_fra[i].important_dir        = old_fra[i].important_dir;
         new_fra[i].time_option          = old_fra[i].time_option;
         new_fra[i].force_reread         = old_fra[i].force_reread;
         new_fra[i].queued               = old_fra[i].queued;
         new_fra[i].priority             = old_fra[i].priority;
         new_fra[i].bytes_received       = old_fra[i].bytes_received;
         new_fra[i].files_received       = old_fra[i].files_received;
         new_fra[i].last_retrieval       = old_fra[i].last_retrieval;
         new_fra[i].next_check_time      = old_fra[i].next_check_time;
         new_fra[i].unknown_file_time    = old_fra[i].old_file_time;
         new_fra[i].queued_file_time     = old_fra[i].old_file_time;
         new_fra[i].end_character        = old_fra[i].end_character;
         new_fra[i].dir_pos              = old_fra[i].dir_pos;
         new_fra[i].fsa_pos              = old_fra[i].fsa_pos;
         new_fra[i].no_of_process        = old_fra[i].no_of_process;
         new_fra[i].max_process          = old_fra[i].max_process;
         new_fra[i].dir_flag             = 0;
         new_fra[i].files_in_dir         = 0;
         new_fra[i].files_queued         = 0;
         new_fra[i].bytes_in_dir         = 0;
         new_fra[i].bytes_in_queue       = 0;
      }

      ptr = (char *)old_fra;
      ptr -= AFD_WORD_OFFSET;

      /*
       * Resize the old FRA to the size of new one and then copy
       * the new structure into it. Then update the FRA version
       * number.
       */
      if ((ptr = mmap_resize(old_fra_fd, ptr, new_size + AFD_WORD_OFFSET)) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to mmap_resize() %s : %s (%s %d)\n",
                   old_fra_stat, strerror(errno), __FILE__, __LINE__);
         free((void *)new_fra);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET;
      (void)memcpy(ptr, new_fra, new_size);
      free((void *)new_fra);
      ptr -= AFD_WORD_OFFSET;
      *(ptr + sizeof(int) + 1 + 1 + 1) = new_version;
      *old_fra_size = new_size + AFD_WORD_OFFSET;

      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Converted FRA from verion %d to %d.\n",
                (int)old_version, (int)new_version);
   }
   else
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Don't know how to convert a version %d FRA to version %d.\n",
                old_version, new_version);
   }

   return(ptr);
}
