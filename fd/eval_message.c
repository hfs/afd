/*
 *  eval_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2001 Deutscher Wetterdienst (DWD),
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
 **
 */
DESCR__E_M3

#include <stdio.h>                /* stderr, fprintf()                   */
#include <stdlib.h>               /* atoi(), malloc(), free(), strtoul() */
#include <string.h>               /* strcmp(), strncmp(), strerror()     */
#include <ctype.h>                /* isascii()                           */
#include <sys/types.h>
#include <sys/stat.h>             /* fstat()                             */
#ifndef _NO_MMAP
#include <sys/mman.h>             /* mmap(), munmap()                    */
#endif
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
extern int sys_log_fd,
           transfer_log_fd;

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
#define FILE_NAME_IS_USER_FLAG    4096
#define CHECK_ANSI_FLAG           8192
#define CHECK_REPLY_FLAG          16384
#define WITH_SEQUENCE_NUMBER_FLAG 32768
#define ATTACH_FILE_FLAG          131072
#define ADD_MAIL_HEADER_FLAG      262144
#define FTP_EXEC_FLAG             524288
#ifdef _WITH_EUMETSAT_HEADERS
#define EUMETSAT_HEADER_FLAG      1048576
#endif /* _WITH_EUMETSAT_HEADERS */
#ifdef _RADAR_CHECK
#define RADAR_FLAG                2097152
#endif /* _RADAR_CHECK */
#define CHANGE_FTP_MODE_FLAG      4194304
#define ATTACH_ALL_FILES_FLAG     8388608
#ifdef _WITH_TRANS_EXEC
#define TRANS_EXEC_FLAG           16777216
#endif /* _WITH_TRANS_EXEC */


/*############################ eval_message() ###########################*/
int
eval_message(char *message_name, struct job *p_db)
{
   int         fd,
               n,
               used = 0;          /* Used to see whether option has      */
                                  /* already been set.                   */
   char        *ptr = NULL,
               *end_ptr = NULL,
               *msg_buf = NULL,
               byte_buf;
   struct stat stat_buf;

   /* Store message to buffer */
   if ((fd = open(message_name, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to open() %s : %s", message_name, strerror(errno));
      exit(NO_MESSAGE_FILE);
   }

   /* Stat() message to get size of it */
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not fstat() %s : %s", message_name, strerror(errno));
      exit(INCORRECT);
   }

   /* Allocate memory to store message */
   if ((msg_buf = malloc(stat_buf.st_size + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not malloc() memory to read message : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

#ifdef _NO_MMAP
   /* Read message in one hunk. */
   if (read(fd, msg_buf, stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() %s : %s", message_name, strerror(errno));
      exit(INCORRECT);
   }
#else
   if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fd, 0)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to mmap() to %s : %s", message_name, strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy(msg_buf, ptr, stat_buf.st_size);
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to munmap() %s : %s", message_name, strerror(errno));
      exit(INCORRECT);
   }
#endif
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   msg_buf[stat_buf.st_size] = '\0';

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

   if (eval_recipient(ptr, p_db, message_name) < 0)
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
                    p_db->age_limit = atoi(ptr);
                    *end_ptr = byte_buf;
                    ptr = end_ptr;
                    while (*ptr == '\n')
                    {
                       ptr++;
                    }
                 }
#endif
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
                    if (strcmp(ptr, LOCK_DOT) == 0)
                    {
                       p_db->lock = DOT;
                    }
                    else if (strcmp(ptr, LOCK_DOT_VMS) == 0)
                         {
                            p_db->lock = DOT_VMS;
                         }
                    else if (strcmp(ptr, LOCK_FILE) == 0)
                         {
                            p_db->lock = LOCKFILE;
                         }
                    else if  (strcmp(ptr, LOCK_OFF) == 0)
                         {
                            p_db->lock = OFF;
                         }
#ifdef _WITH_READY_FILES
                    else if  (strcmp(ptr, LOCK_READY_A_FILE) == 0)
                         {
                            p_db->lock = READY_A_FILE;
                         }
                    else if  (strcmp(ptr, LOCK_READY_B_FILE) == 0)
                         {
                            p_db->lock = READY_B_FILE;
                         }
#endif /* _WITH_READY_FILES */
                         else
                         {
                            p_db->lock = DOT;
                            (void)strcpy(p_db->lock_notation, ptr);
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
                       int i;

                       *end_ptr = '\0';
                       p_db->no_of_restart_files++;
                       max_length++;
                       RT_ARRAY(p_db->restart_file, p_db->no_of_restart_files,
                                max_length, char);

                       for (i = 0; i < p_db->no_of_restart_files; i++)
                       {
                          (void)strcpy(p_db->restart_file[i], ptr);
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
                             int  length = ptr - ptr_start + 1;

                             if ((p_db->subject = malloc(length)) != NULL)
                             {
                                (void)memcpy(p_db->subject, ptr_start, length - 1);
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
                               size_t length;
                               char   *file_name = ptr,
                                      tmp_char;

                               while ((*ptr != '\n') && (*ptr != '\0') &&
                                      (*ptr != ' ') && (*ptr != '\t'))
                               {
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
