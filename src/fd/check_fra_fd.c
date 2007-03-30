/*
 *  check_fra_fd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fra_fd - checks if FRA has been updated
 **
 ** SYNOPSIS
 **   int check_fra_fd(void)
 **
 ** DESCRIPTION
 **   This function checks if the FRA (Fileretrieve Status Area)
 **   which is a memory mapped area, is still in use. If not
 **   it will first check the queue for retrieve jobs to store the
 **   dir_alias, so the position of this can be located int the new
 **   structure. Then it will detach from the old memory area and
 **   attach to the new one with the function fra_attach(). And
 **   if any retrieve jobs where found the position in the queue
 **   will be updated.
 **
 ** RETURN VALUES
 **   Returns NO if the FRA is still in use. Returns YES if a
 **   new FRA has been created. It will then also return new
 **   values for 'fra_id' and 'no_of_dirs'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.08.2000 H.Kiehl Created
 **   22.07.2002 H.Kiehl Consider the case when only dir_alias changes.
 **   21.09.2005 H.Kiehl Update qb_pos in struct rql when we remove a
 **                      message from queue.
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* stderr, NULL                 */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* realloc(), free()            */
#include <unistd.h>                      /* unlink()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>                    /* waitpid()                    */
#include <signal.h>                      /* kill()                       */
#ifdef HAVE_MMAP
#include <sys/mman.h>                    /* munmap()                     */
#endif
#include <errno.h>
#include "fddefs.h"

/* Global variables */
extern int                        fra_id,
                                  max_connections,
                                  *no_msg_queued,
                                  no_of_retrieves,
                                  no_of_dirs,
                                  *retrieve_list;
#ifdef HAVE_MMAP
extern off_t                      fra_size;
#endif
extern char                       *p_work_dir;
extern struct connection          *connection;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;
extern struct queue_buf           *qb;
extern struct afd_status          *p_afd_status;

/* Local function prototypes. */
static int                        get_url_pos(char *, char *, int *);

/* Local structure definition. */
struct remote_queue_list
       {
          char dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          int  qb_pos;
       };
struct old_retrieve_data
       {
          char          dir_alias[MAX_DIR_ALIAS_LENGTH + 1];
          char          url[MAX_RECIPIENT_LENGTH];
          unsigned char remove;
          unsigned char stupid_mode;
       };


