/*
 *  reread_dir_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reread_dir_config - reads the DIR_CONFIG file
 **
 ** SYNOPSIS
 **   void reread_dir_config(time_t           *dc_old_time,
 **                          time_t           *hc_old_time,
 **                          int              old_no_of_hosts,
 **                          int              rewrite_host_config,
 **                          size_t           old_size,
 **                          int              rescan_time,
 **                          int              max_no_proc,
 **                          struct host_list *old_hl)
 **                                                         
 ** DESCRIPTION
 **   This function reads the DIR_CONFIG file and sets the values
 **   in the FSA.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>            /* memcmp(), memset(), strerror()         */
#include <stdlib.h>            /* malloc(), free()                       */
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>            /* kill()                                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        data_length,
                                  no_of_dir,
                                  no_of_hosts,
                                  shm_id,
                                  sys_log_fd;
extern pid_t                      pid,
                                  tmp_pid;
extern char                       dir_config_file[],
                                  *pid_list,
                                  *p_work_dir;
extern struct host_list           *hl;
extern struct filetransfer_status *fsa;


/*########################## reread_dir_config() ########################*/
void
reread_dir_config(time_t           *dc_old_time,
                  time_t           *hc_old_time,
                  int              old_no_of_hosts,
                  int              rewrite_host_config,
                  size_t           old_size,
                  int              rescan_time,
                  int              max_no_proc,
                  struct host_list *old_hl)
{
   int         new_fsa = NO;
   struct stat stat_buf;

