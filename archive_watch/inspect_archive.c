/*
 *  inspect_archive.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Deutscher Wetterdienst (DWD),
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
 **   inspect_archive - walks through a directory and deletes old
 **                     archives
 **
 ** SYNOPSIS
 **   void inspect_archive(char *archive_dir)
 **
 ** DESCRIPTION
 **   The function inspect_archive() walks through the archive of the
 **   AFD and deletes any old archives. The archive directories have
 **   the following format:
 **    $(AFD_WORK_DIR)/archive/[hostalias]/[user]/[dirnumber]/[archive_name]
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1996 H.Kiehl Created
 **   11.05.1998 H.Kiehl Adapted to new message names.
 **   29.11.2000 H.Kiehl Optimized the removing of files.
 **   28.12.2000 H.Kiehl Remove all empty directories in archive.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcmp(), strlen(),    */
                                     /* strerror()                       */
#include <stdlib.h>                  /* atoi()                           */
#include <ctype.h>                   /* isdigit()                        */
#include <unistd.h>                  /* rmdir(), unlink()                */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>                  /* opendir(), readdir(), closedir() */
#include <errno.h>
#include "awdefs.h"

#define TIME_UP  1

/* External global variables. */
extern int          sys_log_fd;
extern unsigned int removed_archives,
                    removed_files;
extern time_t       current_time;

/* Local function prototypes. */
static int          check_time(char *),
                    is_msgname(char *),
                    remove_archive(char *);


