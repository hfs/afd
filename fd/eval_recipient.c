/*
 *  eval_recipient.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
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
 **
 */
DESCR__E_M3

#include <stdio.h>                   /* NULL                             */
#include <string.h>                  /* strcpy()                         */
#include <stdlib.h>                  /* atoi()                           */
#include "fddefs.h"

/* Global variables */
extern int                        no_of_hosts,
                                  sys_log_fd;
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
   if (*ptr == ':')
   {
      register int i;

      /* Get user name */
      if ((*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
      {
         ptr += 3; /* Away with '://' */
         if (*ptr == '\0')
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Just telling me the sheme and nothing else is not of much use! (%s %d)\n",
                      __FILE__, __LINE__);
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
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Hmm. This does NOT look like URL for me!? (%s %d)\n",
                      __FILE__, __LINE__);
            return(INCORRECT);
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Unable to store user name. It is longer than %d Bytes! (%s %d)\n",
                      MAX_USER_NAME_LENGTH, __FILE__, __LINE__);
            return(INCORRECT);
         }
         p_db->user[i] = '\0';
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "This definitely is GARBAGE! Get a new AFD administrator!!! (%s %d)\n",
                   __FILE__, __LINE__);
         return(INCORRECT);
      }
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
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Hmmm. How am I suppose to find the hostname? (%s %d)\n",
                      __FILE__, __LINE__);
            return(INCORRECT);
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Unable to store password. It is longer than %d Bytes! (%s %d)\n",
                      MAX_USER_NAME_LENGTH, __FILE__, __LINE__);
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
              (void)rec(sys_log_fd, ERROR_SIGN,
                        "Hmmm. How am I suppose to find the hostname? (%s %d)\n",
                        __FILE__, __LINE__);
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

      /*
       * Find position of this hostname in FSA.
       */
      t_hostname(p_db->hostname, p_db->host_alias);
      if (((p_db->position = get_position(fsa, p_db->host_alias, no_of_hosts)) < 0) &&
          (full_msg_path != NULL))
      {
         /*
          * Uups. What do we do now? Host not in the FSA!
          * Lets put this message into the faulty directory.
          * Hmm. Maybe somebody has a better idea?
          */
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "The message in %s contains a hostname (%s) that is not in the FSA. (%s %d)\n",
                   full_msg_path, p_db->host_alias, __FILE__, __LINE__);
         return(INCORRECT);
      }

      /*
       * If proxy name is specified, add this to the user
       * name. We do this here because this is the point
       * where we know the position in the FSA.
       */
      if (fsa[p_db->position].proxy_name[0] != '\0')
      {
         (void)strcat(p_db->user, "@");
         (void)strcat(p_db->user, fsa[p_db->position].proxy_name);
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

      /* Save the destination directory */
      if (*ptr == '/')
      {
         ptr++;
         i = 0;
         while ((*ptr != '\0') && (*ptr != ';'))
         {
            p_db->target_dir[i] = *ptr;
            i++; ptr++;
         }
         p_db->target_dir[i] = '\0';
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
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Actually I was expecting <type=> and not <%s=> (%s %d)\n",
                            ptr_tmp, __FILE__, __LINE__);
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
                  default : (void)rec(sys_log_fd, ERROR_SIGN,
                                      "Unknown ftp type (%d). Changing to I. (%s %d)\n",
                                      *ptr, __FILE__, __LINE__);
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
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Actually I was expecting <server=> and not <%s=> (%s %d)\n",
                                 ptr_tmp, __FILE__, __LINE__);
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
   }
   else
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Hmm. This does NOT look like URL for me!? (%s %d)\n",
                __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}
