/*
 *  check_info_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_info_file - checks if the info file changes and rereads it
 **                     if it does
 **
 ** SYNOPSIS
 **   int check_info_file(char *alias_name)
 **
 ** DESCRIPTION
 **   Reads the contents of the file info_file into the buffer
 **   host_info_data when the modification time has changed. If there
 **   is no change in time, the file is left untouched.
 **
 ** RETURN VALUES
 **   Returns YES when no error has occurred and the info file has
 **   changed. Otherwise it will return NO.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.11.1996 H.Kiehl Created
 **   09.02.2005 H.Kiehl Added support for central host info file.
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strerror()                                    */
#include <unistd.h>     /* read(), close()                               */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>  /* mmap(), munmap()                              */
#endif
#include <sys/stat.h>
#include <stdlib.h>     /* free(), malloc()                              */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "x_common_defs.h"

#include <Xm/Xm.h>
#include <Xm/Text.h>

/* External global variables */
extern char   *alias_info_file,
              *central_info_file,
              *info_data;
extern Widget info_w;

/* Local function prototypes */
static void fill_default_info(void);


/*########################### check_info_file() #########################*/
int
check_info_file(char *alias_name)
{
   static int    first_time = YES;
   static time_t last_mtime_central = 0,
                 last_mtime_host = 0;
   int           file_changed = NO,
                 src_fd;
   struct stat   stat_buf;

   if ((stat(central_info_file, &stat_buf) == 0) && (stat_buf.st_size > 0))
   {
      if (stat_buf.st_mtime > last_mtime_central)
      {
         last_mtime_central = stat_buf.st_mtime;

         if ((src_fd = open(central_info_file, O_RDONLY)) < 0)
         {
#ifdef _ERROR_MESSAGES
            (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                          central_info_file, strerror(errno),
                          __FILE__, __LINE__);
#endif
         }
         else
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                            MAP_SHARED, src_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(0, stat_buf.st_size, PROT_READ,
                                MAP_SHARED, central_info_file, 0)) == (caddr_t) -1)
#endif
            {
#ifdef _ERROR_MESSAGES
               (void)fprintf(stderr, "Failed to mmap() %s : %s (%s %d)\n",
                             central_info_file, strerror(errno),
                             __FILE__, __LINE__);
#endif
            }
            else
            {
               size_t length;
               char   *p_end,
                      *p_start,
                      search_str_end[MAX_HOSTNAME_LENGTH + MAX_HOSTNAME_LENGTH + 5],
                      search_str_start[MAX_HOSTNAME_LENGTH + MAX_HOSTNAME_LENGTH + 3];

               search_str_end[0] = '\n';
               search_str_end[1] = '<';
               search_str_end[2] = '/';
               length = strlen(alias_name);
               if (length >= (MAX_HOSTNAME_LENGTH + MAX_HOSTNAME_LENGTH + 4))
               {
                  (void)fprintf(stderr, "Hmmm, this should not be! (%s %d)\n",
                                __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               (void)strcpy(&search_str_end[3], alias_name);
               search_str_end[3 + length] = '>';
               search_str_end[3 + length + 1] = '\0';
               search_str_start[0] = '<';
               (void)strcpy(&search_str_start[1], &search_str_end[3]);
               if (((p_start = posi(ptr, search_str_start)) != NULL) &&
                   ((p_end = posi(p_start, search_str_end)) != NULL))
               {
                  length = p_end - p_start - (length + 4);

                  if (info_data != NULL)
                  {
                     /* Free previous memory chunk */
                     free(info_data);
                  }
                  if ((info_data = malloc(length + 1)) == NULL)
                  {
                     (void)xrec(info_w, FATAL_DIALOG,
                                "Failed to allocate memory : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return(NO);
                  }
                  (void)memcpy(info_data, p_start, length);
                  (void)close(src_fd);
                  info_data[length] = '\0';
#ifdef HAVE_MMAP
                  if (munmap(ptr, stat_buf.st_size) == -1)
#else
                  if (munmap_emu(ptr) == -1)
#endif
                  {
                     (void)fprintf(stderr, "Failed to munmap() from %s : %s\n",
                                   central_info_file, strerror(errno));
                  }

                  /* Display the information. */
                  XmTextSetString(info_w, NULL);  /* Clears old entry */
                  XmTextSetString(info_w, info_data);

                  first_time = YES;
                  return(YES);
               }
            }
         }
      }
      else
      {
         first_time = YES;
         return(NO);
      }
   }

   /*
    * No central Info file or alias not found in it. So lets
    * search for alias info file.
    */
   if ((stat(alias_info_file, &stat_buf) == 0) && (stat_buf.st_size > 0))
   {
      if (stat_buf.st_mtime > last_mtime_host)
      {
         last_mtime_host = stat_buf.st_mtime;

         if ((src_fd = open(alias_info_file, O_RDONLY)) < 0)
         {
#ifdef _ERROR_MESSAGES
            (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                          alias_info_file, strerror(errno), __FILE__, __LINE__);
#endif
            if (first_time == YES)
            {
               fill_default_info();
               first_time = NO;
               file_changed = YES;
            }
            return(file_changed);
         }
         if (info_data != NULL)
         {
            /* Free previous memory chunk */
            free(info_data);
         }
         if ((info_data = malloc(stat_buf.st_size + 1)) == NULL)
         {
            (void)xrec(info_w, FATAL_DIALOG,
                       "Failed to allocate memory : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
            return(NO);
         }

         /* Read file in one chunk. */
         if (read(src_fd, info_data, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)xrec(info_w, FATAL_DIALOG,
                       "read() error when reading from %s : %s (%s %d)",
                       alias_info_file, strerror(errno), __FILE__, __LINE__);
            return(NO);
         }
         (void)close(src_fd);
         info_data[stat_buf.st_size] = '\0';

         /* Display the information. */
         XmTextSetString(info_w, NULL);  /* Clears old entry */
         XmTextSetString(info_w, info_data);

         first_time = YES;
         file_changed = YES;
      }
      else
      {
         first_time = YES;
         file_changed = NO;
      }
   }
   else
   {
#ifdef _ERROR_MESSAGES
      (void)fprintf(stderr, "Failed to stat() %s : %s (%s %d)\n",
                    alias_info_file, strerror(errno), __FILE__, __LINE__);
#endif
      if (first_time == YES)
      {
         fill_default_info();
         first_time = NO;
         file_changed = YES;
      }
   }

   return(file_changed);
}


/*++++++++++++++++++++++++ fill_default_info() ++++++++++++++++++++++++++*/
static void
fill_default_info(void)
{
   if (info_data != NULL)
   {
      /* Free previous memory chunk */
      free(info_data);
   }
   if ((info_data = malloc(384)) == NULL)
   {
      (void)xrec(info_w, FATAL_DIALOG,
                 "Failed to allocate memory : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   (void)sprintf(info_data, "\n\n\n\n\n                   %s\n",
                 NO_INFO_AVAILABLE);

   /* Display the information */
   XmTextSetString(info_w, NULL);  /* Clears old entry */
   XmTextSetString(info_w, info_data);

   return;
}
