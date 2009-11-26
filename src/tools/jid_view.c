/*
 *  jid_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   jid_view - shows all jobs that are held by the AFD
 **
 ** SYNOPSIS
 **   jid_view [-w <AFD work dir>] [--version] [<job ID> [...<job ID n>]]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1998 H.Kiehl Created
 **   15.05.1999 H.Kiehl Option to show only one job.
 **   24.06.2000 H.Kiehl Completely revised.
 **   30.12.2003 H.Kiehl File masks are now in a seperate file.
 **   05.01.2004 H.Kiehl Show DIR_CONFIG ID.
 **   12.09.2007 H.Kiehl Show directory name and make it possible to
 **                      supply multiple job ID's.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* atoi()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "version.h"

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  fd,
                        fml_offset,
                        i, j,
                        mask_offset,
                        no_of_dir_names,
                        no_of_file_masks_id,
                        no_of_job_ids,
                        no_of_search_ids;
   unsigned int         *job_id;
   off_t                dnb_size,
                        fmd_size,
                        jid_size;
   char                 file[MAX_PATH_LENGTH],
                        *fmd = NULL,
                        option_buffer[MAX_OPTION_LENGTH],
                        *ptr,
                        work_dir[MAX_PATH_LENGTH];
   struct stat          stat_buf;
   struct job_id_data  *jd;
   struct dir_name_buf *dnb;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    _("Usage: %s [-w <AFD work dir>] [--version] [<job ID> [...<job ID n>]]\n"),
                    argv[0]);
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   if (argc > 1)
   {
      no_of_search_ids = argc - 1;
      if ((job_id = malloc(no_of_search_ids * sizeof(unsigned int))) != NULL)
      {
         for (i = 0; i < no_of_search_ids; i++)
         {
            job_id[i] = (unsigned int)strtoul(argv[i + 1], (char **)NULL, 16);
         }
      }
      else
      {
         (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      no_of_search_ids = 0;
      job_id = NULL;
   }

   /* Map to JID file. */
   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
   if ((ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                       MAP_SHARED, file, 0)) == (caddr_t)-1)
