/*
 *  eval_recipient.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2005 Deutscher Wetterdienst (DWD),
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
 **   eval_recipient - reads the recipient string
 **
 ** SYNOPSIS
 **   int eval_recipient(char       *recipient,
 **                      struct job *p_db,
 **                      char       *full_msg_path,
 **                      time_t     next_check_time)
 **
 ** DESCRIPTION
 **   Here we evaluate the recipient string which must have the
 **   URL (Uniform Resource Locators) format:
 **
 **    <sheme>://<user>:<password>@<host>:<port>/<url-path>[;type=i|a|d]
 **                                                        [;server=<server-name>]
 **
 ** RETURN VALUES
 **   struct job *p_db   - The structure in which we store the
 **                        following values:
 **
 **                             - user
 **                             - password
 **                             - hostname
 **                             - port
 **                             - directory
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.11.1995 H.Kiehl Created
 **   15.12.1996 H.Kiehl Changed to URL format.
 **   26.03.1998 H.Kiehl Remove host switching information, this is now
 **                      completely handled by the AMG.
 **   12.04.1998 H.Kiehl Added DOS binary mode.
 **   03.06.1998 H.Kiehl Allow user to add special characters (@, :, etc)
 **                      in password.
 **   15.12.2001 H.Kiehl When server= is set take this as hostname.
 **   19.02.2002 H.Kiehl Added the ability to read recipient group file.
 **   21.03.2003 H.Kiehl Support to only map to the required part of FSA.
 **   09.04.2004 H.Kiehl Support for TLS/SSL.
 **   28.01.2005 H.Kiehl Don't keep the error time indefinite long, for
 **                      retrieving remote files.
 **   21.11.2005 H.Kiehl Added time modifier when evaluating the directory.
 **
 */
DESCR__E_M3

#include <stdio.h>                   /* NULL                             */
#include <string.h>                  /* strcpy()                         */
#include <stdlib.h>                  /* atoi()                           */
#include <time.h>                    /* time(), strftime()               */
#include "fddefs.h"

/* Global variables */
extern struct filetransfer_status *fsa;


/*########################## eval_recipient() ###########################*/
int
eval_recipient(char       *recipient,
               struct job *p_db,
               char       *full_msg_path,
               time_t     next_check_time)
{
   char *ptr,
        *ptr_tmp;

   ptr = recipient;
   while ((*ptr != ':') && (*ptr != '\0'))
   {
      /* The scheme is not necessary since we know the */
      /* protocol from the program name.               */
      ptr++;
   }
#ifdef WITH_SSL
   if (*(ptr - 1) == 's')
   {
      p_db->auth = YES;
   }
   else if (recipient[3] == 'S')
        {
           p_db->auth = BOTH;
        }
#endif
   if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
   {
      time_t       time_modifier = 0;
      char         time_mod_sign = '+';
      register int i;

      ptr += 3; /* Away with '://' */

      if (*ptr == MAIL_GROUP_IDENTIFIER)
      {
         ptr++;
         i = 0;
         while ((*ptr != '@') && (*ptr != ';') && (*ptr != ':') &&
                (*ptr != '\0') && (i < MAX_HOSTNAME_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            p_db->user[i] = *ptr;
            ptr++; i++;
         }
         if (i > MAX_HOSTNAME_LENGTH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to store group name. It may only be %d characters long!",
                       MAX_HOSTNAME_LENGTH);
            return(INCORRECT);
         }
         if (i == 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "No group name specified.");
            return(INCORRECT);
         }
         get_group_list();

         if (*ptr == '@')
         {
            ptr++; /* Away with the @. */

            /* Now lets get the host alias name. */
            i = 0;
            while ((*ptr != '\0') && (*ptr != '/') &&
                   (*ptr != ':') && (*ptr != ';'))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               p_db->hostname[i] = *ptr;
               i++; ptr++;
            }
            p_db->hostname[i] = '\0';
         }
         else
         {
            (void)strcpy(p_db->hostname, p_db->user);
         }
      }
      else
      {
#ifndef WITH_PASSWD_IN_MSG
         int  uh_name_length;
         char uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];
#endif

         p_db->group_list = NULL;

         /* Get user name */
         if (*ptr == '\0')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Just telling me the sheme and nothing else is not of much use!");
            return(INCORRECT);
         }
         i = 0;
         while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0') &&
                (i < MAX_USER_NAME_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            p_db->user[i] = *ptr;
#ifndef WITH_PASSWD_IN_MSG
            uh_name[i] = *ptr;
#endif
            ptr++; i++;
         }
         if (*ptr == '\0')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmm. This does NOT look like URL for me!?");
            return(INCORRECT);
         }
         if (i > MAX_USER_NAME_LENGTH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to store user name. It is longer than %d Bytes!",
                       MAX_USER_NAME_LENGTH);
            return(INCORRECT);
         }
         p_db->user[i] = '\0';
