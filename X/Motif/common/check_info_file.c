/*
 *  check_info_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int check_info_file(void)
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
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strerror()                                    */
#include <unistd.h>     /* read(), close()                               */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>     /* free(), malloc()                              */
#include <fcntl.h>
#include <errno.h>
#include "x_common_defs.h"

#include <Xm/Xm.h>
#include <Xm/Text.h>

/* External global variables */
extern char   info_file[MAX_PATH_LENGTH],
              *info_data;
extern Widget info_w;

/* Local function prototypes */
static void fill_default_info(void);


/*########################### check_info_file() #########################*/
int
check_info_file(void)
{
   static int    first_time = YES;
   static time_t last_mtime = 0;
   int           file_changed = NO,
                 src_fd;
   struct stat   stat_buf;

   if (stat(info_file, &stat_buf) < 0)
   {
#ifdef _ERROR_MESSAGES
      (void)fprintf(stderr, "Failed to stat() %s : %s (%s %d)\n",
                    info_file, strerror(errno), __FILE__, __LINE__);
#endif
      if (first_time == YES)
      {
         fill_default_info();
         first_time = NO;
         file_changed = YES;
      }
      return(file_changed);
   }

   if (stat_buf.st_size == 0)
   {
#ifdef _ERROR_MESSAGES
      (void)fprintf(stderr, "Info file %s is empty! (%s %d)\n",
                    info_file, __FILE__, __LINE__);
#endif
      if (first_time == YES)
      {
         fill_default_info();
         first_time = NO;
         file_changed = YES;
      }
      return(file_changed);
   }

   if (stat_buf.st_mtime > last_mtime)
   {
      last_mtime = stat_buf.st_mtime;

      if ((src_fd = open(info_file, O_RDONLY)) < 0)
      {
#ifdef _ERROR_MESSAGES
         (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                       info_file, strerror(errno), __FILE__, __LINE__);
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

      /* Read file in one chunk */
      if (read(src_fd, info_data, stat_buf.st_size) != stat_buf.st_size)
      {
         (void)xrec(info_w, FATAL_DIALOG,
                    "read() error when reading from %s : %s (%s %d)",
                    info_file, strerror(errno), __FILE__, __LINE__);
         return(NO);
      }
      (void)close(src_fd);
      info_data[stat_buf.st_size] = '\0';

      /* Display the information */
      XmTextSetString(info_w, NULL);  /* Clears old entry */
      XmTextSetString(info_w, info_data);

      file_changed = YES;
   }

   /*
    * In case this information is removed, we do so as if this
    * is the first time.
    */
   first_time = YES;

   return(file_changed);
}


/*++++++++++++++++++++++++ fill_default_info() ++++++++++++++++++++++++++*/
static void
fill_default_info(void)
{
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