#endif
   {
      (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
   {
      (void)fprintf(stderr, _("Incorrect JID version (data=%d current=%d)!\n"),
                    *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
      exit(INCORRECT);
   }
   no_of_job_ids = *(int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jd = (struct job_id_data *)ptr;
   jid_size = stat_buf.st_size;

   /* Map to file mask file. */
   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, FILE_MASK_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
         if ((ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                             MAP_SHARED, file, 0)) == (caddr_t)-1)
#endif
         {
            (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
            fmd = NULL;
         }
         else
         {
            no_of_file_masks_id = *(int *)ptr;
            ptr += AFD_WORD_OFFSET;
            fmd = ptr;
            fmd_size = stat_buf.st_size;
            fml_offset = sizeof(int) + sizeof(int);
            mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                          sizeof(unsigned char);
         }
      }

      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* Map to directory_names file. */
   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      if (fstat(fd, &stat_buf) == -1)
      {
         (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
         if ((ptr = mmap_emu(NULL, stat_buf.st_size, PROT_READ,
                             MAP_SHARED, file, 0)) == (caddr_t)-1)
#endif
         {
            (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
            dnb = NULL;
         }
         else
         {
            no_of_dir_names = *(int *)ptr;
            ptr += AFD_WORD_OFFSET;
            dnb = (struct dir_name_buf *)ptr;
            dnb_size = stat_buf.st_size;
         }
      }

      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
   }


   if (no_of_job_ids > 0)
   {
      for (i = 0; i < no_of_job_ids; i++)
      {
         for (j = 0; j < no_of_search_ids; j++)
         {
            if (job_id[j] == jd[i].job_id)
            {
               break;
            }
         }
         if ((job_id == NULL) || (job_id[j] == jd[i].job_id))
         {
            (void)fprintf(stdout, "Job-ID       : %x\n", jd[i].job_id);
            (void)fprintf(stdout, "Dir-ID       : %x\n", jd[i].dir_id);
            (void)fprintf(stdout, "DIR_CONFIG ID: %x\n", jd[i].dir_config_id);
            (void)fprintf(stdout, "File-Mask-ID : %x\n", jd[i].file_mask_id);
            (void)fprintf(stdout, "Recipient-ID : %x\n", jd[i].recipient_id);
            (void)fprintf(stdout, "Host-Alias-ID: %x\n", jd[i].host_id);
            (void)fprintf(stdout, "Dir position : %d\n", jd[i].dir_id_pos);
            (void)fprintf(stdout, "Priority     : %c\n", jd[i].priority);
            if (dnb != NULL)
            {
               for (j = 0; j < no_of_dir_names; j++)
               {
                  if (dnb[j].dir_id == jd[i].dir_id)
                  {
                     (void)fprintf(stdout, "Directory    : %s\n",
                                   dnb[j].orig_dir_name);
                     break;
                  }
               }
            }
            if (fmd != NULL)
            {
               int j;

               ptr = fmd;

               for (j = 0; j < no_of_file_masks_id; j++)
               {
                  if (*(unsigned int *)(ptr + fml_offset + sizeof(int)) == jd[i].file_mask_id)
                  {
                     if (*(int *)ptr == 1)
                     {
                        (void)fprintf(stdout, "File filters : %s\n",
                                      (ptr + mask_offset));
                     }
                     else
                     {
                        char *p_file = ptr + mask_offset;

                        (void)fprintf(stdout, "File filters : %s\n", p_file);
                        NEXT(p_file);
                        for (j = 1; j < *(int *)ptr; j++)
                        {
                           (void)fprintf(stdout, "             : %s\n", p_file);
                           NEXT(p_file);
                        }
                     }
                     break;
                  }
                  ptr += (mask_offset + *(int *)(ptr + fml_offset) +
                          sizeof(char) + *(ptr + mask_offset - 1));
               }
            }
            if (jd[i].no_of_loptions > 0)
            {
               if (jd[i].no_of_loptions == 1)
               {
                  (void)fprintf(stdout, "AMG options  : %s\n", jd[i].loptions);
               }
               else
               {
                  char *ptr = jd[i].loptions;

                  (void)fprintf(stdout, "AMG options  : %s\n", ptr);
                  NEXT(ptr);
                  for (j = 1; j < jd[i].no_of_loptions; j++)
                  {
                     (void)fprintf(stdout, "             : %s\n", ptr);
                     NEXT(ptr);
                  }
               }
            }
            if (jd[i].no_of_soptions > 0)
            {
               if (jd[i].no_of_soptions == 1)
               {
                  (void)fprintf(stdout, "FD options   : %s\n", jd[i].soptions);
               }
               else
               {
                  char *ptr,
                       *ptr_start;

                  ptr = ptr_start = option_buffer;

                  (void)strcpy(option_buffer, jd[i].soptions);
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  *ptr = '\0';
                  ptr++;
                  (void)fprintf(stdout, "FD options   : %s\n", ptr_start);
                  for (j = 1; j < jd[i].no_of_soptions; j++)
                  {
                     ptr_start = ptr;
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     *ptr = '\0';
                     ptr++;
                     (void)fprintf(stdout, "             : %s\n", ptr_start);
                  }
               }
            }
            (void)fprintf(stdout, "Recipient    : %s\n", jd[i].recipient);
            (void)fprintf(stdout, "Host alias   : %s\n", jd[i].host_alias);
            if ((i + 1) < no_of_job_ids)
            {
               (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
            }
         }
      }
   }
   else
   {
      (void)fprintf(stdout, _("Job ID list is empty.\n"));
   }

   if (fmd != NULL)
   {
      if (munmap((char *)fmd - AFD_WORD_OFFSET, fmd_size) == -1)
      {
         (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                       FILE_MASK_FILE, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (dnb != NULL)
   {
      if (munmap((char *)dnb - AFD_WORD_OFFSET, dnb_size) == -1)
      {
         (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                       DIR_NAME_FILE, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (munmap((char *)jd - AFD_WORD_OFFSET, jid_size) == -1)
   {
      (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(INCORRECT);
}
