/*
 *  create_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_fsa - creates the FSA of the AFD
 **
 ** SYNOPSIS
 **   void create_fsa(void)
 **
 ** DESCRIPTION
 **   This function creates the FSA (Filetransfer Status Area), to
 **   which all process of the AFD will map. The FSA has the following
 **   structure:
 **
 **      <int no_of_hosts><struct filetransfer_status fsa[no_of_hosts]>
 **
 **   A detailed description of the structure filetransfer_status can
 **   be found in afddefs.h. The signed integer variable no_of_hosts
 **   contains the number of hosts that the AFD has to serve. This
 **   variable can have the value STALE (-1), which will tell all other
 **   process to unmap from this area and map to the new area.
 **
 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strlen(), strcmp(), strcpy(),     */
                                    /* strerror()                        */
#include <stdlib.h>                 /* calloc(), free()                  */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>               /* mmap(), munmap()                  */
#endif
#include <unistd.h>                 /* read(), write(), close(), lseek() */
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables */
extern char                       *p_work_dir;
extern int                        sys_log_fd,
                                  fsa_id,
                                  fsa_fd,
                                  no_of_hosts; /* The number of remote/  */
                                               /* local hosts to which   */
                                               /* files have to be       */
                                               /* transfered.            */
extern off_t                      fsa_size;
extern struct host_list           *hl;         /* Structure that holds   */
                                               /* all the hosts.         */
extern struct filetransfer_status *fsa;

/* Local global variables */
struct afd_status                 *p_afd_status; /* Used by attach_afd_status() */


