/*
 *  reread_host_config.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 1998 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reread_host_config - reads the HOST_CONFIG file
 **
 ** SYNOPSIS
 **   void reread_host_config(time_t           *hc_old_time,
 **                           int              *old_no_of_hosts,
 **                           int              *rewrite_host_config,
 **                           size_t           *old_size,
 **                           struct host_list **old_hl)
 **
 ** DESCRIPTION
 **   This function reads the HOST_CONFIG file and sets the values
 **   in the FSA.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.01.1998 H.Kiehl Created
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **   19.02.2002 H.Kiehl Stop process dir_check when we reorder the
 **                      FSA.
 **
 */
DESCR__E_M3

#include <string.h>              /* strerror(), memset(), strcmp(),      */
                                 /* memcpy()                             */
#include <stdlib.h>              /* malloc(), free()                     */
#include <sys/stat.h>
#include <sys/wait.h>            /* WNOHANG                              */
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

/* External Global Variables */
extern int                        no_of_hosts,
                                  sys_log_fd;
extern pid_t                      dc_pid;
extern char                       host_config_file[],
                                  *pid_list;
extern struct host_list           *hl;
extern struct filetransfer_status *fsa;


/*########################## reread_host_config() #######################*/
void
reread_host_config(time_t           *hc_old_time,
                   int              *old_no_of_hosts,
                   int              *rewrite_host_config,
                   size_t           *old_size,
                   struct host_list **old_hl)
{
   struct stat stat_buf;

   /* Get the size of the database file */
   if (stat(host_config_file, &stat_buf) < 0)
   {
      if (errno == ENOENT)
      {
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Recreating HOST_CONFIG file with %d hosts.\n",
                   no_of_hosts);
         *hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Could not stat() HOST_CONFIG file %s : %s (%s %d)\n",
                   host_config_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
   }

   /* Check if HOST_CONFIG has changed */
   if (*hc_old_time < stat_buf.st_mtime)
   {
      int              dir_check_stopped = NO,
                       dummy_1,
                       dummy_2,
                       host_order_changed = NO,
                       host_pos,
                       i,
                       j,
                       new_no_of_hosts,
                       no_of_host_changed = 0;
      size_t           dummy_3;
      char             *mark_list = NULL;
      struct host_list *dummy_hl;

      if (old_no_of_hosts == NULL)
      {
         old_no_of_hosts = &dummy_1;
      }
      if (rewrite_host_config == NULL)
      {
         rewrite_host_config = &dummy_2;
      }
      if (old_size == NULL)
      {
         old_size = &dummy_3;
      }
      if (old_hl == NULL)
      {
         old_hl = &dummy_hl;
      }

      /* Tell user we have to reread the new HOST_CONFIG file. */
      (void)rec(sys_log_fd, INFO_SIGN, "Rereading HOST_CONFIG...\n");

      /* Now store the new time */
      *hc_old_time = stat_buf.st_mtime;

      /* Reread HOST_CONFIG file */
      if (hl != NULL)
      {
         *old_size = no_of_hosts * sizeof(struct host_list);

         if ((*old_hl = malloc(*old_size)) == NULL)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "malloc() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            return;
         }
         (void)memcpy(*old_hl, hl, *old_size);
         *old_no_of_hosts = no_of_hosts;
         free(hl);
         hl = NULL;
      }

