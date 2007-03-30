/*
 *  convert_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_ls_data - Convert 32 bit contents of the AFD ls data file to
 **                     64 bit ls data
 **
 ** SYNOPSIS
 **   convert_ls_data [--version] <ls data filename 1>[...<ls data filename n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.2005 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void usage(char *);

struct retrieve_list_int
       {
          char   file_name[MAX_FILENAME_LENGTH];
          char   got_date;
          char   retrieved;              /* Has the file already been      */
                                         /* retrieved?                     */
          char   in_list;                /* Used to remove any files from  */
                                         /* the list that are no longer at */
                                         /* the remote host.               */
          int    size;                   /* Size of the file.              */
          char   fill_bytes[4];
          int    file_mtime;             /* Modification time of file.     */
       };


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                      fd,
                            i,
                            j,
                            no_of_listed_files;
   char                     *ptr,
                            time_str[25],
                            tmp_name[13],
                            work_dir[MAX_PATH_LENGTH];
   struct stat              stat_buf;
   struct retrieve_list_int *rli;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   (void)strcpy(tmp_name, ".ls_data.tmp");
   for (i = 1; i < argc; i++)
   {
      if ((fd = open(argv[i], O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s\n",
                       argv[i], strerror(errno));
      }
      else
      {
         if (fstat(fd, &stat_buf) == -1)
         {
            (void)fprintf(stderr, "Failed to fstat() %s : %s\n",
                          argv[i], strerror(errno));
         }
         else
         {
            if ((ptr = mmap(0, stat_buf.st_size, PROT_READ, MAP_SHARED,
                            fd, 0)) == (caddr_t) -1)
            {
               (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                             argv[i], strerror(errno));
            }
            else
            {
               int                  new_fd;
               size_t               new_size;
               char                 *new_ptr;
               struct retrieve_list *rl;

               no_of_listed_files = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               rli = (struct retrieve_list_int *)ptr;
               new_size = (no_of_listed_files * sizeof(struct retrieve_list)) +
                          AFD_WORD_OFFSET;
               new_ptr = attach_buf(tmp_name, &new_fd, new_size, NULL,
                                    (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
                                    NO);
               if (new_ptr == (caddr_t) -1)
               {
                  (void)fprintf(stderr, "Failed to attach_buf() : %s\n",
                                strerror(errno));
                  exit(INCORRECT);
               }

               *(int *)new_ptr = no_of_listed_files;
               new_ptr += AFD_WORD_OFFSET;
               rl = (struct retrieve_list *)new_ptr;
               for (j = 0; j < no_of_listed_files; j++)
               {
                  (void)strcpy(rl[j].file_name, rli[j].file_name);
                  rl[j].got_date = rli[j].got_date;
                  rl[j].retrieved = rli[j].retrieved;
                  rl[j].in_list = rli[j].in_list;
                  rl[j].size = (off_t)rli[j].size;
                  rl[j].file_mtime = (time_t)rli[j].file_mtime;
               }
               ptr -= AFD_WORD_OFFSET;
               if (munmap(ptr, stat_buf.st_size) == -1)
               {
                  (void)fprintf(stderr, "Failed to munmap() from %s : %s\n",
                                argv[i], strerror(errno));
               }
               unmap_data(new_fd, (void *)&rl);
               if (unlink(argv[i]) == -1)
               {
                  (void)fprintf(stderr, "Failed to unlink() %s : %s\n",
                                argv[i], strerror(errno));
                  exit(INCORRECT);
               }
               if (rename(tmp_name, argv[i]) == -1)
               {
                  (void)fprintf(stderr, "Failed to rename() %s to %s : %s\n",
                                tmp_name, argv[i], strerror(errno));
                  exit(INCORRECT);
               }
            }
         }
         (void)close(fd);
      }
   }
   exit(SUCCESS);
}


/*############################## usage() ################################*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "%s <ls data filename 1>[... <ls data file name n>]\n",
                 progname);
   return;
}