   if (stat(dir_config_file, &stat_buf) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Could not stat() DIR_CONFIG file %s : %s (%s %d)\n",
                dir_config_file, strerror(errno),
                __FILE__, __LINE__);
   }
   else
   {
      if ((*dc_old_time >= stat_buf.st_mtime) && (old_hl != NULL))
      {
         /* First check if there was any change at all. */
         if ((old_no_of_hosts == no_of_hosts) &&
             (memcmp(hl, old_hl, old_size) == 0))
         {
            new_fsa = NONE;
            if (rewrite_host_config == NO)
            {
               (void)rec(sys_log_fd, INFO_SIGN,
                         "There is no change in the HOST_CONFIG file.\n");
            }
         }
         else /* Yes, something did change. */
         {
            int  host_pos,
                 i,
                 j,
                 no_of_gotchas = 0;
            char *gotcha = NULL;

            /*
             * The gotcha array is used to find hosts that are in the
             * old FSA but not in the HOST_CONFIG file.
             */
            if ((gotcha = malloc(old_no_of_hosts)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "malloc() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            (void)memset(gotcha, NO, old_no_of_hosts);

            for (i = 0; i < no_of_hosts; i++)
            {
               host_pos = INCORRECT;
               for (j = 0; j < old_no_of_hosts; j++)
               {
                  if (gotcha[j] != YES)
                  {
                     if (strcmp(hl[i].host_alias, old_hl[j].host_alias) == 0)
                     {
                        host_pos = j;
                        break;
                     }
                  }
               }
               if (host_pos == INCORRECT)
               {
                  /*
                   * A new host. Now we have to create a new FSA.
                   */
                  new_fsa = YES;
                  break;
               }
               else
               {
                  gotcha[j] = YES;
                  no_of_gotchas++;
               }
            } /* for (i = 0; i < no_of_hosts; i++) */

            free(gotcha);

            if ((no_of_gotchas != old_no_of_hosts) ||
                (new_fsa == YES))
            {
               new_fsa = YES;
               create_fsa();
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Found %d hosts in HOST_CONFIG.\n",
                         no_of_hosts);
            }
         }
      }

      /* Check if DIR_CONFIG has changed */
      if (*dc_old_time < stat_buf.st_mtime)
      {
         int i;

         /* Tell user we have to reread the new DIR_CONFIG file */
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Rereading DIR_CONFIG...\n");

         /* Stop/Pause running jobs */
         if ((data_length > 0) && (pid > 0))
         {
#ifdef _WITH_PAUSING
            if (com(HALT) == INCORRECT)
#else
            if (com(STOP) == INCORRECT)
#endif
            {
               int status;

               /* If the process does not answer, lets assume */
               /* something is really wrong here and lets see */
               /* if the process has died.                    */
               if ((status = amg_zombie_check(&pid, WNOHANG)) < 0)
               {
                  pid = status;
               }
               else /* Mark process in unknown state */
               {
                  tmp_pid = pid;
                  pid = UNKNOWN_STATE;
               }
            }
#ifndef _WITH_PAUSING
            else
            {
               pid = NOT_RUNNING;

               if (pid_list != NULL)
               {
                  *(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) = 0;
               }

               /* Collect zombie of stopped job. */
               (void)amg_zombie_check(&pid, 0);
            }
#endif
         }

         /* Now store the new time */
         *dc_old_time = stat_buf.st_mtime;

         /* Reread database file */
         for (i = 0; i < no_of_hosts; i++)
         {
            hl[i].in_dir_config = NO;
         }
         if ((no_of_dir = eval_dir_config(stat_buf.st_size)) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not find any valid entries (%d) in database file %s (%s %d)\n",
                      no_of_dir, dir_config_file,
                      __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /*
          * Since there might have been an old FSA which has more
          * information then the HOST_CONFIG lets rewrite this
          * file using the information from both HOST_CONFIG and
          * old FSA. That what is found in the HOST_CONFIG will
          * always have a higher priority.
          */
         *hc_old_time = write_host_config();
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Found %d hosts in HOST_CONFIG.\n",
                   no_of_hosts);

#ifdef _DEBUG
         show_shm(p_debug_file);
#endif

         /* Start, restart or stop jobs */
         if (data_length > 0)
         {
            switch(pid)
            {
               case NOT_RUNNING :
               case DIED :
                  pid = make_process_amg(p_work_dir, shm_id, rescan_time, max_no_proc, no_of_dir);
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) = pid;
                     *(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t))) = shm_id;
                  }
                  break;

               case UNKNOWN_STATE :
                  /* Since we do not no the state, lets just kill it */
                  if (kill(tmp_pid, SIGINT) < 0)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to send kill signal to process %s : %s (%s %d)\n",
                               IT_PROC_NAME, strerror(errno),
                               __FILE__, __LINE__);

                     /* Don't exit here, since the process might */
                     /* have died in the meantime.               */
                  }

                  /* Eliminate zombie of killed job */
                  (void)amg_zombie_check(&tmp_pid, 0);

                  pid = make_process_amg(p_work_dir, shm_id, rescan_time, max_no_proc, no_of_dir);
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((IT_NO + 1) * sizeof(pid_t))) = pid;
                     *(pid_t *)(pid_list + ((NO_OF_PROCESS + 1) * sizeof(pid_t))) = shm_id;
                  }
                  break;

               default :
                  com(START);
                  break;
            }
         }
         else
         {
            if (pid > 0)
            {
               com(STOP);
               pid = NOT_RUNNING;
            }
         }

         /* Tell user we have reread new DIR_CONFIG file */
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Done with rereading DIR_CONFIG.\n");
      }
      else if ((old_hl != NULL) && (new_fsa == NO))
           {
              int  host_order_changed = NO,
                   no_of_host_changed = 0,
                   host_pos,
                   i,
                   j;
              char *mark_list = NULL;

              if (fsa_attach() == INCORRECT)
              {
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "Could not attach to FSA! (%s %d)\n",
                           __FILE__, __LINE__);
                 return;
              }

              /*
               * In the first step lets just update small changes.
               * (Changes where we do no need to rewrite the FSA.
               * That is when the order of hosts has changed.)
               */
              if ((mark_list = malloc(old_no_of_hosts)) == NULL)
              {
                 (void)fsa_detach();
                 (void)rec(sys_log_fd, ERROR_SIGN,
                           "malloc() error : %s (%s %d)\n",
                           strerror(errno), __FILE__, __LINE__);
                 return;
              }
              for (i = 0; i < no_of_hosts; i++)
              {
                 host_pos = INCORRECT;
                 (void)memset(mark_list, NO, old_no_of_hosts);
                 for (j = 0; j < old_no_of_hosts; j++)
                 {
                    if ((mark_list[j] == NO) &&
                        (strcmp(hl[i].host_alias, old_hl[j].host_alias) == 0))
                    {
                       host_pos = j;
                       mark_list[j] = YES;
                       break;
                    }
                 }
                 if (host_pos != INCORRECT)
                 {
                    if (host_pos != i)
                    {
                       host_order_changed = YES;
                    }
                    if (memcmp(&hl[i], &old_hl[host_pos], sizeof(struct host_list)) != 0)
                    {
                       no_of_host_changed++;

                       /*
                        * Some parameters for this host have
                        * changed. Instead of finding the
                        * place where the change took place,
                        * overwrite all parameters.
                        */
                       (void)strcpy(fsa[host_pos].real_hostname[0], hl[i].real_hostname[0]);
                       (void)strcpy(fsa[host_pos].real_hostname[1], hl[i].real_hostname[1]);
                       (void)strcpy(fsa[host_pos].proxy_name, hl[i].proxy_name);
                       fsa[host_pos].allowed_transfers = hl[i].allowed_transfers;
                       if (old_hl[i].allowed_transfers != hl[i].allowed_transfers)
                       {
                          int k;

                          for (k = 0; k < fsa[host_pos].allowed_transfers; k++)
                          {
                             fsa[host_pos].job_status[k].no_of_files = 0;
                             fsa[host_pos].job_status[k].connect_status = DISCONNECT;
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
                             fsa[host_pos].job_status[k].job_id = NO_ID;
#endif
                          }
                          for (k = fsa[host_pos].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
                          {
                             fsa[host_pos].job_status[k].no_of_files = -1;
                          }
                       }
                       fsa[host_pos].max_errors = hl[i].max_errors;
                       fsa[host_pos].retry_interval = hl[i].retry_interval;
                       fsa[host_pos].block_size = hl[i].transfer_blksize;
                       fsa[host_pos].max_successful_retries = hl[i].successful_retries;
                       fsa[host_pos].file_size_offset = hl[i].file_size_offset;
                       fsa[host_pos].transfer_timeout = hl[i].transfer_timeout;
                       fsa[host_pos].special_flag = (fsa[i].special_flag & (~NO_BURST_COUNT_MASK)) | hl[i].number_of_no_bursts;
                    }
                 }
              } /* for (i = 0; i < no_of_hosts; i++) */

              free(mark_list);

              if (no_of_host_changed > 0)
              {
                 (void)rec(sys_log_fd, INFO_SIGN,
                           "%d host changed in HOST_CONFIG.\n",
                           no_of_host_changed);
              }

              /*
               * Now lets see if the host order has changed.
               */
              if (host_order_changed == YES)
              {
                 size_t new_size = no_of_hosts * sizeof(char *);
                 char   **p_host_names = NULL;

                 if ((p_host_names = malloc(new_size)) == NULL)
                 {
                    (void)fsa_detach();
                    (void)rec(sys_log_fd, FATAL_SIGN,
                              "malloc() error [%d Bytes] : %s (%s %d)\n",
                              new_size, strerror(errno),
                              __FILE__, __LINE__);
                    return;
                 }
                 for (i = 0; i < no_of_hosts; i++)
                 {
                    p_host_names[i] = hl[i].host_alias;
                 }
                 (void)rec(sys_log_fd, INFO_SIGN,
                           "Changing host alias order.\n");
                 change_alias_order(p_host_names, -1);
                 free(p_host_names);
              }

              (void)fsa_detach();
           } /* if (old_hl != NULL) */
   }

   if (rewrite_host_config == YES)
   {
      *hc_old_time = write_host_config();
      (void)rec(sys_log_fd, INFO_SIGN,
                "Found %d hosts in HOST_CONFIG.\n",
                no_of_hosts);
   }

   return;
}