      /*
       * Careful! The function eval_host_config() and fsa_attach()
       * will overwrite no_of_hosts! Store it somewhere save.
       */
      *rewrite_host_config = eval_host_config(&no_of_hosts, host_config_file,
                                              &hl, NO);
      new_no_of_hosts = no_of_hosts;
      if (fsa_attach() == INCORRECT)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not attach to FSA! (%s %d)\n", __FILE__, __LINE__);
         exit(INCORRECT);
      }

      /*
       * In the first step lets just update small changes.
       * (Changes where we do not need to rewrite the FSA.
       * That is when the order of hosts has changed.)
       */
      if ((mark_list = malloc(*old_no_of_hosts)) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)memset(mark_list, NO, *old_no_of_hosts);
      for (i = 0; i < no_of_hosts; i++)
      {
         host_pos = INCORRECT;
         for (j = 0; j < *old_no_of_hosts; j++)
         {
            if ((mark_list[j] == NO) &&
                (CHECK_STRCMP(hl[i].host_alias, (*old_hl)[j].host_alias) == 0))
            {
               host_pos = j;
               mark_list[j] = YES;

               /*
                * At this stage we cannot know what protocols are used
                * or if the host is in the DIR_CONFIG.
                * So lets copy them from the old host list. This value
                * can only change when the DIR_CONFIG changes. The
                * function eval_dir_config() will take care if this
                * is the case.
                */
               hl[i].protocol = (*old_hl)[j].protocol;
               hl[i].in_dir_config = (*old_hl)[j].in_dir_config;
               break;
            }
         }
         if (host_pos != INCORRECT)
         {
            if (host_pos != i)
            {
               host_order_changed = YES;
               if ((dc_pid > 0) && (dir_check_stopped == NO))
               {
                  int options;

                  if (com(STOP) == INCORRECT)
                  {
                     options = WNOHANG;
                  }
                  else
                  {
                     options = 0;
                  }
                  (void)amg_zombie_check(&dc_pid, options);
                  dc_pid = NOT_RUNNING;
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = 0;
                  }
                  dir_check_stopped = YES;
               }
            }
            if (memcmp(&hl[i], (&(*old_hl)[host_pos]), sizeof(struct host_list)) != 0)
            {
               no_of_host_changed++;

               /*
                * Some parameters for this host have changed. Instead of
                * finding the place where the change took place, overwrite
                * all parameters.
                */
               (void)strcpy(fsa[host_pos].real_hostname[0], hl[i].real_hostname[0]);
               (void)strcpy(fsa[host_pos].real_hostname[1], hl[i].real_hostname[1]);
               if (CHECK_STRCMP(hl[i].host_toggle_str, (*old_hl)[host_pos].host_toggle_str) != 0)
               {
                  if (hl[i].host_toggle_str[0] == '\0')
                  {
                     fsa[host_pos].host_toggle_str[0] = '\0';
                     fsa[host_pos].host_dsp_name[(int)fsa[host_pos].toggle_pos] = ' ';
                     fsa[host_pos].original_toggle_pos = NONE;
                  }
                  else
                  {
                     if ((*old_hl)[host_pos].host_toggle_str[0] == '\0')
                     {
                        fsa[host_pos].toggle_pos = strlen(fsa[host_pos].host_alias);
                     }
                     (void)memcpy(fsa[host_pos].host_toggle_str, hl[i].host_toggle_str, 5);
                     if ((hl[i].host_toggle_str[HOST_ONE] != (*old_hl)[host_pos].host_toggle_str[HOST_ONE]) ||
                         (hl[i].host_toggle_str[HOST_TWO] != (*old_hl)[host_pos].host_toggle_str[HOST_TWO]))
                     {
                        fsa[host_pos].host_toggle_str[HOST_ONE] = hl[i].host_toggle_str[HOST_ONE];
                        fsa[host_pos].host_toggle_str[HOST_TWO] = hl[i].host_toggle_str[HOST_TWO];
                        fsa[host_pos].host_dsp_name[(int)fsa[host_pos].toggle_pos] = fsa[host_pos].host_toggle_str[(int)fsa[host_pos].host_toggle];
                     }
                     if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
                     {
                        fsa[host_pos].auto_toggle = ON;
                     }
                     else
                     {
                        fsa[host_pos].auto_toggle = OFF;
                     }
                  }
               }
               (void)strcpy(fsa[host_pos].proxy_name, hl[i].proxy_name);
               fsa[host_pos].allowed_transfers = hl[i].allowed_transfers;
               if ((*old_hl)[host_pos].allowed_transfers != hl[i].allowed_transfers)
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
               fsa[host_pos].special_flag = (fsa[host_pos].special_flag & (~NO_BURST_COUNT_MASK)) | hl[i].number_of_no_bursts;
               if (hl[i].special_flag & FTP_PASSIVE_MODE)
               {
                  fsa[host_pos].protocol |= FTP_PASSIVE_MODE;
               }
               else
               {
                  fsa[host_pos].protocol &= ~FTP_PASSIVE_MODE;
               }
               if (hl[i].special_flag & SET_IDLE_TIME)
               {
                  fsa[host_pos].protocol |= SET_IDLE_TIME;
               }
               else
               {
                  fsa[host_pos].protocol &= ~SET_IDLE_TIME;
               }
               if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
               {
                  fsa[host_pos].special_flag |= HOST_DISABLED;
               }
               else
               {
                  fsa[host_pos].special_flag &= ~HOST_DISABLED;
               }
               if (hl[i].host_status & STOP_TRANSFER_STAT)
               {
                  fsa[host_pos].host_status |= STOP_TRANSFER_STAT;
               }
               else
               {
                  fsa[host_pos].host_status &= ~STOP_TRANSFER_STAT;
               }
               if (hl[i].host_status & PAUSE_QUEUE_STAT)
               {
                  fsa[host_pos].host_status |= PAUSE_QUEUE_STAT;
               }
               else
               {
                  fsa[host_pos].host_status &= ~PAUSE_QUEUE_STAT;
               }
            }
         }
         else /* host_pos == INCORRECT */
         {
            /*
             * Since we cannot find this host in the old host list
             * this must be a brand new host. The current FSA is to
             * small to add the new host here so we have to do it
             * in the function change_alias_order().
             */
            host_order_changed = YES;
         }
      } /* for (i = 0; i < no_of_hosts; i++) */

      if (host_order_changed != YES)
      {
         if (new_no_of_hosts != no_of_hosts)
         {
            host_order_changed = YES;
         }
         else
         {
            /*
             * Before freeing the mark_list, check if all hosts are marked.
             * If not a host has been removed from the FSA. Mark host order
             * as changed so the function change_alias_order() can remove
             * the host.
             */
            for (j = 0; j < *old_no_of_hosts; j++)
            {
               if (mark_list[j] == NO)
               {
                  host_order_changed = YES;
                  break;
               }
            }
         }
      }
      free(mark_list);

      if (no_of_host_changed > 0)
      {
         (void)rec(sys_log_fd, INFO_SIGN,
                   "%d host changed in HOST_CONFIG.\n", no_of_host_changed);
      }

      /*
       * Now lets see if the host order has changed.
       */
      if (host_order_changed == YES)
      {
         size_t new_size;
         char   dummy_name[1],
                **p_host_names = NULL;

         if (new_no_of_hosts > no_of_hosts)
         {
            new_size = new_no_of_hosts * sizeof(char *);
         }
         else
         {
            new_size = no_of_hosts * sizeof(char *);
            dummy_name[0] = '\0';
         }

         if ((p_host_names = malloc(new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "malloc() error [%d Bytes] : %s (%s %d)\n",
                      new_size, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         for (i = 0; i < new_no_of_hosts; i++)
         {
            p_host_names[i] = hl[i].host_alias;
         }
         for (i = new_no_of_hosts; i < no_of_hosts; i++)
         {
            p_host_names[i] = dummy_name;
         }

         if ((dc_pid > 0) && (dir_check_stopped == NO))
         {
            int options;

            if (com(STOP) == INCORRECT)
            {
               options = WNOHANG;
            }
            else
            {
               options = 0;
            }
            (void)amg_zombie_check(&dc_pid, options);
            dc_pid = NOT_RUNNING;
            if (pid_list != NULL)
            {
               *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = 0;
            }
            dir_check_stopped = YES;
         }
         (void)rec(sys_log_fd, INFO_SIGN, "Changing host alias order.\n");
         change_alias_order(p_host_names, new_no_of_hosts);
         free(p_host_names);
      }

      (void)fsa_detach(YES);

      if (old_hl != NULL)
      {
         free(*old_hl);
      }
   }
   else
   {
      (void)rec(sys_log_fd, INFO_SIGN,
                "There is no change in the HOST_CONFIG file. (%s %d)\n",
                __FILE__, __LINE__);
   }

   return;
}
