/*
 *  store_passwd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   store_passwd - stores the passwd into an internal database
 **
 ** SYNOPSIS
 **   void store_passwd(char *recipient, int remove_passwd)
 **
 ** DESCRIPTION
 **   The function store_passwd() searches in the recipient string,
 **   which must be a URL, for a password. If it finds one it is
 **   stored in an internal database under the user and hostalias,
 **   making sure it does not already exist. If for the same user
 **   and host alias there already exist an entry with a different
 **   passwd, this passwd will be overwritten by this one and a
 **   warning is generated. In either cases the original passwd is
 **   removed from the URL when remove_passwd is set to YES.
 **
 ** RETURN VALUES
 **   None.
 **
 ** SEE ALSO
 **   common/get_hostname.c, common/create_message.c, fd/get_job_data.c,
 **   fd/eval_recipient.c, tools/get_dc_data.c, fd/init_msg_buffer.c,
 **   tools/set_pw.c
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.2003 H.Kiehl Created
 **   12.08.2004 H.Kiehl Don't write password in clear text.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* memmove(), strcpy(), strlen(), strerror()*/
#include <stdlib.h>          /* realloc()                                */
#include <sys/types.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int               *no_of_passwd,
                         pwb_fd;
extern char              *p_work_dir;
extern struct passwd_buf *pwb;


/*############################ store_passwd() ###########################*/
void
store_passwd(char *recipient, int remove_passwd)
{
   int           i,
                 uh_name_length;
   char          hostname[MAX_REAL_HOSTNAME_LENGTH],
                 *ptr,
                 uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1]; /* User Host name */
   unsigned char passwd[MAX_USER_NAME_LENGTH];

   /*
    * Lets first see if this recipient does have a user with
    * a password. If not we can return immediatly.
    */
   ptr = recipient;
   while ((*ptr != ':') && (*ptr != '\0'))
   {
      /* We don't need the scheme so lets ignore it here. */
      ptr++;
   }
   if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
   {
      ptr += 3; /* Away with '://' */

      if (*ptr == MAIL_GROUP_IDENTIFIER)
      {
         /* Aaahhh, this is easy! Here we do not have a password. */
         return;
      }
      else
      {
         /* Get user name. */
         if (*ptr == '\0')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Just telling me the sheme and nothing else is not of much use!");
            return;
         }
         i = 0;
#ifdef WITH_SSH_FINGERPRINT
         while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
#else
         while ((*ptr != ':') && (*ptr != '@') &&
#endif
                (*ptr != '\0') && (i < MAX_USER_NAME_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            uh_name[i] = *ptr;
            ptr++; i++;
         }
         if (*ptr == '\0')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmm. This (%s) does NOT look like URL for me!?",
                       recipient);
            return;
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to store user name. It is longer than %d Bytes!",
                       MAX_USER_NAME_LENGTH);
            return;
         }
         uh_name_length = i;
#ifdef WITH_SSH_FINGERPRINT
         if (*ptr == ';')
         {
            while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               ptr++;
            }
         }
#endif
         if (*ptr == ':')
         {
            char *p_start_pwd;
            
            p_start_pwd = ptr;
            ptr++;

            /* Get password. */
            i = 0;
            while ((*ptr != '@') && (*ptr != '\0') &&
                   (i < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               if ((i % 2) == 0)
               {
                  passwd[i] = *ptr - 24 + i;
               }
               else
               {
                  passwd[i] = *ptr - 11 + i;
               }
               ptr++; i++;
            }
            if ((i == 0) && (*ptr != '@'))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Hmmm. How am I suppose to find the hostname?");
               return;
            }
            if (i == MAX_USER_NAME_LENGTH)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to store password. It is longer than %d Bytes!",
                          MAX_USER_NAME_LENGTH);
               return;
            }
            passwd[i] = '\0';

            if (remove_passwd == YES)
            {
               /* Remove the password. */
               (void)memmove(p_start_pwd, ptr, strlen(ptr) + 1);
               ptr = p_start_pwd + 1;
            }
            else
            {
               ptr++;
            }
         }
         else if (*ptr == '@')
              {
                 /* Good, no password given, so lets just return. */
                 return;
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Hmmm. How am I suppose to find the hostname?");
                 return;
              }

         /* Now lets get the host alias name. */
         i = 0;
         while ((*ptr != '\0') && (*ptr != '/') &&
                (*ptr != ':') && (*ptr != ';') &&
                (i < MAX_REAL_HOSTNAME_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            hostname[i] = *ptr;
            uh_name[uh_name_length + i] = *ptr;
            i++; ptr++;
         }
         hostname[i] = '\0';
         uh_name[uh_name_length + i] = '\0';
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Hmm. This does NOT look like URL for me!?");
      return;
   }

   if (pwb == NULL)
   {
      size_t size = (PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                    AFD_WORD_OFFSET;
      char   pwb_file_name[MAX_PATH_LENGTH];
                  
      (void)strcpy(pwb_file_name, p_work_dir);
      (void)strcat(pwb_file_name, FIFO_DIR);  
      (void)strcat(pwb_file_name, PWB_DATA_FILE);            
      if ((ptr = attach_buf(pwb_file_name, &pwb_fd, size, DC_PROC_NAME,
#ifdef GROUP_CAN_WRITE
                            (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP),
                            NO)) == (caddr_t) -1)
#else
                            (S_IRUSR | S_IWUSR), NO)) == (caddr_t) -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() to %s : %s",
                    pwb_file_name, strerror(errno));
         exit(INCORRECT);
      }
      no_of_passwd = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      pwb = (struct passwd_buf *)ptr;

      if (*no_of_passwd > 0)
      {
         for (i = 0; i < *no_of_passwd; i++)
         {
            pwb[i].dup_check = NO;
         }
      }
      else
      {
         ptr -= AFD_WORD_OFFSET;
         *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
         *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
         *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_PWB_VERSION;
         (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
      }

#ifdef LOCK_DEBUG
      lock_region_w(pwb_fd, 1, __FILE__, __LINE__);
#else
      lock_region_w(pwb_fd, 1);
#endif
   }

   /*
    * First check if the password is already stored.
    */
   for (i = 0; i < *no_of_passwd; i++)
   {
      if (CHECK_STRCMP(pwb[i].uh_name, uh_name) == 0)
      {
         if (CHECK_STRCMP((char *)pwb[i].passwd, (char *)passwd) == 0)
         {
            /* Password is already stored. */
            pwb[i].dup_check = YES;
            return;
         }
         else
         {
            if (pwb[i].dup_check == NO)
            {
               pwb[i].dup_check = YES;
            }
            else
            {
               (void)system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Different passwords for user@%s",
                                hostname);
            }
            (void)strcpy((char *)pwb[i].passwd, (char *)passwd);
            return;
         }
      }
   }

   /*
    * Password is not in the stored list so this must be a new one.
    * Add it to this list.
    */
   if ((*no_of_passwd != 0) &&
       ((*no_of_passwd % PWB_STEP_SIZE) == 0))
   {
      size_t new_size = (((*no_of_passwd / PWB_STEP_SIZE) + 1) *
                        PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)pwb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(pwb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_of_passwd = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      pwb = (struct passwd_buf *)ptr;
   }
   (void)strcpy(pwb[*no_of_passwd].uh_name, uh_name);
   (void)strcpy((char *)pwb[*no_of_passwd].passwd, (char *)passwd);
   pwb[*no_of_passwd].dup_check = YES;
   (*no_of_passwd)++;
   
   return;
}
