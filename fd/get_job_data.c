/*
 *  get_job_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_job_data - appends a message to the msg_cache_buf structure
 **
 ** SYNOPSIS
 **   int get_job_data(unsigned int job_id, int mdb_position)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to store the message, otherwise INCORRECT
 **   is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf(), remove()                      */
#include <string.h>          /* strcpy(), strcmp(), strerror()           */
#include <stdlib.h>          /* malloc(), free(), atoi()                 */
#ifdef _AGE_LIMIT
#include <ctype.h>           /* isdigit()                                */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* fstat(), read(), close()                 */
#include <fcntl.h>           /* open()                                   */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int                        mdb_fd,
                                  no_of_hosts,
                                  sys_log_fd,
                                  *no_msg_cached;
extern char                       msg_dir[],
                                  *p_msg_dir;
extern struct filetransfer_status *fsa;
extern struct msg_cache_buf       *mdb;


/*########################### get_job_data() ############################*/
int
get_job_data(unsigned int job_id, int mdb_position)
{
   int         fd,
               pos;
   char        sheme,
               *file_buf,
               *ptr,
               *p_start,
               host_name[MAX_HOSTNAME_LENGTH + 1];
   struct stat stat_buf;

   (void)sprintf(p_msg_dir, "%u", job_id);

retry:
   if ((fd = open(msg_dir, O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Hmmm. No message for JID %d. Will try recreate it. (%s %d)\n",
                   job_id, __FILE__, __LINE__);
         if (recreate_msg(job_id) < 0)
         {
            return(INCORRECT);
         }
         goto retry;
      }
      else
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to open() %s : %s (%s %d)\n",
                   msg_dir, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to fstat() %s : %s (%s %d)\n",
                msg_dir, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }
   if ((file_buf = malloc(stat_buf.st_size + 1)) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      exit(INCORRECT);
   }
   if (read(fd, file_buf, stat_buf.st_size) != stat_buf.st_size)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to read() %s : %s (%s %d)\n",
                msg_dir, strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      free(file_buf);
      return(INCORRECT);
   }
   file_buf[stat_buf.st_size] = '\0';
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /*
    * First let's evaluate the recipient.
    */
   if ((ptr = posi(file_buf, DESTINATION_IDENTIFIER)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Removing %s. It is not a message. (%s %d)\n",
                msg_dir, __FILE__, __LINE__);
#ifdef _WORKING_UNLINK
      if (unlink(msg_dir) == -1)
#else
      if (remove(msg_dir) == -1)
#endif /* _WORKING_UNLINK */
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to delete %s : %s (%s %d)\n",
                   msg_dir, strerror(errno), __FILE__, __LINE__);
      }
      free(file_buf);
      if (recreate_msg(job_id) < 0)
      {
         return(INCORRECT);
      }
      goto retry;
   }

   /* Lets determine the sheme. */
   p_start = ptr;
   while ((*ptr != ':') && (*ptr != '\n'))
   {
      ptr++;
   }
   if (*ptr == ':')
   {
      *ptr = '\0';
      if (strcmp(p_start, FTP_SHEME) != 0)
      {
         if (strcmp(p_start, LOC_SHEME) != 0)
         {
#ifdef _WITH_WMO_SUPPORT
            if (strcmp(p_start, WMO_SHEME) != 0)
            {
#endif
#ifdef _WITH_MAP_SUPPORT
               if (strcmp(p_start, MAP_SHEME) != 0)
               {
#endif
                  if (strcmp(p_start, SMTP_SHEME) != 0)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Removing %s because of unknown sheme [%s]. (%s %d)\n",
                               msg_dir, p_start, __FILE__, __LINE__);
#ifdef _WORKING_UNLINK
                     if (unlink(msg_dir) == -1)
#else
                     if (remove(msg_dir) == -1)
#endif /* _WORKING_UNLINK */
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to delete %s : %s (%s %d)\n",
                                  msg_dir, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                     free(file_buf);
                     return(INCORRECT);
                  }
                  else
                  {
                     sheme = SMTP;
                  }
#ifdef _WITH_MAP_SUPPORT
               }
               else
               {
                  sheme = MAP;
               }
#endif
#ifdef _WITH_WMO_SUPPORT
            }
            else
            {
               sheme = WMO;
            }
#endif
         }
         else
         {
            sheme = LOC;
         }
      }
      else
      {
         sheme = FTP;
      }
   }
   else
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Removing %s. Could not locate end of sheme [:]. (%s %d)\n",
                msg_dir, __FILE__, __LINE__);
#ifdef _WORKING_UNLINK
      if (unlink(msg_dir) == -1)
