/*
 *  get_dc_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dc_data - gets all data out of the DIR_CONFIG for a host
 **
 ** SYNOPSIS
 **   get_dc_data <host alias>
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
 **   27.02.1999 H.Kiehl Created
 **   26.04.1999 H.Kiehl Added real hostname to output.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* free()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>         /* mmap(), munmap()                        */
#include <errno.h>
#include "version.h"
#include "permission.h"

/* Global variables */
int                        *current_jid_list,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_current_jobs,
                           no_of_hosts,
                           sys_log_fd,
                           view_passwd = NO;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;

/* Local function prototypes */
static void                get_dc_data(char *),
                           show_data(struct job_id_data *, char *, int);
static int                 get_current_jid_list(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char *perm_buffer,
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);
   p_work_dir = work_dir;

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <host alias>\n", argv[0]);
      exit(INCORRECT);
   }

   /* Check if user may view the password. */
   switch(get_permissions(&perm_buffer))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS :
         /* Lets evaluate the permissions and see what */
         /* the user may do.                           */
         if ((perm_buffer[0] == 'a') &&
             (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') ||
             (perm_buffer[3] == ' ') ||
             (perm_buffer[3] == '\t')))
         {
            view_passwd = YES;
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, VIEW_PASSWD_PERM) != NULL)
              {
                 view_passwd = YES;
              }
         free(perm_buffer);
         break;

      case INCORRECT:
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (fsa_attach() == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to attach to FSA!\n");
      exit(INCORRECT);
   }
   get_dc_data(argv[1]);
   (void)fsa_detach();

   exit(SUCCESS);
}


