/*
 *  eval_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2003 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_message - reads the recipient string
 **
 ** SYNOPSIS
 **   int eval_message(char *message_name, struct job *p_db)
 **
 ** DESCRIPTION
 **   Reads and evaluates the message file 'message_name'. If this
 **   message is faulty it is moved to the faulty directory.
 **
 ** RETURN VALUES
 **   Returns INCORRECT if it fails to evaluate the message file.
 **   On success it returns SUCCESS and the following structure:
 **   struct job *p_db   - The structure in which we store the
 **                        following values:
 **
 **                             - user
 **                             - password
 **                             - hostname
 **                             - directory
 **                             - send as email
 **                             - lock type
 **                             - archive time
 **                             - trans_rename_rule
 **                             - user_rename_rule
 **                             - user_id
 **                             - group_id
 **                             - FTP transfer mode
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.11.1995 H.Kiehl Created
 **   16.09.1997 H.Kiehl Support for FSS ready files.
 **   08.10.1997 H.Kiehl Included renaming with directories on remote site.
 **   30.11.1997 H.Kiehl Take last RESTART_FILE_ID in message.
 **   25.01.1998 H.Kiehl Allow for more then one restart file.
 **   24.06.1998 H.Kiehl Added option for secure ftp.
 **   26.04.1999 H.Kiehl Added option no login
 **   24.08.1999 H.Kiehl Enhanced option "attach file" to support trans-
 **                      renaming.
 **   18.03.2000 H.Kiehl Added option FTP transfer mode.
 **   03.11.2000 H.Kiehl Chmod option for FTP as well.
 **   27.01.2002 H.Kiehl Added reply-to option.
 **   02.07.2002 H.Kiehl Added from option.
 **   03.07.2002 H.Kiehl Added charset option.
 **   30.07.2002 H.Kiehl Check if subject string contains a %t to insert
 **                      a date.
 **   15.07.2003 H.Kiehl Added option "rename file busy".
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isascii()                           */
#include <time.h>                 /* time(), strftime(), localtime()     */
#include <sys/types.h>
#include <grp.h>                  /* getgrnam()                          */
#include <pwd.h>                  /* getpwnam()                          */
#include <unistd.h>               /* read(), close(), setuid()           */
#include <fcntl.h>                /* O_RDONLY, etc                       */
#ifdef _WITH_EUMETSAT_HEADERS
#include <netdb.h>                /* gethostbyname()                     */
#endif
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"

/* External global variables */
extern int transfer_log_fd;

#define ARCHIVE_FLAG              1
#define AGE_LIMIT_FLAG            2
#define LOCK_FLAG                 4
#define TRANS_RENAME_FLAG         8
#define CHMOD_FLAG                16
#define CHOWN_FLAG                32
#define OUTPUT_LOG_FLAG           64
#define RESTART_FILE_FLAG         128
#define FILE_NAME_IS_HEADER_FLAG  256
#define SUBJECT_FLAG              512
#define FORCE_COPY_FLAG           1024
#define FILE_NAME_IS_SUBJECT_FLAG 2048
#define REPLY_TO_FLAG             4096
#define FROM_FLAG                 8192
#define CHECK_ANSI_FLAG           16384
#define CHECK_REPLY_FLAG          32768
#define WITH_SEQUENCE_NUMBER_FLAG 65536
#define ATTACH_FILE_FLAG          131072
#define ADD_MAIL_HEADER_FLAG      262144
#define FTP_EXEC_FLAG             524288
#define CHARSET_FLAG              1048576
#define FILE_NAME_IS_USER_FLAG    2097152
#ifdef _WITH_EUMETSAT_HEADERS
#define EUMETSAT_HEADER_FLAG      4194304
#endif /* _WITH_EUMETSAT_HEADERS */
#ifdef _RADAR_CHECK
#define RADAR_FLAG                8388608
#endif /* _RADAR_CHECK */
#define CHANGE_FTP_MODE_FLAG      16777216
#define ATTACH_ALL_FILES_FLAG     33554432
#ifdef _WITH_TRANS_EXEC
#define TRANS_EXEC_FLAG           67108864
#endif /* _WITH_TRANS_EXEC */
#define RENAME_FILE_BUSY_FLAG     134217728
#define LOCK_POSTFIX_FLAG         268435456

#define MAX_HUNK                  4096