#ifdef WITH_PASSWD_IN_MSG
         if (*ptr == ':')
         {
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
               p_db->password[i] = *ptr;
               ptr++; i++;
            }
            if ((i == 0) && (*ptr != '@'))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Hmmm. How am I suppose to find the hostname?");
               return(INCORRECT);
            }
            if (i > MAX_USER_NAME_LENGTH)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to store password. It is longer than %d Bytes!",
                          MAX_USER_NAME_LENGTH);
               return(INCORRECT);
            }
            p_db->password[i] = '\0';
            ptr++;
         }
         else if (*ptr == '@')
              {
                 ptr++;

                 /* No password given. This could be a mail or  */
                 /* we want to do anonymous ftp. Since we       */
                 /* currently do not know what protocol we have */
                 /* here lets set the password to 'anonymous'.  */
                 (void)strcpy(p_db->password, "anonymous");
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Hmmm. How am I suppose to find the hostname?");
                 return(INCORRECT);
              }
#else
         uh_name_length = i;
         if (*ptr != '@')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmmm. How am I suppose to find the hostname?");
            return(INCORRECT);
         }
         ptr++;
#endif

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
            p_db->hostname[i] = *ptr;
#ifndef WITH_PASSWD_IN_MSG
            uh_name[uh_name_length + i] = *ptr;
#endif
            i++; ptr++;
         }
         p_db->hostname[i] = '\0';

#ifndef WITH_PASSWD_IN_MSG
         uh_name[uh_name_length + i] = '\0';
         if ((p_db->protocol & SMTP_FLAG) || (p_db->protocol & LOC_FLAG) ||
# ifdef _WITH_MAP_SUPPORT
             (p_db->protocol & MAP_FLAG) ||
# endif
             (p_db->protocol & WMO_FLAG))
         {
            /* No need to lookup a passwd. */;
         }
         else
         {
            if (get_pw(uh_name, p_db->password) == INCORRECT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to get password.");
               return(INCORRECT);
            }
         }