/*############################ create_fsa() #############################*/
void
create_fsa(void)
{
   int                        i,
                              k,
                              fd,
                              old_fsa_fd = -1,
                              old_fsa_id,
                              old_no_of_hosts = -1,
                              afd_cmd_fd,
                              size;
   static int                 first_time = YES;
   off_t                      old_fsa_size = -1;
   char                       *ptr = NULL,
#ifdef _FIFO_DEBUG
                              cmd[2],
#endif
                              new_fsa_stat[MAX_PATH_LENGTH],
                              old_fsa_stat[MAX_PATH_LENGTH],
                              afd_cmd_fifo[MAX_PATH_LENGTH],
                              fsa_id_file[MAX_PATH_LENGTH];
   struct filetransfer_status *old_fsa = NULL;
   struct flock               wlock = {F_WRLCK, SEEK_SET, 0, 1},
                              ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat                stat_buf;

   fsa_size = -1;

   /* Initialise all pathnames and file descriptors */
   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcpy(afd_cmd_fifo, fsa_id_file);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);
   (void)strcpy(old_fsa_stat, fsa_id_file);
   (void)strcat(old_fsa_stat, FSA_STAT_FILE);

   (void)strcat(fsa_id_file, FSA_ID_FILE);

   /*
    * Check if fifos have been created. If not create and open them.
    */
   if ((stat(afd_cmd_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(afd_cmd_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to create fifo %s. (%s %d)\n",
                   afd_cmd_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * First just try open the fsa_id_file. If this fails create
    * the file and initialise old_fsa_id with -1.
    */
   if ((fd = open(fsa_id_file, O_RDWR)) > -1)
   {
      /*
       * Lock FSA ID file. If it is already locked
       * (by edit_hc dialog) wait for it to clear the lock
       * again.
       */
      if (fcntl(fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN))
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not set write lock for %s : %s (%s %d)\n",
                      fsa_id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      /* Read the FSA file ID */
      if (read(fd, &old_fsa_id, sizeof(int)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not read the value of the FSA file ID : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      if ((fd = open(fsa_id_file, (O_RDWR | O_CREAT | O_TRUNC),
                     (S_IRUSR | S_IWUSR))) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open %s : %s (%s %d)\n",
                   fsa_id_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      old_fsa_id = -1;
   }

   /*
    * We now have to determine if this is the first time that the
    * AMG is being run. The only way to find this out is to see
    * whether the startup time of the AFD has been set.  If it is
    * not set (i.e. 0), it is the first time.
    */
   if (first_time == YES)
   {
      if (attach_afd_status() < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to attach to AFD status shared area. (%s %d)\n",
                   __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (p_afd_status->start_time > 0)
      {
         first_time = NO;
      }
      else
      {
         first_time = YES;
      }
#ifdef _NO_MMAP
      if (munmap_emu((void *)(p_afd_status)) == -1)
      {
         char afd_status_file[MAX_PATH_LENGTH];

         (void)strcpy(afd_status_file, p_work_dir);
         (void)strcat(afd_status_file, FIFO_DIR);
         (void)strcat(afd_status_file, STATUS_SHMID_FILE);
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap() %s : %s (%s %d)\n",
                   afd_status_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#else
      {
         char        afd_status_file[MAX_PATH_LENGTH];
         struct stat stat_buf;

         (void)strcpy(afd_status_file, p_work_dir);
         (void)strcat(afd_status_file, FIFO_DIR);
         (void)strcat(afd_status_file, STATUS_SHMID_FILE);
         if (stat(afd_status_file, &stat_buf) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to fstat() %s : %s (%s %d)\n",
                      afd_status_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         if (munmap((void *)(p_afd_status), stat_buf.st_size) == -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      afd_status_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
#endif /* _NO_MMAP */
   }

   /*
    * Mark memory mapped region as old, so no process puts
    * any new information into the region after we
    * have copied it into the new region.
    */
   if (old_fsa_id > -1)
   {
      /* Attach to old region */
      ptr = old_fsa_stat + strlen(old_fsa_stat);
      (void)sprintf(ptr, ".%d", old_fsa_id);

      /* Get the size of the old FSA file. */
      if (stat(old_fsa_stat, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   old_fsa_stat, strerror(errno), __FILE__, __LINE__);
         old_fsa_id = -1;
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
            if ((old_fsa_fd = open(old_fsa_stat, O_RDWR)) < 0)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         old_fsa_stat, strerror(errno), __FILE__, __LINE__);
               old_fsa_id = old_fsa_fd = -1;
            }
            else
            {
#ifdef _NO_MMAP
               if ((ptr = mmap_emu(0, stat_buf.st_size,
                                   (PROT_READ | PROT_WRITE),
                                   MAP_SHARED, old_fsa_stat, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, old_fsa_fd, 0)) == (caddr_t) -1)
#endif
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "mmap() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  old_fsa_id = -1;
               }
               else
               {
                  if (*(int *)ptr == STALE)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "FSA in %s is stale! Ignoring this FSA. (%s %d)\n",
                               old_fsa_stat, __FILE__, __LINE__);
                     old_fsa_id = -1;
                  }
                  else
                  {
                     old_fsa_size = stat_buf.st_size;
                  }

                  /*
                   * We actually could remove the old file now. Better
                   * do it when we are done with it.
                   */
               }

               /*
                * Do NOT close the old file! Else some file system
                * optimisers (like fsr in Irix 5.x) move the contents
                * of the memory mapped file!
                */
            }
         }
         else
         {
            old_fsa_id = -1;
         }
      }

      if (old_fsa_id != -1)
      {
         old_no_of_hosts = *(int *)ptr;

         /* Now mark it as stale */
         *(int *)ptr = STALE;

         /* Move pointer to correct position so */
         /* we can extract the relevant data.   */
         ptr += AFD_WORD_OFFSET;

         old_fsa = (struct filetransfer_status *)ptr;
      }
   }

   /*
    * Create the new mmap region.
    */
   /* First calculate the new size */
   fsa_size = AFD_WORD_OFFSET +
              (no_of_hosts * sizeof(struct filetransfer_status));

   if ((old_fsa_id + 1) > -1)
   {
      fsa_id = old_fsa_id + 1;
   }
   else
   {
      fsa_id = 0;
   }
   (void)sprintf(new_fsa_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, FSA_STAT_FILE, fsa_id);

   /* Now map the new FSA region to a file */
   if ((fsa_fd = open(new_fsa_stat, (O_RDWR | O_CREAT | O_TRUNC),
                      FILE_MODE)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (lseek(fsa_fd, fsa_size - 1, SEEK_SET) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to lseek() in %s : %s (%s %d)\n",
                new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(fsa_fd, "", 1) != 1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef _NO_MMAP
   if ((ptr = mmap_emu(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fsa_stat, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   fsa_fd, 0)) == (caddr_t) -1)
#endif
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "mmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write number of hosts to new memory mapped region */
   *(int*)ptr = no_of_hosts;

   /* Initialize HOST_CONFIG counter. */
   *(unsigned char *)(ptr + sizeof(int)) = 0;

   /* Reposition fsa pointer after no_of_hosts */
   ptr += AFD_WORD_OFFSET;
   fsa = (struct filetransfer_status *)ptr;

   /*
    * Copy all the old and new data into the new mapped region.
    */
   size = MAX_NO_PARALLEL_JOBS * sizeof(struct status);
   if (old_fsa_id < 0)
   {
      /*
       * There is NO old FSA.
       */
      for (i = 0; i < no_of_hosts; i++)
      {
         (void)strcpy(fsa[i].host_alias, hl[i].host_alias);
         (void)strcpy(fsa[i].real_hostname[0], hl[i].real_hostname[0]);
         (void)strcpy(fsa[i].real_hostname[1], hl[i].real_hostname[1]);
         (void)strcpy(fsa[i].proxy_name, hl[i].proxy_name);
         fsa[i].allowed_transfers      = hl[i].allowed_transfers;
         fsa[i].max_errors             = hl[i].max_errors;
         fsa[i].retry_interval         = hl[i].retry_interval;
         fsa[i].block_size             = hl[i].transfer_blksize;
         fsa[i].max_successful_retries = hl[i].successful_retries;
         fsa[i].file_size_offset       = hl[i].file_size_offset;
         fsa[i].transfer_timeout       = hl[i].transfer_timeout;
         fsa[i].protocol               = hl[i].protocol;
         fsa[i].special_flag           = 0;
         if (hl[i].in_dir_config == YES)
         {
            fsa[i].special_flag |= HOST_IN_DIR_CONFIG;
         }
         if (hl[i].number_of_no_bursts > 0)
         {
            fsa[i].special_flag = (fsa[i].special_flag & (~NO_BURST_COUNT_MASK)) | hl[i].number_of_no_bursts;
         }

         /* Determine the host name to display */
         fsa[i].host_toggle         = DEFAULT_TOGGLE_HOST;
         fsa[i].original_toggle_pos = NONE;
         (void)strcpy(fsa[i].host_dsp_name, fsa[i].host_alias);
         fsa[i].toggle_pos = strlen(fsa[i].host_alias);
         if (hl[i].host_toggle_str[0] == '\0')
         {
            fsa[i].host_toggle_str[0]  = '\0';
            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[0], hl[i].fullname);
               (void)strcpy(hl[i].real_hostname[0], hl[i].fullname);
            }
         }
         else
         {
            (void)strcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str);
            if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
            {
               fsa[i].auto_toggle = ON;
            }
            else
            {
               fsa[i].auto_toggle = OFF;
            }
            fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
            fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
               (void)strcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0]);
            }
            if (fsa[i].real_hostname[1][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
               if (fsa[i].host_toggle == HOST_ONE)
               {
                  fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
               }
               else
               {
                  fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
               }
               (void)strcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1]);
            }
         }
         (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

         fsa[i].host_status         = 0;
         fsa[i].error_counter       = 0;
         fsa[i].total_errors        = 0;
         fsa[i].total_connect_time  = 0;
         fsa[i].file_counter_done   = 0;
         fsa[i].bytes_send          = 0;
         fsa[i].connections         = 0;
         fsa[i].active_transfers    = 0;
         fsa[i].transfer_rate       = 0;
         fsa[i].successful_retries  = 0;
         fsa[i].debug               = NO;
         fsa[i].last_connection = fsa[i].last_retry_time = time(NULL);
         (void)memset(&fsa[i].job_status, 0, size);
         for (k = 0; k < fsa[i].allowed_transfers; k++)
         {
            fsa[i].job_status[k].connect_status = DISCONNECT;
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
            fsa[i].job_status[k].job_id = NO_ID;
#endif
         }
         for (k = fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
         {
            fsa[i].job_status[k].no_of_files = -1;
         }
      } /* for (i = 0; i < no_of_hosts; i++) */
   }
   else /* There is an old database file. */
   {
      int  host_pos,
           no_of_gotchas = 0;
      char *gotcha = NULL;

      /*
       * The gotcha array is used to find hosts that are in the
       * old FSA but not in the HOST_CONFIG file.
       */
      if ((gotcha = malloc(old_no_of_hosts)) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      (void)memset(gotcha, NO, old_no_of_hosts);

      for (i = 0; i < no_of_hosts; i++)
      {
         (void)strcpy(fsa[i].host_alias, hl[i].host_alias);
         (void)strcpy(fsa[i].real_hostname[0], hl[i].real_hostname[0]);
         (void)strcpy(fsa[i].real_hostname[1], hl[i].real_hostname[1]);
         (void)strcpy(fsa[i].proxy_name, hl[i].proxy_name);
         fsa[i].allowed_transfers      = hl[i].allowed_transfers;
         fsa[i].max_errors             = hl[i].max_errors;
         fsa[i].retry_interval         = hl[i].retry_interval;
         fsa[i].block_size             = hl[i].transfer_blksize;
         fsa[i].max_successful_retries = hl[i].successful_retries;
         fsa[i].file_size_offset       = hl[i].file_size_offset;
         fsa[i].transfer_timeout       = hl[i].transfer_timeout;
         fsa[i].protocol               = hl[i].protocol;

         /*
          * Search in the old FSA for this host. If it is there use
          * the values from the old FSA or else initialise them with
          * defaults. When we find an old entry, remember this so we
          * can later check if there are entries in the old FSA but
          * there are no corresponding entries in the HOST_CONFIG.
          * This will then have to be updated in the HOST_CONFIG file.
          */
         host_pos = INCORRECT;
         for (k = 0; k < old_no_of_hosts; k++)
         {
            if (gotcha[k] != YES)
            {
               if (strcmp(old_fsa[k].host_alias, hl[i].host_alias) == 0)
               {
                  host_pos = k;
                  break;
               }
            }
         }

         if (host_pos != INCORRECT)
         {
            no_of_gotchas++;
            gotcha[host_pos] = YES;

            /*
             * When restarting the AMG and we did change the switching
             * information we will not recognise this change. Thus we have
             * to always evaluate the host name :-(
             */
            fsa[i].host_toggle = old_fsa[host_pos].host_toggle;
            (void)strcpy(fsa[i].host_dsp_name, fsa[i].host_alias);
            fsa[i].toggle_pos = strlen(fsa[i].host_alias);
            if (hl[i].host_toggle_str[0] == '\0')
            {
               fsa[i].host_toggle_str[0]  = '\0';
               fsa[i].original_toggle_pos = NONE;
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], hl[i].fullname);
                  (void)strcpy(hl[i].real_hostname[0], hl[i].fullname);
               }
            }
            else
            {
               (void)strcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str);
               if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
               {
                  fsa[i].auto_toggle = ON;
                  if (old_fsa[host_pos].original_toggle_pos == NONE)
                  {
                     fsa[i].successful_retries  = 0;
                  }
                  else
                  {
                     fsa[i].successful_retries  = old_fsa[host_pos].successful_retries;
                  }
               }
               else
               {
                  fsa[i].auto_toggle = OFF;
                  fsa[i].original_toggle_pos = NONE;
                  fsa[i].successful_retries  = 0;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
               fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
                  (void)strcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0]);
               }
               if (fsa[i].real_hostname[1][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
                  }
                  else
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
                  }
                  (void)strcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1]);
               }
            }
            (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

            if (fsa[i].real_hostname[0][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[0], old_fsa[host_pos].real_hostname[0]);
               (void)strcpy(hl[i].real_hostname[0], old_fsa[host_pos].real_hostname[0]);
            }
            if (fsa[i].real_hostname[1][0] == '\0')
            {
               (void)strcpy(fsa[i].real_hostname[1], old_fsa[host_pos].real_hostname[1]);
               (void)strcpy(hl[i].real_hostname[1], old_fsa[host_pos].real_hostname[1]);
            }
            fsa[i].host_status         = old_fsa[host_pos].host_status;
            fsa[i].error_counter       = old_fsa[host_pos].error_counter;
            fsa[i].total_errors        = old_fsa[host_pos].total_errors;
            fsa[i].total_connect_time  = old_fsa[host_pos].total_connect_time;
            fsa[i].file_counter_done   = old_fsa[host_pos].file_counter_done;
            fsa[i].bytes_send          = old_fsa[host_pos].bytes_send;
            fsa[i].connections         = old_fsa[host_pos].connections;
            fsa[i].active_transfers    = old_fsa[host_pos].active_transfers;
            fsa[i].transfer_rate       = old_fsa[host_pos].transfer_rate;
            fsa[i].last_connection     = old_fsa[host_pos].last_connection;
            fsa[i].last_retry_time     = old_fsa[host_pos].last_retry_time;
            fsa[i].total_file_counter  = old_fsa[host_pos].total_file_counter;
            fsa[i].total_file_size     = old_fsa[host_pos].total_file_size;
            fsa[i].debug               = old_fsa[host_pos].debug;
            fsa[i].special_flag        = old_fsa[host_pos].special_flag;
            fsa[i].original_toggle_pos = old_fsa[host_pos].original_toggle_pos;
            if (fsa[i].special_flag & ERROR_FILE_UNDER_PROCESS)
            {
               /* Error job is no longer under process. */
               fsa[i].special_flag ^= ERROR_FILE_UNDER_PROCESS;
            }

            /* Copy all job entries. */
            (void)memcpy(&fsa[i].job_status, &old_fsa[host_pos].job_status, size);

            for (k = 0; k < fsa[i].allowed_transfers; k++)
            {
               fsa[i].job_status[k].no_of_files_done = 0;
               fsa[i].job_status[k].file_size_done = 0L;
            }
         }
         else /* This host is not in the old FSA, therefor it is new. */
         {
            fsa[i].host_toggle         = DEFAULT_TOGGLE_HOST;
            fsa[i].original_toggle_pos = NONE;
            (void)strcpy(fsa[i].host_dsp_name, fsa[i].host_alias);
            fsa[i].toggle_pos = strlen(fsa[i].host_alias);
            if (hl[i].host_toggle_str[0] == '\0')
            {
               fsa[i].host_toggle_str[0]  = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], hl[i].fullname);
                  (void)strcpy(hl[i].real_hostname[0], hl[i].fullname);
               }
            }
            else
            {
               (void)strcpy(fsa[i].host_toggle_str, hl[i].host_toggle_str);
               if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
               {
                  fsa[i].auto_toggle = ON;
               }
               else
               {
                  fsa[i].auto_toggle = OFF;
               }
               fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
               fsa[i].host_dsp_name[(int)(fsa[i].toggle_pos + 1)] = '\0';
               if (fsa[i].real_hostname[0][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[0], fsa[i].host_dsp_name);
                  (void)strcpy(hl[i].real_hostname[0], fsa[i].real_hostname[0]);
               }
               if (fsa[i].real_hostname[1][0] == '\0')
               {
                  (void)strcpy(fsa[i].real_hostname[1], fsa[i].host_dsp_name);
                  if (fsa[i].host_toggle == HOST_ONE)
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_TWO];
                  }
                  else
                  {
                     fsa[i].host_dsp_name[(int)fsa[i].toggle_pos] = fsa[i].host_toggle_str[HOST_ONE];
                  }
                  (void)strcpy(hl[i].real_hostname[1], fsa[i].real_hostname[1]);
               }
            }
            (void)memset(&hl[i].fullname[0], 0, MAX_FILENAME_LENGTH);

            fsa[i].host_status         = 0;
            fsa[i].error_counter       = 0;
            fsa[i].total_errors        = 0;
            fsa[i].total_connect_time  = 0;
            fsa[i].file_counter_done   = 0;
            fsa[i].bytes_send          = 0;
            fsa[i].connections         = 0;
            fsa[i].active_transfers    = 0;
            fsa[i].transfer_rate       = 0;
            fsa[i].total_file_counter  = 0;
            fsa[i].total_file_size     = 0;
            fsa[i].special_flag        = 0;
            if (hl[i].number_of_no_bursts > 0)
            {
               fsa[i].special_flag = (fsa[i].special_flag & (~NO_BURST_COUNT_MASK)) | hl[i].number_of_no_bursts;
            }
            fsa[i].successful_retries  = 0;
            fsa[i].debug               = NO;
            fsa[i].last_connection = fsa[i].last_retry_time = time(NULL);
            memset(&fsa[i].job_status, 0, size);
            for (k = 0; k < fsa[i].allowed_transfers; k++)
            {
               fsa[i].job_status[k].connect_status = DISCONNECT;
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
               fsa[i].job_status[k].job_id = NO_ID;
#endif
            }
            for (k = fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
            {
               fsa[i].job_status[k].no_of_files = -1;
            }
         }

         if (hl[i].in_dir_config == YES)
         {
            fsa[i].special_flag |= HOST_IN_DIR_CONFIG;
         }
         else
         {
            if (fsa[i].special_flag & HOST_IN_DIR_CONFIG)
            {
               fsa[i].special_flag ^= HOST_IN_DIR_CONFIG;
            }
         }
      } /* for (i = 0; i < no_of_hosts; i++) */

      /*
       * Check if there is a host entry in the old FSA but NOT in
       * the HOST_CONFIG.
       */
      if (gotcha != NULL)
      {
         if (no_of_gotchas != old_no_of_hosts)
         {
            int no_of_new_old_hosts = old_no_of_hosts - no_of_gotchas,
                j;

            /*
             * It could be that some of the new old hosts should be
             * deleted. The only way to find this out is to see
             * if they still have files to be send.
             */
            for (j = 0; j < old_no_of_hosts; j++)
            {
               if ((gotcha[j] == NO) &&
                   (old_fsa[j].total_file_counter == 0))
               {
                  /* This host is to be removed! */
                  no_of_new_old_hosts--;
                  gotcha[j] = YES;
               }
            }

            if (no_of_new_old_hosts > 0)
            {
               fsa_size += (no_of_new_old_hosts * sizeof(struct filetransfer_status));
               no_of_hosts += no_of_new_old_hosts;

               /*
                * Resize the host_list structure if necessary.
                */
               if ((no_of_hosts % HOST_BUF_SIZE) == 0)
               {
                  size_t new_size,
                         offset;

                  /* Calculate new size for host list */
                  new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                             HOST_BUF_SIZE * sizeof(struct host_list);

                  /* Now increase the space */
                  if ((hl = realloc(hl, new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Could not reallocate memory for host list : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }

                  /* Initialise the new memory area */
                  new_size = HOST_BUF_SIZE * sizeof(struct host_list);
                  offset = (no_of_hosts / HOST_BUF_SIZE) * new_size;
                  memset((char *)hl + offset, 0, new_size);
               }

               /*
                * We now have to make the FSA and host_list structure
                * larger to store the 'new' hosts.
                */
               ptr = (char *)fsa;
               ptr -= AFD_WORD_OFFSET;
               if (fsa_size > 0)
               {
#ifdef _NO_MMAP
                  if (msync_emu(ptr) == -1)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "msync_emu() error (%s %d)\n",
                               __FILE__, __LINE__);
                  }
                  if (munmap_emu(ptr) == -1)
#else
                  if (munmap(ptr, fsa_size) == -1)
#endif
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Failed to munmap() %s : %s (%s %d)\n",
                               new_fsa_stat, strerror(errno),
                               __FILE__, __LINE__);
                  }
               }
               if (lseek(fsa_fd, fsa_size - 1, SEEK_SET) == -1)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Failed to lseek() in %s : %s (%s %d)\n",
                            new_fsa_stat, strerror(errno),
                            __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               if (write(fsa_fd, "", 1) != 1)
               {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "write() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
#ifdef _NO_MMAP
               if ((ptr = mmap_emu(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                                   new_fsa_stat, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                               fsa_fd, 0)) == (caddr_t) -1)
#endif
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "mmap() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }

               /* Write new number of hosts to memory mapped region */
               *(int*)ptr = no_of_hosts;

               /* Reposition fsa pointer after no_of_hosts */
               ptr += AFD_WORD_OFFSET;
               fsa = (struct filetransfer_status *)ptr;

               /*
                * Insert the 'new' old hosts.
                */
               for (j = 0; j < old_no_of_hosts; j++)
               {
                  if (gotcha[j] == NO)
                  {
                     /* Position the new host there were it was in the old FSA. */
                     if (j < i)
                     {
                        size_t move_size = (i - j) * sizeof(struct filetransfer_status);

                        (void)memmove(&fsa[j + 1], &fsa[j], move_size);

                        move_size = (i - j) * sizeof(struct host_list);
                        (void)memmove(&hl[j + 1], &hl[j], move_size);
                     }

                     /* Insert 'new' old host in FSA. */
                     (void)memcpy(&fsa[j], &old_fsa[j], sizeof(struct filetransfer_status));
                     if (fsa[j].special_flag & ERROR_FILE_UNDER_PROCESS)
                     {
                        /* Error job is no longer under process. */
                        fsa[j].special_flag ^= ERROR_FILE_UNDER_PROCESS;
                     }
                     for (k = 0; k < fsa[j].allowed_transfers; k++)
                     {
                        if (fsa[j].job_status[k].no_of_files == -1)
                        {
                           fsa[j].job_status[k].no_of_files = 0;
                           fsa[j].job_status[k].connect_status = DISCONNECT;
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
                           fsa[j].job_status[k].job_id = NO_ID;
#endif
                        }
                     }
                     for (k = fsa[j].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
                     {
                        fsa[j].job_status[k].no_of_files = -1;
                     }

                     /* Insert 'new' old host in host_list structure. */
                     (void)strcpy(hl[j].host_alias, fsa[j].host_alias);
                     (void)strcpy(hl[j].real_hostname[0], fsa[j].real_hostname[0]);
                     (void)strcpy(hl[j].real_hostname[1], fsa[j].real_hostname[1]);
                     (void)strcpy(hl[j].proxy_name, fsa[j].proxy_name);
                     (void)memset(&hl[j].fullname[0], 0, MAX_FILENAME_LENGTH);
                     hl[j].allowed_transfers   = fsa[j].allowed_transfers;
                     hl[j].max_errors          = fsa[j].max_errors;
                     hl[j].retry_interval      = fsa[j].retry_interval;
                     hl[j].transfer_blksize    = fsa[j].block_size;
                     hl[j].successful_retries  = fsa[j].max_successful_retries;
                     hl[j].file_size_offset    = fsa[j].file_size_offset;
                     hl[j].transfer_timeout    = fsa[j].transfer_timeout;
                     hl[j].number_of_no_bursts = fsa[j].special_flag & NO_BURST_COUNT_MASK;
                     hl[j].in_dir_config       = NO;
                     if (fsa[j].special_flag & HOST_IN_DIR_CONFIG)
                     {
                        fsa[j].special_flag ^= HOST_IN_DIR_CONFIG;
                     }

                     i++;
                  } /* if (gotcha[j] == NO) */
               } /* for (j = 0; j < old_no_of_hosts; j++) */
            } /* if (no_of_new_old_hosts > 0) */
         }

         free(gotcha);
      }
   }

   /* Reposition fsa pointer after no_of_hosts */
   ptr = (char *)fsa;
   ptr -= AFD_WORD_OFFSET;
   if (fsa_size > 0)
   {
#ifdef _NO_MMAP
      if (msync_emu(ptr) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "msync_emu() error (%s %d)\n",
                   __FILE__, __LINE__);
      }
      if (munmap_emu(ptr) == -1)