/*############################ eval_message() ###########################*/
int
eval_message(char *message_name, struct job *p_db)
{
   int  bytes_buffered,
        fd,
        n,
        used = 0; /* Used to see whether option has already been set. */
   char *ptr,
        *end_ptr,
        *msg_buf,
        byte_buf;

   /* Store message to buffer. */
   if ((fd = open(message_name, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", message_name, strerror(errno));
      exit(NO_MESSAGE_FILE);
   }

   /* Allocate memory to store message. */
   if ((msg_buf = malloc(MAX_HUNK + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not malloc() memory to read message : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   ptr = msg_buf;
   bytes_buffered = 0;
   do
   {
      if ((n = read(fd, ptr, MAX_HUNK)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to read() %s : %s", message_name, strerror(errno));
         exit(INCORRECT);
      }
      bytes_buffered += n;
      if (n == MAX_HUNK)
      {
         if ((msg_buf = realloc(msg_buf, bytes_buffered + MAX_HUNK + 1)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory to read message : %s",
                       strerror(errno));
            exit(INCORRECT);
         }
         ptr = msg_buf + bytes_buffered;
      }
   } while (n == MAX_HUNK);

   msg_buf[bytes_buffered] = '\0';
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   /*
    * First let's evaluate the recipient.
    */
   if ((ptr = posi(msg_buf, DESTINATION_IDENTIFIER)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Message %s is faulty.", message_name);

      /* Don't forget to free memory we have allocated */
      free(msg_buf);

      /* It's obvious that the message is corrupt */
      return(INCORRECT);
   }
   end_ptr = ptr;
   while ((*end_ptr != '\0') && (*end_ptr != '\n'))
   {
      end_ptr++;
   }
   byte_buf = *end_ptr;
   *end_ptr = '\0';

   if (eval_recipient(ptr, p_db, message_name, 0) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Message %s has a faulty recipient.", message_name);

      /* Don't forget to free memory we have allocated */
      free(msg_buf);

      return(INCORRECT);
   }

   if (byte_buf != '\0')
   {
      /* Put back character that was torn out. */
      *end_ptr = byte_buf;
      ptr = end_ptr;

      /*
       * Now lets see which options have been set.
       */
      if ((ptr = posi(ptr, OPTION_IDENTIFIER)) != NULL)
      {
         while (*ptr != '\0')
         {
            if (((used & ARCHIVE_FLAG) == 0) &&
                (strncmp(ptr, ARCHIVE_ID, ARCHIVE_ID_LENGTH) == 0))
            {
               used |= ARCHIVE_FLAG;
               ptr += ARCHIVE_ID_LENGTH;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               end_ptr = ptr;
               while ((*end_ptr != '\n') && (*end_ptr != '\0'))
               {
                 end_ptr++;
               }
               byte_buf = *end_ptr;
               *end_ptr = '\0';
               p_db->archive_time = atoi(ptr);
               *end_ptr = byte_buf;
               ptr = end_ptr;
               while (*ptr == '\n')
               {
                  ptr++;
               }
            }
#ifdef _AGE_LIMIT
            else if (((used & AGE_LIMIT_FLAG) == 0) &&
                     (strncmp(ptr, AGE_LIMIT_ID, AGE_LIMIT_ID_LENGTH) == 0))
                 {
                    used |= AGE_LIMIT_FLAG;
                    ptr += AGE_LIMIT_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    p_db->age_limit = (unsigned int)atoi(ptr);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif
            else if (((used & LOCK_POSTFIX_FLAG) == 0) &&
                     (strncmp(ptr, LOCK_POSTFIX_ID, LOCK_POSTFIX_ID_LENGTH) == 0))
                 {
                    size_t length = 0;

                    used |= LOCK_POSTFIX_FLAG;
                    ptr += LOCK_POSTFIX_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                       end_ptr++;
                       length++;
                    }
                    if (length > 0)
                    {
                       if (length > 39)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Lock postfix notation `%s' in message `%s' is %d Bytes long, it may only be 40 Bytes long.",
                                     LOCK_POSTFIX_ID, message_name, length);
                          p_db->lock = OFF;
                       }
                       else
                       {
                          p_db->lock = POSTFIX;
                          memcpy(p_db->lock_notation, ptr, length);
                          p_db->lock_notation[length] = '\0';
                       }
                    }
                    else
                    {
                       p_db->lock = OFF;
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "No postfix found for option `%s' in message `%s'.",
                                  LOCK_POSTFIX_ID, message_name);
                    }
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & LOCK_FLAG) == 0) &&
                     (strncmp(ptr, LOCK_ID, LOCK_ID_LENGTH) == 0))
                 {
                    used |= LOCK_FLAG;
                    ptr += LOCK_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                       end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    if (CHECK_STRCMP(ptr, LOCK_DOT) == 0)
                    {
                       p_db->lock = DOT;
                    }
                    else if (CHECK_STRCMP(ptr, LOCK_DOT_VMS) == 0)
                         {
                            p_db->lock = DOT_VMS;
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_FILE) == 0)
                         {
                            p_db->lock = LOCKFILE;
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_OFF) == 0)
                         {
                            p_db->lock = OFF;
                         }
#ifdef _WITH_READY_FILES
                    else if (CHECK_STRCMP(ptr, LOCK_READY_A_FILE) == 0)
                         {
                            p_db->lock = READY_A_FILE;
                         }
                    else if (CHECK_STRCMP(ptr, LOCK_READY_B_FILE) == 0)
                         {
                            p_db->lock = READY_B_FILE;
                         }
#endif /* _WITH_READY_FILES */
                         else
                         {
                            size_t length;

                            length = strlen(ptr);
                            if (length > 39)
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "Lock notation `%s' in message `%s' is %d Bytes long, it may only be 40 Bytes long.",
                                          LOCK_ID, message_name, length);
                               p_db->lock = OFF;
                            }
                            else
                            {
                               p_db->lock = DOT;
                               (void)strcpy(p_db->lock_notation, ptr);
                            }
                         }
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & TRANS_RENAME_FLAG) == 0) &&
                     (strncmp(ptr, TRANS_RENAME_ID, TRANS_RENAME_ID_LENGTH) == 0))
                 {
                    used |= TRANS_RENAME_FLAG;
                    ptr += TRANS_RENAME_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    n = 0;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
                           (n < MAX_RULE_HEADER_LENGTH))
                    {
                      p_db->trans_rename_rule[n] = *end_ptr;
                      end_ptr++; n++;
                    }
                    p_db->trans_rename_rule[n] = '\0';
                    if (n == MAX_RULE_HEADER_LENGTH)
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Rule header for trans_rename option %s to long. #%d",
                                  p_db->trans_rename_rule, p_db->job_id);
                    }
                    else if (n == 0)
                         {
                            system_log(WARN_SIGN, __FILE__, __LINE__,
                                       "No rule header specified in message %d.",
                                       p_db->job_id);
                         }
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHMOD_FLAG) == 0) &&
                     (strncmp(ptr, CHMOD_ID, CHMOD_ID_LENGTH) == 0))
                 {
                    used |= CHMOD_FLAG;
                    ptr += CHMOD_ID_LENGTH;
                    if (p_db->protocol & LOC_FLAG)
                    {
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0') &&
                              (*end_ptr != ' ') && (*end_ptr != '\t'))
                       {
                          end_ptr++;
                       }
                       n = end_ptr - ptr;
                       if ((n == 3) || (n == 4))
                       {
                          int error_flag = NO;

                          p_db->chmod = 0;
                          if (n == 4)
                          {
                             if (*ptr == '7')
                             {
                                p_db->chmod |= S_ISUID | S_ISGID | S_ISVTX;
                             }
                             else if (*ptr == '6')
                                  {
                                     p_db->chmod |= S_ISUID | S_ISGID;
                                  }
                             else if (*ptr == '5')
                                  {
                                     p_db->chmod |= S_ISUID | S_ISVTX;
                                  }
                             else if (*ptr == '4')
                                  {
                                     p_db->chmod |= S_ISUID;
                                  }
                             else if (*ptr == '3')
                                  {
                                     p_db->chmod |= S_ISGID | S_ISVTX;
                                  }
                             else if (*ptr == '2')
                                  {
                                     p_db->chmod |= S_ISGID;
                                  }
                             else if (*ptr == '1')
                                  {
                                     p_db->chmod |= S_ISVTX;
                                  }
                             else if (*ptr == '0')
                                  {
                                     /* Nothing to be done here. */;
                                  }
                                  else
                                  {
                                     error_flag = YES;
                                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                                "Incorrect parameter for chmod option %c%c%c%c",
                                                ptr, ptr + 1, ptr + 2, ptr + 3);
                                  }
                             ptr++;
                          }
                          if (*ptr == '7')
                          {
                             p_db->chmod |= S_IRUSR | S_IWUSR | S_IXUSR;
                          }
                          else if (*ptr == '6')
                               {
                                  p_db->chmod |= S_IRUSR | S_IWUSR;
                               }
                          else if (*ptr == '5')
                               {
                                  p_db->chmod |= S_IRUSR | S_IXUSR;
                               }
                          else if (*ptr == '4')
                               {
                                  p_db->chmod |= S_IRUSR;
                               }
                          else if (*ptr == '3')
                               {
                                  p_db->chmod |= S_IWUSR | S_IXUSR;
                               }
                          else if (*ptr == '2')
                               {
                                  p_db->chmod |= S_IWUSR;
                               }
                          else if (*ptr == '1')
                               {
                                  p_db->chmod |= S_IXUSR;
                               }
                          else if (*ptr == '0')
                               {
                                  /* Nothing to be done here. */;
                               }
                               else
                               {
                                  error_flag = YES;
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "Incorrect parameter for chmod option %c%c%c",
                                             ptr, ptr + 1, ptr + 2);
                               }
                          ptr++;
                          if (*ptr == '7')
                          {
                             p_db->chmod |= S_IRGRP | S_IWGRP | S_IXGRP;
                          }
                          else if (*ptr == '6')
                               {
                                  p_db->chmod |= S_IRGRP | S_IWGRP;
                               }
                          else if (*ptr == '5')
                               {
                                  p_db->chmod |= S_IRGRP | S_IXGRP;
                               }
                          else if (*ptr == '4')
                               {
                                  p_db->chmod |= S_IRGRP;
                               }
                          else if (*ptr == '3')
                               {
                                  p_db->chmod |= S_IWGRP | S_IXGRP;
                               }
                          else if (*ptr == '2')
                               {
                                  p_db->chmod |= S_IWGRP;
                               }
                          else if (*ptr == '1')
                               {
                                  p_db->chmod |= S_IXGRP;
                               }
                          else if (*ptr == '0')
                               {
                                  /* Nothing to be done here. */;
                               }
                               else
                               {
                                  error_flag = YES;
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "Incorrect parameter for chmod option %c%c%c",
                                             ptr, ptr + 1, ptr + 2);
                               }
                          ptr++;
                          if (*ptr == '7')
                          {
                             p_db->chmod |= S_IROTH | S_IWOTH | S_IXOTH;
                          }
                          else if (*ptr == '6')
                               {
                                  p_db->chmod |= S_IROTH | S_IWOTH;
                               }
                          else if (*ptr == '5')
                               {
                                  p_db->chmod |= S_IROTH | S_IXOTH;
                               }
                          else if (*ptr == '4')
                               {
                                  p_db->chmod |= S_IROTH;
                               }
                          else if (*ptr == '3')
                               {
                                  p_db->chmod |= S_IWOTH | S_IXOTH;
                               }
                          else if (*ptr == '2')
                               {
                                  p_db->chmod |= S_IWOTH;
                               }
                          else if (*ptr == '1')
                               {
                                  p_db->chmod |= S_IXOTH;
                               }
                          else if (*ptr == '0')
                               {
                                  /* Nothing to be done here. */;
                               }
                               else
                               {
                                  error_flag = YES;
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "Incorrect parameter for chmod option %c%c%c",
                                             ptr, ptr + 1, ptr + 2);
                               }
                          if (error_flag == NO)
                          {
                             p_db->special_flag |= CHANGE_PERMISSION;
                          }
                       }
                       ptr = end_ptr;
                    }
                    else if (p_db->protocol & FTP_FLAG)
                         {
                            while ((*ptr == ' ') || (*ptr == '\t'))
                            {
                               ptr++;
                            }
                            n = 0;
                            while ((*ptr != '\n') && (*ptr != '\0') &&
                                   (n < 4) && (isdigit(*ptr)))
                            {
                               p_db->chmod_str[n] = *ptr;
                               ptr++; n++;
                            }
                            if (n > 1)
                            {
                               p_db->chmod_str[n] = '\0';
                            }
                            else
                            {
                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                          "Incorrect parameter for chmod option, ignoring it.");
                               p_db->chmod_str[0] = '\0';
                            }
                         }
                         else
                         {
                            while ((*ptr != '\n') && (*ptr != '\0'))
                            {
                               ptr++;
                            }
                         }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHOWN_FLAG) == 0) &&
                     (strncmp(ptr, CHOWN_ID, CHOWN_ID_LENGTH) == 0))
                 {
                    used |= CHOWN_FLAG;
                    ptr += CHOWN_ID_LENGTH;
                    if (p_db->protocol & LOC_FLAG)
                    {
#ifdef _WITH_SETUID_CHECK
                       if (setuid(0) != -1)
                       {
#endif
                          int lookup_id = NO;

                          while ((*ptr == ' ') || (*ptr == '\t'))
                          {
                             ptr++;
                          }
                          end_ptr = ptr;
                          while ((*end_ptr != ' ') && (*end_ptr != ':') &&
                                 (*end_ptr != '.') && (*end_ptr != '\n') &&
                                 (*end_ptr != '\0'))
                          {
                             if (isdigit(*end_ptr) == 0)
                             {
                                lookup_id = YES;
                             }
                             end_ptr++;
                          }
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if (lookup_id == YES)
                          {
                             struct passwd *pw;

                             if ((pw = getpwnam(ptr)) == NULL)
                             {
                                (void)rec(transfer_log_fd, ERROR_SIGN,
                                          "getpwnam() error for user %s : %s (%s %d)\n",
                                          ptr, strerror(errno),
                                          __FILE__, __LINE__);
                             }
                             else
                             {
                                p_db->user_id = pw->pw_uid;
                             }
                          }
                          else
                          {
                             p_db->user_id = atoi(ptr);
                          }
                          p_db->special_flag |= CHANGE_UID_GID;
                          *end_ptr = byte_buf;
                          ptr = end_ptr;
                          if ((*ptr == ' ') || (*ptr == ':') ||
                              (*ptr == '.'))
                          {
                             ptr++; end_ptr++;
                             lookup_id = NO;
                             while ((*end_ptr != ' ') && (*end_ptr != ':') &&
                                    (*end_ptr != '.') && (*end_ptr != '\n') &&
                                    (*end_ptr != '\0'))
                             {
                                if (isdigit(*end_ptr) == 0)
                                {
                                   lookup_id = YES;
                                }
                                end_ptr++;
                             }
                             byte_buf = *end_ptr;
                             *end_ptr = '\0';
                             if (lookup_id == YES)
                             {
                                struct group *gr;

                                if ((gr = getgrnam(ptr)) == NULL)
                                {
                                   (void)rec(transfer_log_fd, ERROR_SIGN,
                                             "getgrnam() error for group %s : %s (%s %d)\n",
                                             ptr, strerror(errno),
                                             __FILE__, __LINE__);
                                }
                                else
                                {
                                   p_db->group_id = gr->gr_gid;
                                }
                             }
                             else
                             {
                                p_db->group_id = atoi(ptr);
                             }
                             *end_ptr = byte_buf;
                             ptr = end_ptr;
                          }
#ifdef _WITH_SETUID_CHECK
                       }
                       else
                       {
                          if (errno == EPERM)
                          {
                             (void)rec(transfer_log_fd, ERROR_SIGN,
                                       "Cannot change user or group ID if the program sf_xxx is not setuid root! (%s %d)\n",
                                       __FILE__, __LINE__);
                          }
                          else
                          {
                             (void)rec(transfer_log_fd, ERROR_SIGN,
                                       "setuid() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                          }
                          while ((*ptr != '\n') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                       }
#endif /* _WITH_SETUID_CHECK */
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & OUTPUT_LOG_FLAG) == 0) &&
                     (strncmp(ptr, OUTPUT_LOG_ID, OUTPUT_LOG_ID_LENGTH) == 0))
                 {
                    used |= OUTPUT_LOG_FLAG;
#ifdef _OUTPUT_LOG
                    p_db->output_log = NO;
#endif
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & RESTART_FILE_FLAG) == 0) &&
                     (strncmp(ptr, RESTART_FILE_ID, RESTART_FILE_ID_LENGTH) == 0))
                 {
                    int length = 0,
                        max_length = 0;

                    used |= RESTART_FILE_FLAG;
                    ptr += RESTART_FILE_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }

                    /*
                     * First determine the number of file names and the
                     * length of the longest file name, so we can allocate
                     * the necessary memory.
                     */
                    p_db->no_of_restart_files = 0;
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      if (*end_ptr == ' ')
                      {
                         *end_ptr = '\0';
                         p_db->no_of_restart_files++;
                         if (length > max_length)
                         {
                            max_length = length;
                         }
                      }
                      end_ptr++;
                      length++;
                    }
                    if (length > max_length)
                    {
                       max_length = length;
                    }
                    if (max_length > 0)
                    {
                       int  i;
                       char *tmp_ptr;

                       *end_ptr = '\0';
                       p_db->no_of_restart_files++;
                       max_length++;
                       RT_ARRAY(p_db->restart_file, p_db->no_of_restart_files,
                                max_length + 2, char);
                       /*
                        * NOTE: The + 2 is only for compatibility for
                        *       the older restart option which did
                        *       not store the file date.
                        */

                       for (i = 0; i < p_db->no_of_restart_files; i++)
                       {
                          (void)strcpy(p_db->restart_file[i], ptr);
                          tmp_ptr = p_db->restart_file[i];
                          while ((*tmp_ptr != '|') && (*tmp_ptr != '\0'))
                          {
                             tmp_ptr++;
                          }
                          if (*tmp_ptr == '|')
                          {
                             *tmp_ptr = '\0';
                          }
                          else
                          {
                             /*
                              * This is only for compatibility for
                              * the older restart option which did
                              * not store the file date.
                              */
                             *(tmp_ptr + 1) = '0';
                             *(tmp_ptr + 2) = '\0';
                          }
                          NEXT(ptr);
                       }
                    }
                    ptr = end_ptr;
                 }
            else if (((used & FILE_NAME_IS_HEADER_FLAG) == 0) &&
                     (strncmp(ptr, FILE_NAME_IS_HEADER_ID, FILE_NAME_IS_HEADER_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_HEADER_FLAG;
                    p_db->special_flag |= FILE_NAME_IS_HEADER;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & SUBJECT_FLAG) == 0) &&
                     (strncmp(ptr, SUBJECT_ID, SUBJECT_ID_LENGTH) == 0))
                 {
                    used |= SUBJECT_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       off_t length;

                       ptr += SUBJECT_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if (*ptr == '"')
                       {
                          char *ptr_start;

                          ptr++;
                          ptr_start = ptr;
                          while ((*ptr != '"') && (*ptr != '\n') &&
                                 (*ptr != '\0') && (isascii(*ptr)))
                          {
                             ptr++;
                          }
                          if (*ptr == '"')
                          {
                             length = ptr - ptr_start + 1;

                             if ((p_db->subject = malloc(length)) != NULL)
                             {
                                (void)memcpy(p_db->subject, ptr_start, length - 1);
                                p_db->subject[length - 1] = '\0';
                                p_db->special_flag |= MAIL_SUBJECT;
                             }
                             else
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "malloc() error : %s",
                                           strerror(errno));
                             }
                          }
                       }
                       else if (*ptr == '/')
                            {
                               char *file_name = ptr,
                                    tmp_char;

                               while ((*ptr != '\n') && (*ptr != '\0') &&
                                      (*ptr != ' ') && (*ptr != '\t'))
                               {
                                  if (*ptr == '\\')
                                  {
                                     ptr++;
                                  }
                                  ptr++;
                               }
                               tmp_char = *ptr;
                               *ptr = '\0';
                               if ((length = read_file(file_name,
                                                       &p_db->subject)) != INCORRECT)
                               {
                                  p_db->special_flag |= MAIL_SUBJECT;
                                  if (p_db->subject[length - 1] == '\n')
                                  {
                                     p_db->subject[length - 1] = '\0';
                                  }
                               }
                               *ptr = tmp_char;
                            }

                       /* Check if we need to insert some other info. */
                       if ((length > 0) && (p_db->subject != NULL))
                       {
                          int  new_length = length;
                          char *search_ptr = p_db->subject;

                          while (*search_ptr != '\0')
                          {
                             if (*search_ptr == '\\')
                             {
                                search_ptr++;
                             }
                             else
                             {
                                if (*search_ptr == '%')
                                {
                                   search_ptr++;
                                   if (*search_ptr == 't')
                                   {
                                      search_ptr++;
                                      switch (*search_ptr)
                                      {
                                         case 'a': /* short day of the week 'Tue' */
                                         case 'b': /* short month 'Jan' */
                                         case 'j': /* day of year [001,366] */
                                            new_length += 3;
                                            break;
                                         case 'd': /* day of month [01,31] */
                                         case 'M': /* minute [00,59] */
                                         case 'm': /* month [01,12] */
                                         case 'y': /* year 2 chars [01,99] */
                                         case 'H': /* hour [00,23] */
                                         case 'S': /* second [00,59] */
                                            new_length += 2;
                                            break;
                                         case 'Y': /* year 4 chars 1997 */
                                            new_length += 4;
                                            break;
                                         case 'A': /* long day of the week 'Tuesday' */
                                         case 'B': /* month 'January' */
                                         case 'U': /* Unix time. */
                                            new_length += 20;
                                            break;
                                      }
                                   }
                                }
                             }
                             search_ptr++;
                          }
                          if (new_length > length)
                          {
                             char *tmp_buffer;

                             if ((tmp_buffer = malloc(new_length)) == NULL)
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "malloc() error : %s",
                                           strerror(errno));
                             }
                             else
                             {
                                int    number;
                                time_t time_buf;
                                char   *read_ptr = p_db->subject,
                                       *write_ptr = tmp_buffer;

                                time_buf = time(NULL);
                                while (*read_ptr != '\0')
                                {
                                   if (*read_ptr == '\\')
                                   {
                                      read_ptr++;
                                      *write_ptr = *read_ptr;
                                      read_ptr++; write_ptr++;
                                   }
                                   else
                                   {
                                      if (*read_ptr == '%')
                                      {
                                         read_ptr++;
                                         if (*read_ptr == 't')
                                         {
                                            read_ptr++;
                                            switch (*read_ptr)
                                            {
                                               case 'a': /* short day of the week 'Tue' */
                                                  number = strftime(write_ptr, 4,
                                                                    "%a", localtime(&time_buf));
                                                  break;
                                               case 'b': /* short month 'Jan' */
                                                  number = strftime(write_ptr, 4,
                                                                    "%b", localtime(&time_buf));
                                                  break;
                                               case 'j': /* day of year [001,366] */
                                                  number = strftime(write_ptr, 4,
                                                                    "%j", localtime(&time_buf));
                                                  break;
                                               case 'd': /* day of month [01,31] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%d", localtime(&time_buf));
                                                  break;
                                               case 'M': /* minute [00,59] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%M", localtime(&time_buf));
                                                  break;
                                               case 'm': /* month [01,12] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%m", localtime(&time_buf));
                                                  break;
                                               case 'y': /* year 2 chars [01,99] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%y", localtime(&time_buf));
                                                  break;
                                               case 'H': /* hour [00,23] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%H", localtime(&time_buf));
                                                  break;
                                               case 'S': /* second [00,59] */
                                                  number = strftime(write_ptr, 3,
                                                                    "%S", localtime(&time_buf));
                                                  break;
                                               case 'Y': /* year 4 chars 1997 */
                                                  number = strftime(write_ptr, 5,
                                                                    "%Y", localtime(&time_buf));
                                                  break;
                                               case 'A': /* long day of the week 'Tuesday' */
                                                  number = strftime(write_ptr, 21,
                                                                    "%A", localtime(&time_buf));
                                                  break;
                                               case 'B': /* month 'January' */
                                                  number = strftime(write_ptr, 21,
                                                                    "%B", localtime(&time_buf));
                                                  break;
                                               case 'U': /* Unix time. */
                                                  number = sprintf(write_ptr, "%ld", time_buf);
                                                  break;
                                               default:
                                                  number = 0;
                                                  *write_ptr = '%';
                                                  *(write_ptr + 1) = 't';
                                                  *(write_ptr + 2) = *read_ptr;
                                                  write_ptr += 3;
                                                  break;
                                            }
                                            write_ptr += number;
                                            read_ptr++;
                                         }
                                         else
                                         {
                                            *write_ptr = '%';
                                            write_ptr++;
                                         }
                                      }
                                      else
                                      {
                                         *write_ptr = *read_ptr;
                                         read_ptr++; write_ptr++;
                                      }
                                   }
                                }
                                *write_ptr = '\0';
                                if ((p_db->subject = realloc(p_db->subject, new_length)) == NULL)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "realloc() error : %s",
                                              strerror(errno));
                                }
                                else
                                {
                                   (void)memcpy(p_db->subject, tmp_buffer, new_length);
                                }

                                free(tmp_buffer);
                             }
                          }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FORCE_COPY_FLAG) == 0) &&
                     (strncmp(ptr, FORCE_COPY_ID, FORCE_COPY_ID_LENGTH) == 0))
                 {
                    used |= FORCE_COPY_FLAG;
                    if (p_db->protocol & LOC_FLAG)
                    {
                       p_db->special_flag |= FORCE_COPY;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FILE_NAME_IS_SUBJECT_FLAG) == 0) &&
                     (strncmp(ptr, FILE_NAME_IS_SUBJECT_ID, FILE_NAME_IS_SUBJECT_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_SUBJECT_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       p_db->special_flag |= FILE_NAME_IS_SUBJECT;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & REPLY_TO_FLAG) == 0) &&
                     (strncmp(ptr, REPLY_TO_ID, REPLY_TO_ID_LENGTH) == 0))
                 {
                    used |= REPLY_TO_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       size_t length = 0;

                       ptr += REPLY_TO_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if ((p_db->reply_to = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore reply-to option : %s",
                                     strerror(errno));
                       }
                       else
                       {
                          (void)memcpy(p_db->reply_to, ptr, length);
                          p_db->reply_to[length] = '\0';
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FROM_FLAG) == 0) &&
                     (strncmp(ptr, FROM_ID, FROM_ID_LENGTH) == 0))
                 {
                    used |= FROM_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       size_t length = 0;

                       ptr += FROM_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if ((p_db->from = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore from option : %s",
                                     strerror(errno));
                       }
                       else
                       {
                          (void)memcpy(p_db->from, ptr, length);
                          p_db->from[length] = '\0';
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHECK_ANSI_FLAG) == 0) &&
                     (strncmp(ptr, ENCODE_ANSI_ID, ENCODE_ANSI_ID_LENGTH) == 0))
                 {
                    used |= CHECK_ANSI_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       p_db->special_flag |= ENCODE_ANSI;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_WMO_SUPPORT
            else if (((used & CHECK_REPLY_FLAG) == 0) &&
                     (strncmp(ptr, CHECK_REPLY_ID, CHECK_REPLY_ID_LENGTH) == 0))
                 {
                    used |= CHECK_REPLY_FLAG;
                    if (p_db->protocol & WMO_FLAG)
                    {
                       p_db->special_flag |= WMO_CHECK_ACKNOWLEDGE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & WITH_SEQUENCE_NUMBER_FLAG) == 0) &&
                     (strncmp(ptr, WITH_SEQUENCE_NUMBER_ID, WITH_SEQUENCE_NUMBER_ID_LENGTH) == 0))
                 {
                    used |= WITH_SEQUENCE_NUMBER_FLAG;
                    if (p_db->protocol & WMO_FLAG)
                    {
                       p_db->special_flag |= WITH_SEQUENCE_NUMBER;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _WITH_WMO_SUPPORT */
            else if (((used & ATTACH_FILE_FLAG) == 0) &&
                     (strncmp(ptr, ATTACH_FILE_ID, ATTACH_FILE_ID_LENGTH) == 0))
                 {
                    used |= ATTACH_FILE_FLAG;
                    ptr += ATTACH_FILE_ID_LENGTH;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       p_db->special_flag |= ATTACH_FILE;

                       while ((*ptr == ' ') && (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          int n = 0;

                          while ((*ptr != '\n') && (*ptr != '\0') &&
                                 (n < MAX_RULE_HEADER_LENGTH))
                          {
                            p_db->trans_rename_rule[n] = *ptr;
                            ptr++; n++;
                          }
                          p_db->trans_rename_rule[n] = '\0';
                          if (n == MAX_RULE_HEADER_LENGTH)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Rule header for trans_rename option %s to long. #%d",
                                        p_db->trans_rename_rule, p_db->job_id);
                          }
                          else if (n == 0)
                               {
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "No rule header specified in message %d.",
                                             p_db->job_id);
                               }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & ADD_MAIL_HEADER_FLAG) == 0) &&
                     (strncmp(ptr, ADD_MAIL_HEADER_ID, ADD_MAIL_HEADER_ID_LENGTH) == 0))
                 {
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       int length = 0;

                       used |= ADD_MAIL_HEADER_FLAG;
                       p_db->special_flag |= ADD_MAIL_HEADER;
                       ptr += ADD_MAIL_HEADER_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if (length > 0)
                       {
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if ((p_db->special_ptr = malloc(length + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() memory, will ignore mail header file : %s",
                                        strerror(errno));
                          }
                          else
                          {
                             (void)strcpy(p_db->special_ptr, ptr);
                          }
                          *end_ptr = byte_buf;
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FTP_EXEC_FLAG) == 0) &&
                     (strncmp(ptr, FTP_EXEC_CMD, FTP_EXEC_CMD_LENGTH) == 0))
                 {
                    used |= FTP_EXEC_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       int length = 0;

                       ptr += FTP_EXEC_CMD_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if (length > 0)
                       {
                          p_db->special_flag |= EXEC_FTP;
                          byte_buf = *end_ptr;
                          *end_ptr = '\0';
                          if ((p_db->special_ptr = malloc(length + 1)) == NULL)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to malloc() memory, will ignore %s option : %s",
                                        FTP_EXEC_CMD, strerror(errno));
                          }
                          else
                          {
                             (void)strcpy(p_db->special_ptr, ptr);
                          }
                          *end_ptr = byte_buf;
                       }
                       ptr = end_ptr;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHARSET_FLAG) == 0) &&
                     (strncmp(ptr, CHARSET_ID, CHARSET_ID_LENGTH) == 0))
                 {
                    used |= CHARSET_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       size_t length = 0;

                       ptr += CHARSET_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                         length++;
                       }
                       if ((p_db->charset = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore charset option : %s",
                                     strerror(errno));
                       }
                       else
                       {
                          (void)memcpy(p_db->charset, ptr, length);
                          p_db->charset[length] = '\0';
                       }
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & FILE_NAME_IS_USER_FLAG) == 0) &&
                     (strncmp(ptr, FILE_NAME_IS_USER_ID, FILE_NAME_IS_USER_ID_LENGTH) == 0))
                 {
                    used |= FILE_NAME_IS_USER_FLAG;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       p_db->special_flag |= FILE_NAME_IS_USER;
                       ptr += FILE_NAME_IS_USER_ID_LENGTH;
                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       end_ptr = ptr;
                       while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                       {
                         end_ptr++;
                       }
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       (void)strcpy(p_db->user_rename_rule, ptr);
                       *end_ptr = byte_buf;
                       ptr = end_ptr;
                    }
                    else
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_EUMETSAT_HEADERS
            else if (((used & EUMETSAT_HEADER_FLAG) == 0) &&
                     (strncmp(ptr, EUMETSAT_HEADER_ID, EUMETSAT_HEADER_ID_LENGTH) == 0))
                 {
                    int length;

                    used |= EUMETSAT_HEADER_FLAG;
                    ptr += EUMETSAT_HEADER_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if ((p_db->special_ptr = malloc(4 + 1)) == NULL)
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to malloc() memory, will ignore option %s : %s",
                                  EUMETSAT_HEADER_ID, strerror(errno));
                    }
                    else
                    {
                       char str_num[5];

                       while ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       length = 0;
                       while ((*ptr != ' ') && (*ptr != '\t') &&
                              (length < 4) && (*ptr != '\0') &&
                              (*ptr != '\n'))
                       {
                         str_num[length] = *ptr;
                         ptr++; length++;
                       }
                       if ((length == 0) || (length == 4) ||
                           (*ptr == '\0'))
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Missing/incorrect DestEnvId. Ignoring option %s.",
                                     EUMETSAT_HEADER_ID);
                          free(p_db->special_ptr);
                          p_db->special_ptr = NULL;
                       }
                       else
                       {
                          int number;

                          str_num[length] = '\0';
                          number = atoi(str_num);
                          if (number > 255)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "DestEnvId to large (%d). Ignoring option %s.",
                                        number, EUMETSAT_HEADER_ID);
                             free(p_db->special_ptr);
                             p_db->special_ptr = NULL;
                          }
                          else
                          {
                             char local_host[256];

                             /*
                              * CPU ID is IP number of current host.
                              */
                             if (gethostname(local_host, 255) == -1)
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Failed to gethostname() : %s",
                                           strerror(errno));
                             }
                             else
                             {
                                register struct hostent *p_host = NULL;

                                if ((p_host = gethostbyname(local_host)) == NULL)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to gethostbyname() of local host : %s",
                                              strerror(errno));
                                }
                                else
                                {
                                   unsigned char *ip_ptr = (unsigned char *)p_host->h_addr;

                                   p_db->special_ptr[0] = *ip_ptr;
                                   p_db->special_ptr[1] = *(ip_ptr + 1);
                                   p_db->special_ptr[2] = *(ip_ptr + 2);
                                   p_db->special_ptr[3] = *(ip_ptr + 3);

                                   p_db->special_ptr[4] = (unsigned char)number;
                                   p_db->special_flag |= ADD_EUMETSAT_HEADER;
                                }
                             }
                          }
                       }
                    }
                    while (*ptr != '\n')
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _WITH_EUMETSAT_HEADERS */
#ifdef _RADAR_CHECK
            else if (((used & RADAR_FLAG) == 0) &&
                     (strncmp(ptr, RADAR_CHECK_ID, RADAR_CHECK_ID_LENGTH) == 0))
                 {
                    used |= RADAR_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       p_db->special_flag |= RADAR_CHECK_FLAG;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _RADAR_CHECK */
            else if (((used & RENAME_FILE_BUSY_FLAG) == 0) &&
                     (strncmp(ptr, RENAME_FILE_BUSY_ID, RENAME_FILE_BUSY_ID_LENGTH) == 0))
                 {
                    used |= RENAME_FILE_BUSY_FLAG;
                    ptr += RENAME_FILE_BUSY_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (isascii(*ptr))
                    {
                      p_db->rename_file_busy = *ptr;
                      ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHANGE_FTP_MODE_FLAG) == 0) &&
                     (strncmp(ptr, PASSIVE_FTP_MODE, PASSIVE_FTP_MODE_LENGTH) == 0))
                 {
                    used |= CHANGE_FTP_MODE_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       p_db->mode_flag = PASSIVE_MODE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & CHANGE_FTP_MODE_FLAG) == 0) &&
                     (strncmp(ptr, ACTIVE_FTP_MODE, ACTIVE_FTP_MODE_LENGTH) == 0))
                 {
                    used |= CHANGE_FTP_MODE_FLAG;
                    if (p_db->protocol & FTP_FLAG)
                    {
                       p_db->mode_flag = ACTIVE_MODE;
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
            else if (((used & ATTACH_ALL_FILES_FLAG) == 0) &&
                     (strncmp(ptr, ATTACH_ALL_FILES_ID, ATTACH_ALL_FILES_ID_LENGTH) == 0))
                 {
                    used |= ATTACH_ALL_FILES_FLAG;
                    used |= ATTACH_FILE_FLAG;
                    ptr += ATTACH_ALL_FILES_ID_LENGTH;
                    if (p_db->protocol & SMTP_FLAG)
                    {
                       p_db->special_flag |= ATTACH_FILE;
                       p_db->special_flag |= ATTACH_ALL_FILES;

                       while ((*ptr == ' ') && (*ptr == '\t'))
                       {
                          ptr++;
                       }
                       if ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          int n = 0;

                          while ((*ptr != '\n') && (*ptr != '\0') &&
                                 (n < MAX_RULE_HEADER_LENGTH))
                          {
                            p_db->trans_rename_rule[n] = *ptr;
                            ptr++; n++;
                          }
                          p_db->trans_rename_rule[n] = '\0';
                          if (n == MAX_RULE_HEADER_LENGTH)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Rule header for trans_rename option %s to long. #%d",
                                        p_db->trans_rename_rule, p_db->job_id);
                          }
                          else if (n == 0)
                               {
                                  system_log(WARN_SIGN, __FILE__, __LINE__,
                                             "No rule header specified in message %d.",
                                             p_db->job_id);
                               }
                       }
                    }
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#ifdef _WITH_TRANS_EXEC
            else if (((used & TRANS_EXEC_FLAG) == 0) &&
                     (strncmp(ptr, TRANS_EXEC_ID, TRANS_EXEC_ID_LENGTH) == 0))
                 {
                    int length = 0;
                    used |= TRANS_EXEC_FLAG;

                    ptr += TRANS_EXEC_ID_LENGTH;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                      end_ptr++;
                      length++;
                    }
                    if (length > 0)
                    {
                       p_db->special_flag |= TRANS_EXEC;
                       byte_buf = *end_ptr;
                       *end_ptr = '\0';
                       if ((p_db->trans_exec_cmd = malloc(length + 1)) == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to malloc() memory, will ignore %s option : %s",
                                     TRANS_EXEC_ID, strerror(errno));
                       }
                       else
                       {
                          (void)strcpy(p_db->trans_exec_cmd, ptr);
                       }
                       *end_ptr = byte_buf;
                    }
                    ptr = end_ptr;
                    while ((*ptr != '\n') && (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif /* _WITH_TRANS_EXEC */
                 else
                 {
                    /* Ignore this option */
                    end_ptr = ptr;
                    while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                    {
                       end_ptr++;
                    }
                    byte_buf = *end_ptr;
                    *end_ptr = '\0';
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               "Unknown or duplicate option <%s> in message %s",
                               ptr, message_name);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
         } /* while (*ptr != '\0') */
      }
   } /* if (byte_buf != '\0') */

   /* Don't forget to free memory we have allocated */
   free(msg_buf);

   return(SUCCESS);
}