/*########################### check_fra_fd() ############################*/
int
check_fra_fd(void)
{
   if (fra != NULL)
   {
      char *ptr;

      ptr = (char *)fra;
      ptr -= AFD_WORD_OFFSET;

      if (*(int *)ptr == STALE)
      {
         register int             i, j;
         int                      no_remote_queued = 0;
         struct remote_queue_list *rql = NULL;
         struct old_retrieve_data *ord = NULL;

         for (i = 0; i < *no_msg_queued; i++)
         {
            if (qb[i].msg_name[0] == '\0')
            {
               if ((no_remote_queued % 10) == 0)
               {
                  size_t new_size = ((no_remote_queued / 10) + 1) * 10 *
                                    sizeof(struct remote_queue_list);

                  if ((rql = realloc(rql, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "realloc() error : %s", strerror(errno));
                     exit(INCORRECT);
                  }
               }
               (void)strcpy(rql[no_remote_queued].dir_alias,
                            fra[qb[i].pos].dir_alias);
               rql[no_remote_queued].qb_pos = i;
               no_remote_queued++;
            }
         }

         /*
          * We need to check if the url ie. the directory is still
          * the same and if it is still in the new FRA. If not lets
          * remove the corresponding file in the ls_data directory.
          */
         if (no_of_retrieves > 0)
         {
            char *tmp_ptr;

            if ((tmp_ptr = malloc((no_of_retrieves * sizeof(struct old_retrieve_data)))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s", strerror(errno));
            }
            else
            {
               ord = (struct old_retrieve_data *)tmp_ptr;
               for (i = 0; i < no_of_retrieves; i++)
               {
                  (void)strcpy(ord[i].url, fra[retrieve_list[i]].url);
                  (void)strcpy(ord[i].dir_alias,
                               fra[retrieve_list[i]].dir_alias);
                  ord[i].remove = fra[retrieve_list[i]].remove;
                  ord[i].stupid_mode = fra[retrieve_list[i]].stupid_mode;
               }
            }
         }

#ifdef HAVE_MMAP
         if (munmap(ptr, fra_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() from FRA [fra_id = %d fra_size = %d] : %s",
                       fra_id, fra_size, strerror(errno));
         }
#else
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap_emu() from FRA (%d) : %s",
                       fra_id, strerror(errno));
         }
#endif

         if (fra_attach() < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to attach to FRA.");
            exit(INCORRECT);
         }

         if ((no_of_retrieves > 0) && (ord != NULL))
         {
            int pos;

            for (i = 0; i < no_of_retrieves; i++)
            {
               if (get_url_pos(ord[i].url,
                               ord[i].dir_alias, &pos) == INCORRECT)
               {
                  char fullname[MAX_PATH_LENGTH];

                  if ((ord[i].stupid_mode != YES) && (ord[i].remove == NO))
                  {
                     (void)sprintf(fullname, "%s%s%s%s/%s", p_work_dir,
                                   AFD_FILE_DIR, INCOMING_DIR, LS_DATA_DIR,
                                   ord[i].dir_alias);
                     if (unlink(fullname) == -1)
                     {
                        if (errno != ENOENT)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      "Failed to unlink() old ls data file %s : %s",
                                      fullname, strerror(errno));
                        }
                     }
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Remove old ls data file %s.", fullname);
                     }
                  }

                  /*
                   * Only when the dir_alias is NOT found may we delete
                   * the remote directory. Otherwise we will remove it
                   * AFTER AMG has created it!
                   */
                  if (pos == INCORRECT)
                  {
                     if (create_remote_dir(ord[i].url, fullname) != INCORRECT)
                     {
                        if (rec_rmdir(fullname) != INCORRECT)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Remove incoming directory %s.",
                                      fullname);
                        }
                     }
                  }

                  for (j = 0; j < no_remote_queued; j++)
                  {
                     if (CHECK_STRCMP(ord[i].dir_alias, rql[j].dir_alias) == 0)
                     {
                        int k;

                        if (qb[rql[j].qb_pos].pid > 0)
                        {
                           if (kill(qb[rql[j].qb_pos].pid, SIGKILL) < 0)
                           {
                              if (errno != ESRCH)
                              {
                                 system_log(WARN_SIGN, __FILE__, __LINE__,
                                            "Failed to kill transfer job with pid %d : %s",
                                            qb[rql[j].qb_pos].pid,
                                            strerror(errno));
                              }
                           }
                           else
                           {
                              int ret,
                                  status;

                              /* Catch the zombie! */
                              if ((ret = waitpid(qb[rql[j].qb_pos].pid, &status,
                                                 0)) == qb[rql[j].qb_pos].pid)
                              {
                                 int m;

                                 for (m = 0; m < max_connections; m++)
                                 {
                                    if (connection[m].pid == qb[rql[j].qb_pos].pid)
                                    {
                                       fsa[connection[m].fsa_pos].active_transfers -= 1;
                                       if (fsa[connection[m].fsa_pos].active_transfers < 0)
                                       {
                                          fsa[connection[m].fsa_pos].active_transfers = 0;
                                       }
                                       calc_trl_per_process(connection[m].fsa_pos);
                                       fsa[connection[m].fsa_pos].job_status[connection[m].job_no].proc_id = -1;
                                       fsa[connection[m].fsa_pos].job_status[connection[m].job_no].connect_status = DISCONNECT;
#ifdef _WITH_BURST_2
                                       fsa[connection[m].fsa_pos].job_status[connection[m].job_no].unique_name[0] = '\0';
                                       fsa[connection[m].fsa_pos].job_status[connection[m].job_no].job_id = NO_ID;
#endif
                                       connection[m].pid = 0;
                                       connection[m].hostname[0] = '\0';
                                       connection[m].job_no = -1;
                                       connection[m].fsa_pos = -1;
                                       connection[m].fra_pos = -1;
                                       connection[m].msg_name[0] = '\0';
                                       break;
                                    }
                                 }
                                 if (p_afd_status->no_of_transfers > 0)
                                 {
                                    p_afd_status->no_of_transfers--;
                                 }
                              }
                              else if (ret == -1)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 "waitpid() error [%d] : %s",
                                                 qb[rql[j].qb_pos].pid,
                                                 strerror(errno));
                                   }
                           }
                        }
                        remove_msg(rql[j].qb_pos);
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Removed message for retrieving directory %s from queue.",
                                   rql[j].dir_alias);

                        /*
                         * If we remove a message from queue, we must update
                         * qb_pos in struct rql!
                         */
                        for (k = 0; k < no_remote_queued; k++)
                        {
                           if (rql[k].qb_pos > rql[j].qb_pos)
                           {
                              rql[k].qb_pos -= 1;
                           }
                        }
                     }
                  }
               }
               else /* get_url_pos() == CORRECT */
               {
                  for (j = 0; j < no_remote_queued; j++)
                  {
                     if (CHECK_STRCMP(ord[i].dir_alias, rql[j].dir_alias) == 0)
                     {
                        qb[rql[j].qb_pos].pos = pos;
                     }
                  }
               }
            } /* for (i = 0; i < no_of_retrieves; i++) */
         }
         if (rql != NULL)
         {
            free(rql);
         }
         if (ord != NULL)
         {
            free(ord);
         }

         return(YES);
      }
   }

   return(NO);
}


/*++++++++++++++++++++++++++++ get_url_pos() ++++++++++++++++++++++++++++*/
static int
get_url_pos(char *url, char *dir_alias, int *pos)
{
   register int i;

   *pos = INCORRECT;
   for (i = 0; i < no_of_dirs; i++)
   {
      if ((fra[i].host_alias[0] != '\0') &&
          (CHECK_STRCMP(fra[i].dir_alias, dir_alias) == 0))
      {
         *pos = i;
         if (CHECK_STRCMP(fra[i].url, url) == 0)
         {
            return(SUCCESS);
         }
         break;
      }
   }

   /* Check if only the dir_alias was changed. */
   if (*pos == INCORRECT)
   {
      for (i = 0; i < no_of_dirs; i++)
      {
         if ((fra[i].host_alias[0] != '\0') &&
             (CHECK_STRCMP(fra[i].url, url) == 0))
         {
            *pos = i;
            return(SUCCESS);
         }
      }
   }

   return(INCORRECT);
}
