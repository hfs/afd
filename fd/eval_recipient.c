/*
 *  eval_recipient.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Deutscher Wetterdienst (DWD),
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
 **                      char       *full_msg_path)
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
 **
 */
DESCR__E_M3

#include <stdio.h>                   /* NULL                             */
#include <string.h>                  /* strcpy()                         */
#include <stdlib.h>                  /* atoi()                           */
#include <time.h>                    /* time(), strftime()               */
#include "fddefs.h"

/* Global variables */
extern int                        no_of_hosts;
extern struct filetransfer_status *fsa;


/*########################## eval_recipient() ###########################*/
int
eval_recipient(char *recipient, struct job *p_db, char *full_msg_path)
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
   if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
   {
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
         if (i == MAX_HOSTNAME_LENGTH)
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
         get_group_list(p_db->user);

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
            ptr++; i++;
         }
         if (*ptr == '\0')
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Hmm. This does NOT look like URL for me!?");
            return(INCORRECT);
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Unable to store user name. It is longer than %d Bytes!",
                       MAX_USER_NAME_LENGTH);
            return(INCORRECT);
         }
         p_db->user[i] = '\0';
         if (*ptr == ':')
         {
            ptr++;

            /* Get password */
            i = 0;
            while ((*ptr != '@') && (*ptr != '\0') && (i < MAX_USER_NAME_LENGTH))
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
            if (i == MAX_USER_NAME_LENGTH)
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

      /* Save TCP port number */
      if (*ptr == ':')
      {
         ptr++;
         ptr_tmp = ptr;
         while ((*ptr != '/') && (*ptr != '\0') && (*ptr != ';'))
         {
            ptr++;
         }
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

      /* Save the destination directory. */
      if (*ptr == '/')
      {
         ptr++;
         i = 0;
         while ((*ptr != '\0') && (*ptr != ';'))
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

                  time_buf = time(NULL);
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
                        number = sprintf(&p_db->target_dir[i], "%ld", time_buf);
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
               else
               {
                  p_db->target_dir[i] = *ptr;
                  i++; ptr++;
               }
            }
         }
         p_db->target_dir[i] = '\0';
      }
      else
      {
         p_db->target_dir[0] = '\0';
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
                    p_db->smtp_server[i] = '\0';
                 }
         }
         /*
          * Ignore anything behind the ftp type.
          */
      }

      /*
       * Find position of this hostname in FSA.
       */
      if (p_db->smtp_server[0] == '\0')
      {
         t_hostname(p_db->hostname, p_db->host_alias);
      }
      else
      {
         t_hostname(p_db->smtp_server, p_db->host_alias);
      }
      if ((p_db->fsa_pos = get_host_position(fsa, p_db->host_alias, no_of_hosts)) < 0)
      {
         /*
          * Uups. What do we do now? Host not in the FSA!
          * Lets put this message into the faulty directory.
          * Hmm. Maybe somebody has a better idea?
          */
         if (full_msg_path != NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "The message in %s contains a hostname (%s) that is not in the FSA.",
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
                         fsa[p_db->fsa_pos].real_hostname[(int)(fsa[p_db->fsa_pos].host_toggle - 1)]);
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