#else
      if (munmap(ptr, fsa_size) == -1)
#endif
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap() %s : %s (%s %d)\n",
                   new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      }
   }

   /*
    * Unmap from old memory mapped region.
    */
   if (first_time == NO)
   {
      ptr = (char *)old_fsa;
      ptr -= AFD_WORD_OFFSET;

      /* Don't forget to unmap old FSA file. */
      if (old_fsa_size > 0)
      {
#ifdef _NO_MMAP
         if (munmap_emu(ptr) == -1)
#else
         if (munmap(ptr, old_fsa_size) == -1)
#endif
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      old_fsa_stat, strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   else /* This is the first time that the FSA is created, */
        /* so notify AFD that we are done.                 */
   {
#ifdef _FIFO_DEBUG
      cmd[0] = AMG_READY; cmd[1] = '\0';
      show_fifo_data('W', "afd_cmd", cmd, 1, __FILE__, __LINE__);
#endif
      if (send_cmd(AMG_READY, afd_cmd_fd) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Was not able to send AMG_READY to %s. (%s %d)\n",
                   AFD, __FILE__, __LINE__);
      }
      first_time = NO;
   }
   if (close(afd_cmd_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Remove the old FSA file if there was one. */
   if (old_fsa_size > -1)
   {
      if (remove(old_fsa_stat) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to remove() %s : %s (%s %d)\n",
                   old_fsa_stat, strerror(errno), __FILE__, __LINE__);
      }
   }

   /*
    * Copy the new fsa_id into the locked FSA_ID_FILE file, unlock
    * and close the file.
    */
   /* Go to beginning in file */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not seek() to beginning of %s : %s (%s %d)\n",
                fsa_id_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Write new value into FSA_ID_FILE file */
   if (write(fd, &fsa_id, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not write value to FSA ID file : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Unlock file which holds the fsa_id */
   if (fcntl(fd, F_SETLKW, &ulock) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not unset write lock : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Close the FSA ID file */
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Close file with new FSA */
   if (close(fsa_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   fsa_fd = -1;

   /* Close old FSA file */
   if (old_fsa_fd != -1)
   {
      if (close(old_fsa_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