/*########################### inspect_archive() #########################*/
void
inspect_archive(char *archive_dir)
{
   DIR *p_dir_archive;

   if ((p_dir_archive = opendir(archive_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s : %s (%s %d)\n",
                archive_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      int           noh, nou, nod;
      char          *ptr_archive,
                    *ptr_hostname,
                    *ptr_username,
                    *ptr_dirnumber;
      struct dirent *dp_archive,
                    *dp_hostname,
                    *dp_username,
                    *dp_dirnumber;
      DIR           *p_dir_hostname,
                    *p_dir_username,
                    *p_dir_dirnumber;

      ptr_archive = archive_dir + strlen(archive_dir);
      *(ptr_archive++) = '/';
      while ((dp_archive = readdir(p_dir_archive)) != NULL)
      {
         if (dp_archive->d_name[0]  != '.')
         {
            /* Enter directory with hostname. */
            (void)strcpy(ptr_archive, dp_archive->d_name);
            if ((p_dir_hostname = opendir(archive_dir)) != NULL)
            {
               ptr_hostname = archive_dir + strlen(archive_dir);
               *(ptr_hostname++) = '/';
               noh = 0;
               while ((dp_hostname = readdir(p_dir_hostname)) != NULL)
               {
                  noh++;
                  if (dp_hostname->d_name[0]  != '.')
                  {
                     /* Enter directory with username. */
                     (void)strcpy(ptr_hostname, dp_hostname->d_name);
                     if ((p_dir_username = opendir(archive_dir)) != NULL)
                     {
                        ptr_username = archive_dir + strlen(archive_dir);
                        *(ptr_username++) = '/';
                        nou = 0;
                        while ((dp_username = readdir(p_dir_username)) != NULL)
                        {
                           nou++;
                           if (dp_username->d_name[0]  != '.')
                           {
                              /* Enter directory with dirnumber. */
                              (void)strcpy(ptr_username, dp_username->d_name);
                              if ((p_dir_dirnumber = opendir(archive_dir)) != NULL)
                              {
                                 ptr_dirnumber = archive_dir + strlen(archive_dir);
                                 *(ptr_dirnumber++) = '/';
                                 nod = 0;
                                 while ((dp_dirnumber = readdir(p_dir_dirnumber)) != NULL)
                                 {
                                    nod++;
                                    if (dp_dirnumber->d_name[0]  != '.')
                                    {
                                       if (is_msgname(dp_dirnumber->d_name) != INCORRECT)
                                       {
                                          if (check_time(dp_dirnumber->d_name) == TIME_UP)
                                          {
                                             (void)strcpy(ptr_dirnumber, dp_dirnumber->d_name);
                                             if (remove_archive(archive_dir) != INCORRECT)
                                             {
                                                removed_archives++;
                                                nod--;
#ifdef _LOG_REMOVE_INFO
                                                (void)rec(sys_log_fd, INFO_SIGN,
                                                          "Removed archive %s. (%s %d)\n",
                                                          archive_dir, __FILE__, __LINE__);
#endif
                                             }
                                          }
                                       }
                                    }
                                 } /* while (readdir(dirnumber)) */
                                 if (closedir(p_dir_dirnumber) == -1)
                                 {
                                    ptr_dirnumber[-1] = '\0';
                                    (void)rec(sys_log_fd, ERROR_SIGN,
                                              "Failed to closedir() %s : %s (%s %d)\n",
                                              archive_dir, strerror(errno), __FILE__, __LINE__);
                                 }
                                 else if (nod == 2)
                                      {
                                         ptr_dirnumber[-1] = '\0';
                                         if ((rmdir(archive_dir) == -1) &&
                                             (errno != EEXIST))
                                         {
                                            (void)rec(sys_log_fd, WARN_SIGN,
                                                      "Failed to rmdir() %s : %s (%s %d)\n",
                                                      archive_dir, strerror(errno),
                                                      __FILE__, __LINE__);
                                         }
                                         else
                                         {
                                            nou--;
                                         }
                                      }
                              }
                           }
                        }
                        if (closedir(p_dir_username) == -1)
                        {
                           ptr_username[-1] = '\0';
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "Failed to closedir() %s : %s (%s %d)\n",
                                     archive_dir, strerror(errno), __FILE__, __LINE__);
                        }
                        else if (nou == 2)
                             {
                                ptr_username[-1] = '\0';
                                if ((rmdir(archive_dir) == -1) &&
                                    (errno != EEXIST))
                                {
                                   (void)rec(sys_log_fd, WARN_SIGN,
                                             "Failed to rmdir() %s : %s (%s %d)\n",
                                             archive_dir, strerror(errno),
                                             __FILE__, __LINE__);
                                }
                                else
                                {
                                   noh--;
                                }
                             }
                     }
                  }
               }
               if (closedir(p_dir_hostname) == -1)
               {
                  ptr_hostname[-1] = '\0';
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "Failed to closedir() %s : %s (%s %d)\n",
                            archive_dir, strerror(errno), __FILE__, __LINE__);
               }
               else if (noh == 2)
                    {
                       ptr_hostname[-1] = '\0';
                       if ((rmdir(archive_dir) == -1) &&
                           (errno != EEXIST))
                       {
                          (void)rec(sys_log_fd, WARN_SIGN,
                                    "Failed to rmdir() %s : %s (%s %d)\n",
                                    archive_dir, strerror(errno),
                                    __FILE__, __LINE__);
                       }
                    }
            }
         }
      }
      ptr_archive[-1] = '\0';
      if (closedir(p_dir_archive) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to closedir() %s : %s (%s %d)\n",
                   archive_dir, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ is_msgname() ++++++++++++++++++++++++++++*/
static int
is_msgname(char *name)
{
   if (isdigit(name[0]) != 0)
   {
      if (name[1] == '_')
      {
         int i = 2;

         while (name[i] != '\0')
         {
            if (isdigit(name[i]) == 0)
            {
               if (name[i] != '_')
               {
                  return(INCORRECT);
               }
            }
            i++;
         }
         return(SUCCESS);
      }
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ check_time() +++++++++++++++++++++++++++++*/
static int
check_time(char *name)
{
   int  i = 2;
   char str_time[MAX_INT_LENGTH];

   while ((name[i] != '_') && (name[i] != '\0'))
   {
      str_time[i - 2] = name[i];
      i++;
   }
   str_time[i - 2] = '\0';

   if (current_time < atoi(str_time))
   {
      return(0);
   }

   return(TIME_UP);
}


/*+++++++++++++++++++++++++++ remove_archive() ++++++++++++++++++++++++++*/
static int
remove_archive(char *dirname)
{
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

   if ((dp = opendir(dirname)) == NULL)
   {
      if (errno != ENOTDIR)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to opendir() %s : %s (%s %d)\n",
                   dirname, strerror(errno), __FILE__, __LINE__);
      }
      return(INCORRECT);
   }

   ptr = dirname + strlen(dirname);
   *ptr++ = '/';

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
      if (unlink(dirname) == -1)
      {
         if (errno == ENOENT)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Failed to unlink() %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to delete %s : %s (%s %d)\n",
                      dirname, strerror(errno), __FILE__, __LINE__);
            (void)closedir(dp);
            return(INCORRECT);
         }
      }
      else
      {
         removed_files++;
      }
   }
   ptr[-1] = 0;
   if ((rmdir(dirname) == -1) && (errno != EEXIST))
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to rmdir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      (void)closedir(dp);
      return(INCORRECT);
   }
   if (closedir(dp) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to closedir() %s : %s (%s %d)\n",
                dirname, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   return(SUCCESS);
}