#else
      if (remove(msg_dir) == -1)
#endif /* _WORKING_UNLINK */
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to delete %s : %s (%s %d)\n",
                   msg_dir, strerror(errno), __FILE__, __LINE__);
      }
      free(file_buf);
      return(INCORRECT);
   }

   /* On we go with the hostname. */
   while ((*ptr != '@') && (*ptr != '\n'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }
   if (*ptr == '@')
   {
      int length = 0;

      ptr++;
      while ((*ptr != '/') && (*ptr != '.') &&
             (*ptr != ':') && (*ptr != '\n') &&
             (*ptr != ';') &&
             (length < MAX_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         host_name[length] = *ptr;
         ptr++; length++;
      }
      host_name[length] = '\0';

      if ((pos = get_host_position(fsa, host_name, no_of_hosts)) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "Failed to locate host %s in FSA [%s]. Ignoring! (%s %d)\n",
                   host_name, msg_dir, __FILE__, __LINE__);
         free(file_buf);
         return(INCORRECT);
      }
   }
   else
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Removing %s. Could not locate start of host name [@]. (%s %d)\n",
                msg_dir, __FILE__, __LINE__);
#ifdef _WORKING_UNLINK
      if (unlink(msg_dir) == -1)
#else
      if (remove(msg_dir) == -1)
#endif /* _WORKING_UNLINK */
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to delete %s : %s (%s %d)\n",
                   msg_dir, strerror(errno), __FILE__, __LINE__);
      }
      free(file_buf);
      return(INCORRECT);
   }
   if (mdb_position == -1)
   {
      (*no_msg_cached)++;
      if ((*no_msg_cached != 0) &&
          ((*no_msg_cached % MSG_CACHE_BUF_SIZE) == 0))
      {
         char   *tmp_ptr;
         size_t new_size;

         new_size = (((*no_msg_cached / MSG_CACHE_BUF_SIZE) + 1) *
                    MSG_CACHE_BUF_SIZE * sizeof(struct msg_cache_buf)) +
                    AFD_WORD_OFFSET;
         tmp_ptr = (char *)mdb - AFD_WORD_OFFSET;
         if ((tmp_ptr = mmap_resize(mdb_fd, tmp_ptr, new_size)) == (caddr_t) -1)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "mmap() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         no_msg_cached = (int *)tmp_ptr;
         tmp_ptr += AFD_WORD_OFFSET;
         mdb = (struct msg_cache_buf *)tmp_ptr;
      }
      (void)strcpy(mdb[*no_msg_cached - 1].host_name, host_name);
      mdb[*no_msg_cached - 1].fsa_pos = pos;
      mdb[*no_msg_cached - 1].job_id = job_id;
#ifdef _AGE_LIMIT
      if ((ptr = posi(ptr, AGE_LIMIT_ID)) == NULL)
      {
         mdb[*no_msg_cached - 1].age_limit = 0;
      }
      else
      {
         int  k = 0;
         char age_limit_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit(*ptr))
         {
            age_limit_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            age_limit_str[k] = '\0';
            mdb[*no_msg_cached - 1].age_limit = atoi(age_limit_str);
         }
         else
         {
            mdb[*no_msg_cached - 1].age_limit = 0;
         }
      }
#endif
      mdb[*no_msg_cached - 1].type = sheme;
      mdb[*no_msg_cached - 1].msg_time = stat_buf.st_mtime;
      mdb[*no_msg_cached - 1].last_transfer_time = 0L;
   }
   else
   {
      (void)strcpy(mdb[mdb_position].host_name, host_name);
      mdb[mdb_position].fsa_pos = pos;
      mdb[mdb_position].job_id = job_id;
#ifdef _AGE_LIMIT
      if ((ptr = posi(ptr, AGE_LIMIT_ID)) == NULL)
      {
         mdb[mdb_position].age_limit = 0;
      }
      else
      {
         int  k = 0;
         char age_limit_str[MAX_INT_LENGTH + 1];

         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (isdigit(*ptr))
         {
            age_limit_str[k] = *ptr;
            ptr++; k++;
         }
         if (k > 0)
         {
            age_limit_str[k] = '\0';
            mdb[mdb_position].age_limit = atoi(age_limit_str);
         }
         else
         {
            mdb[mdb_position].age_limit = 0;
         }
      }
#endif
      mdb[mdb_position].type = sheme;
      mdb[mdb_position].msg_time = stat_buf.st_mtime;
      /*
       * NOTE: Do NOT initialize last_transfer_time! This could
       *       lead to a to early deletion of a job.
       */
   }

   free(file_buf);

   return(SUCCESS);
}
