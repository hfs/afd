/*
 *  show_exec_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   show_exec_stat - shows exec statistics of dir_check
 **
 ** SYNOPSIS
 **   show_exec_stat [-w <working directory>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.04.2004 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                       /* open()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "amgdefs.h"
#include "permission.h"
#include "version.h"

/* Global variables */
int        sys_log_fd = STDERR_FILENO;   /* Not used!    */
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ show_exec_stat() $$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  fd;
   char action,
        fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer,
        user[MAX_FULL_USER_ID_LENGTH],
        fifo[MAX_PATH_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   get_user(user, fake_user);

   /*
    * Ensure that the user may use this program.
    */
   switch(get_permissions(&perm_buffer, fake_user))
   {
      case NONE     :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         {
            int permission = NO;

            if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
                (perm_buffer[2] == 'l') &&
                ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
                 (perm_buffer[3] == '\t')))
            {
               permission = YES;
            }
            else
            {
               if (posi(perm_buffer, SHOW_EXEC_STAT_PERM) != NULL)
               {
                  permission = YES;
               }
            }
            free(perm_buffer);
            if (permission != YES)
            {
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
               exit(INCORRECT);
            }
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default       :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   (void)sprintf(fifo, "%s%s%s", p_work_dir, FIFO_DIR, DC_CMD_FIFO);
   if ((fd = open(fifo, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   action = SR_EXEC_STAT;
   if (write(fd, &action, 1) != 1)
   {
      (void)fprintf(stderr, "Failed to write() to %s : %s (%s %d)\n",
                    fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                    fifo, strerror(errno), __FILE__, __LINE__);
   }

   exit(SUCCESS);
}
