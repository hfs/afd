/*
 *  check_old_time_jobs.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 1999 - 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_old_time_jobs - checks if there are any old time jobs
 **                         after a DIR_CONFIG update
 **
 ** SYNOPSIS
 **   void check_old_time_jobs(int no_of_jobs)
 **
 ** DESCRIPTION
 **   The function check_old_time_jobs() searches the time directory
 **   for any old time jobs after a DIR_CONFIG update. If it finds any
 **   old job, it tries to locate that job in the current job list
 **   and move all the files to the new time job directory. If it
 **   fails to locate the the job in the current or old job list, all
 **   files will be deleted and logged in the delete log.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* rename()                                */
#include <string.h>           /* strerror(), strcpy(), strlen()          */
#include <stdlib.h>           /* strtoul()                               */
#include <ctype.h>            /* isdigit()                               */
#include <sys/types.h>
#include <sys/stat.h>         /* S_ISDIR()                               */
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* rmdir(), unlink()                       */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                 *no_of_job_ids,
                           no_of_time_jobs,
                           sys_log_fd,
                           *time_job_list;
extern char                time_dir[];
extern struct instant_db   *db;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;

/* Local function prototype */
static void move_time_dir(int);

/* #define _STRONG_OPTION_CHECK 1 */


/*####################### check_old_time_jobs() #########################*/
void
check_old_time_jobs(int no_of_jobs)
{
   DIR *dp;

   /*
    * Search time pool directory and see if there are any old jobs
    * from the last DIR_CONFIG that still have to be processed.
    */
   if ((dp = opendir(time_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Can't access directory %s : %s (%s %d)\n",
                time_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      int           i;
      char          *ptr,
                    str_number[MAX_INT_LENGTH],
                    *time_dir_ptr;
      struct dirent *p_dir;

      time_dir_ptr = time_dir + strlen(time_dir);
      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         ptr = p_dir->d_name;
         i = 0;
         while ((*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            if (isdigit(*ptr) == 0)
            {
               break;
            }
            str_number[i] = *ptr;
            i++; ptr++;
         }
         if (*ptr == '\0')
         {
            struct stat stat_buf;

            (void)strcpy(time_dir_ptr, p_dir->d_name);
            if (stat(time_dir, &stat_buf) == -1)
            {
               if (errno != ENOENT)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to stat() %s : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               if (S_ISDIR(stat_buf.st_mode))
               {
                  int gotcha = NO,
                      job_id;

                  str_number[i] = '\0';
                  job_id = (int)strtoul(str_number, NULL, 10);

                  for (i = 0; i < no_of_time_jobs; i++)
                  {
                     if (db[time_job_list[i]].job_id == job_id)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     /*
                      * Befor we try to determine the new JID number lets
                      * try to delete the directory. If we are successful
                      * then there where no files and we can save us a
                      * complex search for the new one.
                      */
                     if ((rmdir(time_dir) == -1) && (errno == ENOTEMPTY))
                     {
                        int jid_pos = -1;

                        /* Locate the lost job in structure job_id_data */
                        for (i = 0; i < *no_of_job_ids; i++)
                        {
                           if (jd[i].job_id == job_id)
                           {
                              jid_pos = i;
                              break;
                           }
                        }
                        if (jid_pos == -1)
                        {
                           /*
                            * The job cannot be found in the JID structure!?
                            * The only thing we can do now is remove them.
                            */
#ifdef _DELETE_LOG
                           remove_time_dir("-", -1, OTHER_DEL);
#else
                           remove_time_dir("-", -1);
#endif
                        }
                        else
                        {
                           int new_job_id = -1;

                           /*
                            * Hmmm. Now comes the difficult part! Try find
                            * a current job in the JID structure that resembles
                            * the lost job. Question is how close should the
                            * new job resemble the old one? If we resemble it
                            * to close we loose more data, since we do not find
                            * a job matching. On the other hand the less we
                            * care how well the job resembles, the higher will
                            * be the chance that we catch the wrong job in the
                            * JID structure. The big problem here are the options.
                            * Well, some options we do not need to worry about:
                            * priority, time and lock. If these options cause
                            * difficulties, then the DIR_CONFIG configuration
                            * is broke!
                            */
                           for (i = 0; i < no_of_jobs; i++)
                           {
                              if ((dnb[jd[jid_pos].dir_id_pos].dir_id == db[i].dir_no) &&
                                  (jd[jid_pos].no_of_files == db[i].no_of_files))
                              {
                                 int  j;
                                 char *p_file_db = db[i].files,
                                      *p_file_jd = jd[jid_pos].file_list;

                                 gotcha = NO;
                                 for (j = 0; j < jd[jid_pos].no_of_files; j++)
                                 {
                                    if (CHECK_STRCMP(p_file_jd, p_file_db) != 0)
                                    {
                                       gotcha = YES;
                                       break;
                                    }
                                    NEXT(p_file_db);
                                    NEXT(p_file_jd);
                                 }
                                 if (gotcha == NO)
                                 {
                                    if (CHECK_STRCMP(jd[jid_pos].recipient, db[i].recipient) == 0)
                                    {
#ifdef _STRONG_OPTION_CHECK
                                       if (jd[jid_pos].no_of_loptions == db[i].no_of_loptions)
                                       {
                                          char *p_loptions_db = db[i].loptions,
                                               *p_loptions_jd = jd[jid_pos].loptions;

                                          for (j = 0; j < jd[jid_pos].no_of_loptions; j++)
                                          {
                                             if ((CHECK_STRCMP(p_loptions_jd, p_loptions_db) != 0) &&
                                                 (strncmp(p_loptions_jd, TIME_ID, TIME_ID_LENGTH) != 0))
                                             {
                                                gotcha = YES;
                                                break;
                                             }
                                             NEXT(p_loptions_db);
                                             NEXT(p_loptions_jd);
                                          }
                                          if (gotcha == NO)
                                          {
                                             if (jd[jid_pos].no_of_soptions == db[i].no_of_soptions)
                                             {
                                                if (jd[jid_pos].no_of_soptions > 0)
                                                {
                                                   if (CHECK_STRCMP(jd[jid_pos].soptions, db[i].soptions) != 0)
                                                   {
                                                      gotcha = YES;
                                                   }
                                                }
                                                if (gotcha == NO)
                                                {
                                                   new_job_id = db[i].job_id;
                                                   break;
                                                }
                                             }
                                          }
                                       }
#else
                                       new_job_id = db[i].job_id;
                                       break;
#endif
                                    }
                                 }
                              }
                           } /* for (i = 0; i < no_of_jobs; i++) */

                           if (new_job_id == -1)
                           {
                              /*
                               * Since we were not able to locate a similar
                               * job lets assume it has been taken from the
                               * DIR_CONFIG. In that case we have to remove
                               * all files.
                               */
#ifdef _DELETE_LOG
                             remove_time_dir(jd[jid_pos].host_alias,
                                              jd[jid_pos].job_id,
                                              OTHER_DEL);
#else
                             remove_time_dir(jd[jid_pos].host_alias,
                                              jd[jid_pos].job_id);
#endif
                           }
                           else
                           {
                              /*
                               * Lets move all files from the old job directory
                               * to the new one.
                               */
                              move_time_dir(new_job_id);
                           }
                        }
                     } /* if ((rmdir(time_dir) == -1) && (errno == ENOTEMPTY)) */
                  } /* if (gotcha == NO) */
               } /* if (S_ISDIR(stat_buf.st_mode) == 0) */
            } /* stat() successful */
         } /* if (*ptr == '\0') */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */

      *time_dir_ptr = '\0';
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to readdir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to closedir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}


/*+++++++++++++++++++++++++++ move_time_dir() +++++++++++++++++++++++++++*/
static void
move_time_dir(int job_id)
{
#ifdef _CHECK_TIME_DIR_DEBUG
   (void)rec(sys_log_fd, INFO_SIGN,
             "Moving time directory %s to %d (%s %d)\n",
             time_dir, job_id, __FILE__, __LINE__);
#else
   DIR *dp;

   if ((dp = opendir(time_dir)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to opendir() %s to move old time jobs : %s (%s %d)\n",
                time_dir, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      char          *ptr,
                    *tmp_ptr,
                    to_dir[MAX_PATH_LENGTH],
                    *to_dir_ptr;
      struct dirent *p_dir;

      ptr = time_dir + strlen(time_dir);
      *(ptr++) = '/';
      tmp_ptr = ptr - 2;
      while ((*tmp_ptr != '/') && (tmp_ptr > time_dir))
      {
         tmp_ptr--;
      }
      if (*tmp_ptr == '/')
      {
         char tmp_char;

         tmp_ptr++;
         tmp_char = *tmp_ptr;
         *tmp_ptr = '\0';
         to_dir_ptr = to_dir + sprintf(to_dir, "%s%d", time_dir, job_id);
         *tmp_ptr = tmp_char;
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Hmmm.. , something is wrong here!? (%s %d)\n",
                   __FILE__, __LINE__);
         *(ptr - 1) = '\0';
         return;
      }
      if (mkdir(to_dir, (S_IRUSR | S_IWUSR | S_IXUSR)) == -1)
      {
         if (errno != EEXIST)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not mkdir() %s to move old time job : %s (%s %d)\n",
                      to_dir, strerror(errno), __FILE__, __LINE__);
            *(ptr - 1) = '\0';
            if (closedir(dp) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not closedir() %s : %s (%s %d)\n",
                         time_dir, strerror(errno), __FILE__, __LINE__);
            }
            return;
         }
      }
      *(to_dir_ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         (void)strcpy(to_dir_ptr, p_dir->d_name);

         if (rename(time_dir, to_dir) == -1)
         {
            if (errno != EEXIST)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to rename() %s to %s : %s (%s %d)\n",
                         time_dir, to_dir, strerror(errno),
                         __FILE__, __LINE__);
            }
            if (unlink(time_dir) == -1)
            {
               if (errno != ENOENT)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to unlink() %s : %s (%s %d)\n",
                            time_dir, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         errno = 0;
      }

      *(ptr - 1) = '\0';
      if (errno)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not readdir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (closedir(dp) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not closedir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
      if (rmdir(time_dir) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not rmdir() %s : %s (%s %d)\n",
                   time_dir, strerror(errno), __FILE__, __LINE__);
      }
   }
#endif

   return;
}