/*############################ get_dc_data() ############################*/
static void
get_dc_data(char *host_name)
{
   int                 dnb_fd,
                       i,
                       j,
                       jd_fd,
                       *no_of_job_ids,
                       position;
   size_t              jid_size = 0,
                       dnb_size = 0;
   char                job_id_data_file[MAX_PATH_LENGTH];
   struct stat         stat_buf;
   struct job_id_data  *jd = NULL;
   struct dir_name_buf *dnb = NULL;

   /* First check if the host is in the FSA */
   if ((position = get_host_position(fsa, host_name, no_of_hosts)) == INCORRECT)
   {
      (void)fprintf(stderr, "Host alias %s is not in FSA. (%s %d)\n",
                    host_name, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   current_jid_list = NULL;
   no_of_current_jobs = 0;

   if (get_current_jid_list() == INCORRECT)
   {
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
   }

   /* Map to JID database. */
   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((jd_fd = open(job_id_data_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr,
                 "Failed to open() %s : %s (%s %d)",
                 job_id_data_file, strerror(errno), __FILE__, __LINE__);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
      (void)close(jd_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      jid_size = stat_buf.st_size;
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      (void)fprintf(stderr, "Job ID database file is empty. (%s %d)\n",
                    __FILE__, __LINE__);
      (void)close(jd_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      return;
   }
   if (close(jd_fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /* Map to directory name buffer. */
   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (fstat(dnb_fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
      (void)close(dnb_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (stat_buf.st_size > 0)
   {
      char *ptr;

      dnb_size = stat_buf.st_size;
      if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                      MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         if (jid_size > 0)
         {
            if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
            {
               (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
         }
         return;
      }
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   else
   {
      (void)fprintf(stderr, "Job ID database file is empty. (%s %d)\n",
                    __FILE__, __LINE__);
      (void)close(dnb_fd);
      if (current_jid_list != NULL)
      {
         free(current_jid_list);
      }
      if (jid_size > 0)
      {
         if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
         {
            (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      return;
   }
   if (close(dnb_fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /*
    * Go through current job list and search the JID structure for
    * the host we are looking for.
    */
   for (i = 0; i < no_of_current_jobs; i++)
   {
      for (j = 0; j < *no_of_job_ids; j++)
      {
         if (jd[j].job_id == current_jid_list[i])
         {
            if (strcmp(jd[j].host_alias, host_name) == 0)
            {
               show_data(&jd[j], dnb[jd[j].dir_id_pos].dir_name, position);
            }
            break;
         }
      }
   }

   /* Free all memory that was allocated or mapped. */
   if (current_jid_list != NULL)
   {
      free(current_jid_list);
   }
   if (dnb_size > 0)
   {
      if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) < 0)
      {
         (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   if (jid_size > 0)
   {
      if (munmap(((char *)jd - AFD_WORD_OFFSET), jid_size) < 0)
      {
         (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ show_data() ++++++++++++++++++++++++++++++*/
static void
show_data(struct job_id_data *p_jd, char *dir_name, int position)
{
   int  i;
   char *p_file = p_jd->file_list,
        value[MAX_PATH_LENGTH];

   (void)fprintf(stdout, "Directory     : %s\n", dir_name);

   /* Show file filters. */
   (void)fprintf(stdout, "Filter        : %s\n", p_file);
   NEXT(p_file);
   for (i = 1; i < p_jd->no_of_files; i++)
   {
      (void)fprintf(stdout, "                %s\n", p_file);
      NEXT(p_file);
   }

   /* Print recipient */
   (void)strcpy(value, p_jd->recipient);
   if (view_passwd != YES)
   {
      char *ptr = value;

      /*
       * The user may not see the password. Lets cut it out and
       * replace it with DEFAULT_VIEW_PASSWD.
       */
      while (*ptr != ':')
      {
         ptr++;
      }
      ptr++;
      while ((*ptr != ':') && (*ptr != '@'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
      if (*ptr == ':')
      {
         char *p_end = ptr + 1,
              tmp_buffer[MAX_RECIPIENT_LENGTH];

         ptr++;
         while (*ptr != '@')
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         (void)strcpy(tmp_buffer, ptr);
         *p_end = '\0';
         (void)strcat(value, "XXXXX");
         (void)strcat(value, tmp_buffer);
      }
   }
   (void)fprintf(stdout, "Recipient     : %s\n", value);
   (void)fprintf(stdout, "Real hostname : %s\n",
                 fsa[position].real_hostname[0]);
   if (fsa[position].real_hostname[1][0] != '\0')
   {
      (void)fprintf(stdout, "                %s\n",
                    fsa[position].real_hostname[1]);
   }

   /* Show all AMG options */
   if (p_jd->no_of_loptions > 0)
   {
      char *ptr = p_jd->loptions;

      (void)fprintf(stdout, "AMG-options   : %s\n", ptr);
      NEXT(ptr);
      for (i = 1; i < p_jd->no_of_loptions; i++)
      {
         (void)fprintf(stdout, "                %s\n", ptr);
         NEXT(ptr);
      }
   }

   /* Show all FD options */
   if (p_jd->no_of_soptions > 0)
   {
      int  counter = 0;
      char *ptr = p_jd->soptions;

      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         value[counter] = *ptr;
         ptr++; counter++;
      }
      value[counter] = '\0';
      ptr++;
      (void)fprintf(stdout, "FD-options    : %s\n", value);
      for (i = 1; i < p_jd->no_of_soptions; i++)
      {
         counter = 0;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            value[counter] = *ptr;
            ptr++; counter++;
         }
         value[counter] = '\0';
         (void)fprintf(stdout, "                %s\n", value);
         if (ptr == '\0')
         {
            break;
         }
      }
   }

   /* Priority */
   (void)fprintf(stdout, "Priority      : %c\n", p_jd->priority);

   /* Job ID */
   (void)fprintf(stdout, "Job-ID        : %d\n\n", p_jd->job_id);

   return;
}


/*######################## get_current_jid_list() #######################*/
static int
get_current_jid_list(void)
{
   int         fd;
   char        file[MAX_PATH_LENGTH],
               *ptr,
               *tmp_ptr;
   struct stat stat_buf;

   (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, CURRENT_MSG_LIST_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s. Will not be able to get all information. : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to fstat() %s. Will not be able to get all information. : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() to %s. Will not be able to get all information. : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }
   tmp_ptr = ptr;
   no_of_current_jobs = *(int *)ptr;
   ptr += sizeof(int);

   if (no_of_current_jobs > 0)
   {
      int i;

      if ((current_jid_list = malloc(no_of_current_jobs * sizeof(int))) == NULL)
      {
         (void)fprintf(stderr,
                       "Failed to malloc() memory. Will not be able to get all information. : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         return(INCORRECT);
      }
      for (i = 0; i < no_of_current_jobs; i++)
      {
         current_jid_list[i] = *(int *)ptr;
         ptr += sizeof(int);
      }
   }

   if (munmap(tmp_ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr, "Failed to munmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}
