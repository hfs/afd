/*
 *  get_permissions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2004 Deutscher Wetterdienst (DWD),
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
 **   get_permissions - retrieves permissions from the AFD_USER_FILE
 **                     for the calling user
 **
 ** SYNOPSIS
 **   int get_permissions(char **perm_buffer, char *fake_user)
 **
 ** DESCRIPTION
 **   This function returns a list of permissions in 'perm_buffer'.
 **   To store the permissions it needs to allocate memory for
 **   which the caller is responsible to free.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it either fails to stat()/open()/read()
 **   from the AFD_USER_FILE. SUCCESS is returned when the calling
 **   user is found in AFD_USER_FILE and a list of permissions is
 **   stored in 'perm_buffer'. When the calling user could not be found,
 **   NONE is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.05.1997 H.Kiehl Created
 **   05.11.2003 H.Kiehl Ensure that we do check the full username.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strcpy(), strerror()        */
#include <stdlib.h>             /* getenv(), calloc(), free()            */
#include <unistd.h>             /* read(), close(), getuid()             */
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>                /* getpwuid()                            */
#include <fcntl.h>
#include <errno.h>

/* External global variables */
extern char *p_work_dir;


/*########################## get_permissions() ##########################*/
int
get_permissions(char **perm_buffer, char *fake_user)
{
   int           fd;
   char          *ptr = NULL,
                 *buffer,
                 user[256],
                 afd_user_file[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct passwd *pwd;

   user[0] = '\n';
   if (fake_user[0] == '\0')
   {
      if ((pwd = getpwuid(getuid())) != NULL)
      {
         (void)strcpy(&user[1], pwd->pw_name);
      }
      else
      {
         (void)strcpy(&user[1], "unknown");
      }
   }
   else
   {
      (void)strcpy(&user[1], fake_user);
   }

   (void)strcpy(afd_user_file, p_work_dir);
   (void)strcat(afd_user_file, ETC_DIR);
   (void)strcat(afd_user_file, AFD_USER_FILE);

   if ((fd = open(afd_user_file, O_RDONLY)) == -1)
   {
      /*
       * If there is no AFD_USER_FILE, or we fail to open
       * it for whatever reason, lets allow everything.
       */
      return(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      /*
       * If there is no AFD_USER_FILE, or we fail to open
       * it for whatever reason, lets allow everything.
       */
      (void)close(fd);
      return(INCORRECT);
   }

   /* Create two buffers, one to scribble around 'buffer' the other */
   /* is returned to the calling process.                           */
   if (((buffer = malloc(stat_buf.st_size + 2)) == NULL) ||
       ((*perm_buffer = calloc(stat_buf.st_size, sizeof(char))) == NULL))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to calloc(). Permission control deactivated!!! : %s",
                 strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   if (read(fd, &buffer[1], stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() <%s>. Permission control deactivated!!! : %s",
                 afd_user_file, strerror(errno));
      free(buffer); free(*perm_buffer);
      (void)close(fd);
      return(INCORRECT);
   }
   buffer[0] = '\n';
   buffer[stat_buf.st_size + 1] = '\0';

   ptr = buffer;
   do
   {
      if (((ptr = posi(ptr, user)) != NULL) &&
          ((*(ptr - 1) == ' ') || (*(ptr - 1) == '\t')))
      {
         break;
      }
   } while (ptr != NULL);

   if (ptr != NULL)
   {
      int  i = 0;

      do
      {
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (*ptr != '\n')
         {
            *(*perm_buffer + i) = *ptr;
            i++; ptr++;
         }
         if (((ptr + 1) - buffer) >= stat_buf.st_size)
         {
            break;
         }
         ptr++;
      } while ((*ptr == ' ') || (*ptr == '\t'));

      *(*perm_buffer + i) = '\0';
   }
   else
   {
      free(buffer); free(*perm_buffer);
      (void)close(fd);
      return(NONE);
   }

   if (close(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   free(buffer);

   /*
    * The caller is responsible to free 'perm_buffer'!
    */

   return(SUCCESS);
}
