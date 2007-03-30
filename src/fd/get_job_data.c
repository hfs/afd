/*
 *  get_job_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int get_job_data(unsigned int job_id,
 **                    int          mdb_position,
 **                    time_t       msg_mtime,
 **                    off_t        msg_size)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to store the message, otherwise INCORRECT
 **   is returned.
 **
 ** SEE ALSO
 **   common/get_hostname.c, common/create_message.c, fd/eval_recipient.c,
 **   amg/store_passwd.c, fd/init_msg_buffer.c, tools/get_dc_data.c,
 **   tools/set_pw.c
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.01.1998 H.Kiehl Created
 **   31.01.2005 H.Kiehl Store the port as well.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strcpy(), strcmp(), strerror()           */
#include <stdlib.h>          /* malloc(), free(), atoi()                 */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>          /* fstat(), read(), close(), unlink()       */
#include <fcntl.h>           /* open()                                   */
#include <errno.h>
#include "fddefs.h"

/* External global variables */
extern int                        mdb_fd,
                                  no_of_hosts,
                                  *no_msg_cached;
extern char                       msg_dir[],
                                  *p_msg_dir;
extern struct filetransfer_status *fsa;
extern struct msg_cache_buf       *mdb;


/*########################### get_job_data() ############################*/
int
get_job_data(unsigned int job_id,
             int          mdb_position,
             time_t       msg_mtime,
             off_t        msg_size)
{
   int  fd,
        port,
        pos,
        length;
   char sheme,
        *file_buf,
        *ptr,
        *p_start,
        host_name[MAX_HOSTNAME_LENGTH + 1];

   (void)sprintf(p_msg_dir, "%x", job_id);

retry:
   if ((fd = open(msg_dir, O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm. No message for job `%x'. Will try recreate it.",
                    job_id);
         if (recreate_msg(job_id) < 0)
         {
            return(INCORRECT);
         }
         goto retry;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to open() %s : %s",
                    msg_dir, strerror(errno));
         return(INCORRECT);
      }
   }
   if (mdb_position == -1)
   {
      struct stat stat_buf;

      if (fstat(fd, &stat_buf) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() %s : %s", msg_dir, strerror(errno));
         (void)close(fd);
         return(INCORRECT);
      }
      msg_size = stat_buf.st_size;
      msg_mtime = stat_buf.st_mtime;
   }
   if ((file_buf = malloc(msg_size + 1)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error [%d bytes] : %s",
                 msg_size + 1, strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }
   if (read(fd, file_buf, msg_size) != msg_size)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to read() %s : %s", msg_dir, strerror(errno));
      (void)close(fd);
      free(file_buf);
      return(INCORRECT);
   }
   file_buf[msg_size] = '\0';
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /*
    * First let's evaluate the recipient.
    */
   if ((ptr = posi(file_buf, DESTINATION_IDENTIFIER)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. It is not a message.", msg_dir);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
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
      if ((*p_start == 'f') && (*(p_start + 1) == 't') &&
#ifdef WITH_SSL
          (*(p_start + 2) == 'p') && ((*(p_start + 3) == '\0') ||
          (((*(p_start + 3) == 's') || (*(p_start + 3) == 'S')) &&
          (*(p_start + 4) == '\0'))))
#else
          (*(p_start + 2) == 'p') && (*(p_start + 3) == '\0'))
#endif
      {
         sheme = FTP;
      }
      else
      {
         if (CHECK_STRCMP(p_start, LOC_SHEME) != 0)
         {
#ifdef _WITH_SCP_SUPPORT
            if (CHECK_STRCMP(p_start, SCP_SHEME) != 0)
            {
#endif /* _WITH_SCP_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
               if (CHECK_STRCMP(p_start, WMO_SHEME) != 0)
               {
#endif
#ifdef _WITH_MAP_SUPPORT
                  if (CHECK_STRCMP(p_start, MAP_SHEME) != 0)
                  {
#endif
                     if ((*p_start == 's') &&
                         (*(p_start + 1) == 'f') &&
                         (*(p_start + 2) == 't') &&
                         (*(p_start + 3) == 'p') &&
                         (*(p_start + 4) == '\0'))
                     {
                        sheme = SFTP;
                     }
                     else
                     {
                        if ((*p_start == 'h') &&
                            (*(p_start + 1) == 't') &&
                            (*(p_start + 2) == 't') &&
                            (*(p_start + 3) == 'p') &&
#ifdef WITH_SSL
                            ((*(p_start + 4) == '\0') ||
                             ((*(p_start + 4) == 's') &&
                              (*(p_start + 5) == '\0'))))
#else
                            (*(p_start + 4) == '\0'))
#endif
                        {
                           sheme = HTTP;
                        }
                        else
                        {
                           if ((*p_start == 'm') &&
                               (*(p_start + 1) == 'a') &&
                               (*(p_start + 2) == 'i') &&
                               (*(p_start + 3) == 'l') &&
                               (*(p_start + 4) == 't') &&
                               (*(p_start + 5) == 'o') &&
#ifdef WITH_SSL
                               ((*(p_start + 6) == '\0') ||
                                ((*(p_start + 6) == 's') &&
                                 (*(p_start + 7) == '\0'))))
#else
                               (*(p_start + 6) == '\0'))
#endif
                           {
                              sheme = SMTP;
                           }
                           else
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Removing %s because of unknown sheme [%s].",
                                         msg_dir, p_start);
                              if (unlink(msg_dir) == -1)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to unlink() %s : %s",
                                            msg_dir, strerror(errno));
                              }
                              free(file_buf);
                              return(INCORRECT);
                           }
                        }
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
#ifdef _WITH_SCP_SUPPORT
            }
            else
            {
               sheme = SCP;
            }
#endif /* _WITH_SCP_SUPPORT */
         }
         else
         {
            sheme = LOC;
         }
      }
   }
   else
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. Could not locate end of sheme [:].", msg_dir);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
      }
      free(file_buf);
      return(INCORRECT);
   }

   ptr += 3;
   length = 0;
   if (*ptr == MAIL_GROUP_IDENTIFIER)
   {
      ptr++;
      while ((*ptr != '@') && (*ptr != ':') && (*ptr != '\n') &&
             (*ptr != ';') && (length < MAX_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         host_name[length] = *ptr;
         ptr++; length++;
      }
   }

   /* On we go with the hostname. */
   while ((*ptr != '@') && (*ptr != '\n') && (*ptr != ';'))
   {
      if (*ptr == '\\')
      {
         ptr++;
      }
      ptr++;
   }
#ifdef WITH_SSH_FINGERPRINT
   if (*ptr == ';')
   {
      char *tmp_ptr = ptr + 1;

      while ((*tmp_ptr != '@') && (*tmp_ptr != '\n') && (*tmp_ptr != ';'))
      {
         if (*tmp_ptr == '\\')
         {
            tmp_ptr++;
         }
         tmp_ptr++;
      }
      if ((*tmp_ptr == '@') || (*tmp_ptr == ';'))
      {
         ptr = tmp_ptr;
      }
   }
#endif
   if (*ptr == '@')
   {
      ptr++;
      length = 0;
      while ((*ptr != '/') && (*ptr != '.') &&
             (*ptr != ':') && (*ptr != '\n') &&
             (*ptr != ';') && (length < MAX_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         host_name[length] = *ptr;
         ptr++; length++;
      }
   }

   /* Ignore rest of host name. */
   if ((*ptr == '.') ||
       ((length == MAX_HOSTNAME_LENGTH) && (*ptr != '/')))
   {
      while ((*ptr != '\n') && (*ptr != '/') &&
             (*ptr != ':') && (*ptr != ';'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
   }

   /* Store port if it is there. */
   if (*ptr == ':')
   {
      char *ptr_tmp;

      ptr++;
      ptr_tmp = ptr;
      while ((*ptr != '/') && (*ptr != '\n') && (*ptr != ';') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (ptr != ptr_tmp) /* Ensure that port number is there. */
      {
         if ((*ptr == '/') || (*ptr != '\n') || (*ptr != ';'))
         {
            char tmp_char;

            tmp_char = *ptr;
            *ptr = '\0';
            port = atoi(ptr_tmp);
            *ptr = tmp_char;
         }
         else if (*ptr == '\0')
              {
                 port = atoi(ptr_tmp);
              }
              else
              {
                 port = -1;
              }
      }
      else
      {
         port = -1;
      }
   }
   else
   {
      port = -1;
   }

   /* No need for the directory. */
   if (*ptr == '/')
   {
      while ((*ptr != '\n') && (*ptr != ';') && (*ptr != '\0'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
   }

   /*
    * NOTE: For mail we take the hostname from the "server=" if
    *       it does exist.
    */
   if ((*ptr == ';') && (*(ptr + 1) == 's') && (*(ptr + 2) == 'e') &&
       (*(ptr + 3) == 'r') && (*(ptr + 4) == 'v') &&
       (*(ptr + 5) == 'e') && (*(ptr + 6) == 'r') &&
       (*(ptr + 7) == '='))
   {
      /* Store the hostname. */
      ptr += 8;
      length = 0;
      while ((*ptr != '\n') && (*ptr != '.') && (length < MAX_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         host_name[length] = *ptr;
         length++; ptr++;
      }
   }
   if (length > 0)
   {
      host_name[length] = '\0';
      if ((pos = get_host_position(fsa, host_name, no_of_hosts)) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to locate host %s in FSA [%s]. Ignoring!",
                    host_name, msg_dir);
         free(file_buf);
         return(INCORRECT);
      }
   }
   else
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Removing %s. Could not locate host name.", msg_dir);
      if (unlink(msg_dir) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to unlink() %s : %s", msg_dir, strerror(errno));
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
         if ((tmp_ptr = mmap_resize(mdb_fd, tmp_ptr,
                                    new_size)) == (caddr_t) -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "mmap() error : %s", strerror(errno));
            exit(INCORRECT);
         }
         no_msg_cached = (int *)tmp_ptr;
         tmp_ptr += AFD_WORD_OFFSET;
         mdb = (struct msg_cache_buf *)tmp_ptr;
      }
      (void)strcpy(mdb[*no_msg_cached - 1].host_name, host_name);
      mdb[*no_msg_cached - 1].fsa_pos = pos;
      mdb[*no_msg_cached - 1].job_id = job_id;
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
         while (isdigit((int)(*ptr)))
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
      mdb[*no_msg_cached - 1].type = sheme;
      mdb[*no_msg_cached - 1].port = port;
      mdb[*no_msg_cached - 1].msg_time = msg_mtime;
      mdb[*no_msg_cached - 1].last_transfer_time = 0L;
   }
   else
   {
      (void)strcpy(mdb[mdb_position].host_name, host_name);
      mdb[mdb_position].fsa_pos = pos;
      mdb[mdb_position].job_id = job_id;
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
         while (isdigit((int)(*ptr)))
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
      mdb[mdb_position].type = sheme;
      mdb[*no_msg_cached - 1].port = port;
      mdb[mdb_position].msg_time = msg_mtime;
      /*
       * NOTE: Do NOT initialize last_transfer_time! This could
       *       lead to a to early deletion of a job.
       */
   }

   free(file_buf);

   return(SUCCESS);
}