#endif
         if (i > MAX_REAL_HOSTNAME_LENGTH)
         {
            while ((*ptr != '\0') && (*ptr != '/') &&
                   (*ptr != ':') && (*ptr != ';'))
            {
               ptr++;
            }
         }
      }

      /* Save TCP port number. */
      if (*ptr == ':')
      {
         ptr++;
         ptr_tmp = ptr;
         while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';'))
         {
            ptr++;
         }
         if (ptr != ptr_tmp) /* Ensure that port number is there. */
         {
            if (*ptr == '/')
            {
               *ptr = '\0';
               p_db->port = atoi(ptr_tmp);
               *ptr = '/';
            }
            else if (*ptr == '\0')
                 {
                    p_db->port = atoi(ptr_tmp);
                 }
         }
      }

      /* Save the destination directory. */
      if (*ptr == '/')
      {
         ptr++;
         i = 0;
         while ((*ptr != '\0') && (*ptr != ';') && (i < MAX_RECIPIENT_LENGTH))
         {
            if (*ptr == '\\')
            {
               ptr++;
               p_db->target_dir[i] = *ptr;
               i++; ptr++;
            }
            else
            {
               if ((*ptr == '%') && (*(ptr + 1) == 't'))
               {
                  int    number;
                  time_t time_buf;

                  if ((next_check_time > 0) && (fsa->error_counter > 0) &&
                      (fsa->error_counter < fsa->max_errors))
                  {
                     time_buf = next_check_time;
                  }
                  else
                  {
                     time_buf = time(NULL);
                  }
                  if (time_modifier > 0)
                  {
                     switch (time_mod_sign)
                     {
                        case '-' :
                           time_buf = time_buf - time_modifier;
                           break;
                        case '*' :
                           time_buf = time_buf * time_modifier;
                           break;
                        case '/' :
                           time_buf = time_buf / time_modifier;
                           break;
                        case '%' :
                           time_buf = time_buf % time_modifier;
                           break;
                        case '+' :
                        default :
                           time_buf = time_buf + time_modifier;
                           break;
                     }
                  }
                  switch(*(ptr + 2))
                  {
                     case 'a': /* short day of the week 'Tue' */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%a", localtime(&time_buf));
                        break;
                     case 'b': /* short month 'Jan' */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%b", localtime(&time_buf));
                        break;
                     case 'j': /* day of year [001,366] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%j", localtime(&time_buf));
                        break;
                     case 'd': /* day of month [01,31] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%d", localtime(&time_buf));
                        break;
                     case 'M': /* minute [00,59] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%M", localtime(&time_buf));
                        break;
                     case 'm': /* month [01,12] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%m", localtime(&time_buf));
                        break;
                     case 'y': /* year 2 chars [01,99] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%y", localtime(&time_buf));
                        break;
                     case 'H': /* hour [00,23] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%H", localtime(&time_buf));
                        break;
                     case 'S': /* second [00,59] */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%S", localtime(&time_buf));
                        break;
                     case 'Y': /* year 4 chars 2002 */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%Y", localtime(&time_buf));
                        break;
                     case 'A': /* long day of the week 'Tuesday' */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%A", localtime(&time_buf));
                        break;
                     case 'B': /* month 'January' */
                        number = strftime(&p_db->target_dir[i],
                                          MAX_PATH_LENGTH - i,
                                          "%B", localtime(&time_buf));
                        break;
                     case 'U': /* Unix time. */
#if SIZEOF_TIME_T == 4
                        number = sprintf(&p_db->target_dir[i], "%ld", time_buf);
#else
                        number = sprintf(&p_db->target_dir[i], "%lld", time_buf);
#endif
                        break;
                     default :
                        number = 3;
                        p_db->target_dir[i] = '%';
                        p_db->target_dir[i + 1] = 't';
                        p_db->target_dir[i + 2] = *(ptr + 2);
                        break;
                  }
                  i += number;
                  ptr += 3;
               }
               else if ((*ptr == '%') && (*(ptr + 1) == 'T'))
                    {
                       int  j,
                            time_unit;
                       char string[MAX_INT_LENGTH + 1];

                       ptr += 2;
                       switch (*ptr)
                       {
                          case '+' :
                          case '-' :
                          case '*' :
                          case '/' :
                          case '%' :
                             time_mod_sign = *ptr;
                             ptr++;
                             break;
                          default  :
                             time_mod_sign = '+';
                             break;
                       }
                       j = 0;
                       while ((isdigit(*ptr)) && (j < MAX_INT_LENGTH))
                       {
                          string[j++] = *ptr++;
                       }
                       if ((j > 0) && (j < MAX_INT_LENGTH))
                       {
                          string[j] = '\0';
                          time_modifier = atoi(string);
                       }
                       else
                       {
                          if (i == MAX_INT_LENGTH)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "The time modifier specified in the recipient of message %s for host %s is to large.",
                                        full_msg_path, p_db->host_alias);
                          }
                          else
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "There is no time modifier specified in the recipient of message %s for host %s.",
                                        full_msg_path, p_db->host_alias);
                          }
                          time_modifier = 0;
                       }
                       switch (*ptr)
                       {
                          case 'S' : /* Second */
                             time_unit = 1;
                             ptr++;
                             break;
                          case 'M' : /* Minute */
                             time_unit = 60;
                             ptr++;
                             break;
                          case 'H' : /* Hour */
                             time_unit = 3600;
                             ptr++;
                             break;
                          case 'd' : /* Day */
                             time_unit = 86400;
                             ptr++;
                             break;
                          default :
                             time_unit = 1;
                             break;
                       }
                       if (time_modifier > 0)
                       {
                          time_modifier = time_modifier * time_unit;
                       }
                    }
                    else
                    {
                       p_db->target_dir[i] = *ptr;
                       i++; ptr++;
                    }
            }
         }
         if (i >= MAX_RECIPIENT_LENGTH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Recipient longer then the allowed length of %d bytes.",
                       MAX_RECIPIENT_LENGTH);
            return(INCORRECT);
         }
         if ((i > 0) && (p_db->target_dir[i - 1] != '/') &&
             (fsa->protocol & HTTP_FLAG))
         {
            p_db->target_dir[i] = '/';
            i++;
         }
         p_db->target_dir[i] = '\0';
      }
      else
      {
         if (fsa->protocol & HTTP_FLAG)
         {
            p_db->target_dir[0] = '/';
            p_db->target_dir[1] = '\0';
         }
         else
         {
            p_db->target_dir[0] = '\0';
         }
      }

      /* Save the type code (FTP) or the server name (SMTP) */
      if (*ptr == ';')
      {
         int count = 0;

         ptr++;
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
#ifdef WITH_SSL
         if ((*ptr == 'e') && (*(ptr + 1) == 'n') && (*(ptr + 2) == 'c') &&
             (*(ptr + 3) == 'r') && (*(ptr + 4) == 'y') &&
             (*(ptr + 5) == 'p') && (*(ptr + 6) == 't') &&
             (*(ptr + 7) == 'd') && (*(ptr + 8) == 'a') &&
             (*(ptr + 9) == 't') && (*(ptr + 10) == 'a'))
         {
            p_db->auth = BOTH;
            ptr += 11;
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if (*ptr == ';')
            {
               ptr++;
            }
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
         }
#endif
         ptr_tmp = ptr;
         while ((*ptr != '\0') && (*ptr != '='))
         {
            ptr++; count++;
         }
         if (*ptr == '=')
         {
            if (count == 4)
            {
               if ((*(ptr - 1) != 'e') || (*(ptr - 2) != 'p') ||
                   (*(ptr - 3) != 'y') || (*(ptr - 4) != 't'))
               {
                  *ptr = '\0';
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Actually I was expecting <type=> and not <%s=>",
                             ptr_tmp);
                  *ptr = '=';
                  return(INCORRECT);
               }
               ptr++;
               switch (*ptr)
               {
                  case 'a': /* ASCII */
                  case 'A': p_db->transfer_mode = 'A';
                            break;
                  case 'd': /* DOS binary */
                  case 'D': p_db->transfer_mode = 'D';
                            break;
                  case 'i': /* Image/binary */
                  case 'I': p_db->transfer_mode = 'I';
                            break;
                  default : system_log(ERROR_SIGN, __FILE__, __LINE__,
                                       "Unknown ftp type (%d). Changing to I.",
                                       *ptr);
                            p_db->transfer_mode = 'I';
                            break;
               }
            }
            else if (count == 6)
                 {
                    if ((*(ptr - 1) != 'r') || (*(ptr - 2) != 'e') ||
                        (*(ptr - 3) != 'v') || (*(ptr - 4) != 'r') ||
                        (*(ptr - 5) != 'e') || (*(ptr - 6) != 's'))
                    {
                       *ptr = '\0';
                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Actually I was expecting <server=> and not <%s=>",
                                  ptr_tmp);
                       *ptr = '=';
                       return(INCORRECT);
                    }
                    ptr++;
                    i = 0;
                    while ((*ptr != '\0') && (*ptr != ' ') && (*ptr != '\t'))
                    {
                       p_db->smtp_server[i] = *ptr;
                       i++; ptr++;
                    }
                    if (i != 0)
                    {
                       p_db->smtp_server[i] = '\0';
                    }
                 }
         }
         /*
          * Ignore anything behind the ftp type.
          */
      }

      /*
       * Find position of this hostname in FSA.
       */
      if ((p_db->smtp_server[0] == '\0') ||
          (p_db->special_flag & SMTP_SERVER_NAME_IN_AFD_CONFIG))
      {
         t_hostname(p_db->hostname, p_db->host_alias);
      }
      else
      {
         t_hostname(p_db->smtp_server, p_db->host_alias);
      }
      if (CHECK_STRCMP(p_db->host_alias, fsa->host_alias) == 0)
      {
         if ((p_db->smtp_server[0] != '\0') &&
             ( (p_db->special_flag & SMTP_SERVER_NAME_IN_AFD_CONFIG) == 0))
         {
            (void)strcpy(p_db->smtp_server,
                         fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
         }
      }
      else
      {
         int ret;

         /*
          * Uups. What do we do now? Host not in the FSA!
          * Hmm. Maybe somebody has a better idea?
          */
         if (((ret = gsf_check_fsa()) == NO) || (ret == NEITHER) ||
             ((ret == YES) && (p_db->fsa_pos == INCORRECT)) ||
             ((ret == YES) && (p_db->fsa_pos != INCORRECT) &&
              (CHECK_STRCMP(p_db->host_alias, fsa->host_alias))))
         {
            if (full_msg_path != NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "The message %s contains a hostname (%s) that is not in the FSA.",
                          full_msg_path, p_db->host_alias);
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to locate host %s in the FSA.",
                          p_db->host_alias);
            }
            return(INCORRECT);
         }
         else
         {
            if (p_db->smtp_server[0] != '\0')
            {
               (void)strcpy(p_db->smtp_server,
                            fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
            }
         }
      }
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Hmm. This does NOT look like URL for me!?");
      return(INCORRECT);
   }

   return(SUCCESS);
}
