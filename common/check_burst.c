/*
 *  check_burst.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_burst - checks when all connections are in use if the job
 **                 can be added to a job currently in transfer
 **
 ** SYNOPSIS
 **   int check_burst(char         protocol,
 **                   int          position,
 **                   unsigned int job_id,
 **                   char         *src_dir,
 **                   char         *dst_dir,
 **                   int          limit)
 **
 ** DESCRIPTION
 **    When a connection is currently open to the remote site this
 **    function adds new files to this job. This can only be done under
 **    the following condition:
 **
 **        - This is an FTP (or WMO or SCP1 when enabled) job.
 **        - All connections for this host are in use.
 **        - The active job is currently transmitting files.
 **        - The job ID's are the same.
 **        - The number of jobs that are queued for bursting may
 **          not be larger than MAX_NO_OF_BURSTS.
 **
 **    NOTE: Checking for an error is unneccessary since
 **          all that happens in such a situation is that a job
 **          gets larger and the next time we try to connect this
 **          and the additional job will connect only once.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.01.1997 H.Kiehl Created
 **   05.03.1997 H.Kiehl Interrupt burst when there is already a file
 **                      with the same name in the transmitting directory.
 **   14.10.1997 H.Kiehl Allow user to reduce the number of parallel bursts.
 **   26.11.1997 H.Kiehl Search for a connection that is not bursting or
 **                      find the lowest burst counter in bursting jobs. 
 **   30.05.1998 H.Kiehl Added support for WMO bursts.
 **   19.03.1999 H.Kiehl New parameter limit.
 **   05.08.2001 H.Kiehl Make this bursting work better in conjunction with
 **                      burst2.
 **   07.08.2001 H.Kiehl Take the lowest number of bytes still to be send
 **                      instead of the lowest number of burst to determine
 **                      which burst should be taken.
 **   30.01.2002 H.Kiehl Addition of global burst counter for AMG and FD.
 **
 */
DESCR__E_M3

#ifdef _BURST_MODE
#include <stdio.h>
#include <string.h>                /* strcpy(), strcat(), strerror()     */
#include <unistd.h>                /* rmdir(), R_OK                      */
#include <sys/types.h>
#include <sys/stat.h>              /* struct stat                        */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>

/* External global variables */
extern int                        amg_flag,
                                  fsa_fd,
                                  no_of_hosts;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;

/* Local function prototypes */
static int                        burst_files(int, int, char *, char *);

#define MAX_NO_OF_BURSTS 50


/*############################ check_burst() ############################*/
int
check_burst(char         protocol,
            int          position,
            unsigned int job_id,
            char         *src_dir,
            char         *dst_dir,
            int          limit)
{
   /* Do NOT burst when FSA is STALE! */
   if (no_of_hosts == STALE)
   {
      return(NO);
   }

   if (((protocol == FTP)
#ifdef _WITH_WMO_SUPPORT
       || (protocol == WMO)
#endif
#ifdef _WITH_SCP1_SUPPORT
       || (protocol == SCP1)) &&
#else
       ) &&
#endif
       (fsa[position].active_transfers >= fsa[position].allowed_transfers) &&
       ((fsa[position].special_flag & NO_BURST_COUNT_MASK) < fsa[position].allowed_transfers))
   {
      int           j,
                    status,
#if defined (_WITH_WMO_SUPPORT) || defined (_WITH_SCP1_SUPPORT)
                    connect_status,
#endif
                    burst_connection,
                    no_of_bursts = 0,
                    no_of_total_bursts = 0,
                    burst_array[MAX_NO_PARALLEL_JOBS],
                    no_of_no_bursts = 0,
                    no_burst_array[MAX_NO_PARALLEL_JOBS];
#ifdef _WITH_FILE_SIZE_CRITERIA
      unsigned long lowest_bytes,
                    tmp_lowest_bytes;
#else
      int           lowest_burst;
#endif

#if defined (_WITH_WMO_SUPPORT) || defined (_WITH_SCP1_SUPPORT)
      if (protocol == FTP)
      {
         connect_status = FTP_BURST_TRANSFER_ACTIVE;
      }
#if defined (_WITH_WMO_SUPPORT) && defined (_WITH_SCP1_SUPPORT)
      else if (protocol == SCP1)
           {
              connect_status = SCP1_BURST_TRANSFER_ACTIVE;
           }
           else
           {
              connect_status = WMO_BURST_TRANSFER_ACTIVE;
           }
#else
      else
      {
#ifdef _WITH_SCP1_SUPPORT
         connect_status = SCP1_BURST_TRANSFER_ACTIVE;
#else
         connect_status = WMO_BURST_TRANSFER_ACTIVE;
#endif
      }
#endif
#endif

      /*
       * Lock this host while we are checking if we may burst.
       * Otherwise we cannot ensure that the no burst counter
       * does have any effect.
       */
      lock_region_w(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);

      for (j = 0; j < fsa[position].allowed_transfers; j++)
      {
#if defined (_WITH_WMO_SUPPORT) || defined (_WITH_SCP1_SUPPORT)
         if ((fsa[position].job_status[j].connect_status == connect_status) ||
#else
         if ((fsa[position].job_status[j].connect_status == FTP_BURST_TRANSFER_ACTIVE) ||
#endif
             (fsa[position].job_status[j].burst_counter > 0))
         {
            if ((fsa[position].job_status[j].job_id == job_id) &&
                (fsa[position].job_status[j].burst_counter < MAX_NO_OF_BURSTS))
            {
               burst_array[no_of_bursts] = j;
               no_of_bursts++;
            }
            no_of_total_bursts++;
         }
         else
         {
            if (fsa[position].job_status[j].job_id == job_id)
            {
               no_burst_array[no_of_no_bursts] = j;
               no_of_no_bursts++;
            }
         }
      }

      if ((no_of_bursts == 0) && (no_of_no_bursts == 0))
      {
         /*
          * All connections are curently busy with some other type
          * of job.
          */
         unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
         return(NO);
      }

      if ((fsa[position].special_flag & NO_BURST_COUNT_MASK) > 0)
      {
         if (no_of_total_bursts >= (fsa[position].allowed_transfers - (fsa[position].special_flag & NO_BURST_COUNT_MASK)))
         {
            int burst_array_position;

            /*
             * We may not create a new burst. Try to find a burst with
             * the same job ID and append it to the one with the lowest
             * burst counter.
             */
            while (no_of_bursts > 0)
            {
#ifdef _WITH_FILE_SIZE_CRITERIA
               lowest_bytes = fsa[position].job_status[burst_array[0]].file_size - fsa[position].job_status[burst_array[0]].file_size;
               burst_connection = burst_array[0];
               burst_array_position = 0;
               for (j = 1; j < no_of_bursts; j++)
               {
                  tmp_lowest_bytes = fsa[position].job_status[burst_array[j]].file_size - fsa[position].job_status[burst_array[j]].file_size;
                  if (tmp_lowest_bytes < lowest_bytes)
                  {
                     lowest_bytes = tmp_lowest_bytes;
                     burst_connection = burst_array[j];
                     burst_array_position = j;
                  }
               }
#else
               lowest_burst = fsa[position].job_status[burst_array[0]].burst_counter;
               burst_connection = burst_array[0];
               burst_array_position = 0;
               for (j = 1; j < no_of_bursts; j++)
               {
                  if (fsa[position].job_status[burst_array[j]].burst_counter < lowest_burst)
                  {
                     lowest_burst = fsa[position].job_status[burst_array[j]].burst_counter;
                     burst_connection = burst_array[j];
                     burst_array_position = j;
                  }
               }
#endif
               if ((fsa[position].job_status[burst_connection].job_id == job_id) &&
                   ((fsa[position].job_status[burst_connection].connect_status == FTP_BURST_TRANSFER_ACTIVE)
#ifdef _WITH_WMO_SUPPORT
                    || (fsa[position].job_status[burst_connection].connect_status == WMO_BURST_TRANSFER_ACTIVE)
#endif
#ifdef _WITH_SCP1_SUPPORT
                    || (fsa[position].job_status[burst_connection].connect_status == SCP1_BURST_TRANSFER_ACTIVE)) &&
#else
                   ) &&
#endif
                   ((limit == NO) ||
                    (fsa[position].job_status[burst_connection].burst_counter < MAX_NO_OF_BURSTS)) &&
                   (lock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa) == LOCK_IS_NOT_SET))
               {
                  if (fsa[position].job_status[burst_connection].job_id != job_id)
                  {
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "GOTCHA, you lousy bug! Job ID's not the same! [%d != %d]",
                                fsa[position].job_status[burst_connection].job_id,
                                job_id);
                  }
                  else
                  {
                     /* BURST FILES!!! */
                     status = burst_files(burst_connection, position, src_dir, dst_dir);
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);

                     if (status == SUCCESS)
                     {
                        unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
                        return(YES);
                     }
                  }
               }

               for (j = burst_array_position; j < (no_of_bursts - 1); j++)
               {
                  burst_array[j] = burst_array[j + 1];
               }
               no_of_bursts--;
            }
         }
         else
         {
            /*
             * Create a new burst if possible. First search those that
             * do not burst and only then test the bursting connections.
             */
            for (j = 0; j < no_of_no_bursts; j++)
            {
               if ((fsa[position].job_status[no_burst_array[j]].job_id == job_id) &&
                   ((fsa[position].job_status[no_burst_array[j]].connect_status == FTP_ACTIVE) ||
                    (fsa[position].job_status[no_burst_array[j]].connect_status == FTP_BURST2_TRANSFER_ACTIVE)
#ifdef _WITH_WMO_SUPPORT
                    || (fsa[position].job_status[no_burst_array[j]].connect_status == WMO_ACTIVE)
#endif
#ifdef _WITH_SCP1_SUPPORT
                    || (fsa[position].job_status[no_burst_array[j]].connect_status == SCP1_ACTIVE)) &&
#else
                   ) &&
#endif
                   ((limit == NO) ||
                    (fsa[position].job_status[no_burst_array[j]].burst_counter < MAX_NO_OF_BURSTS)) &&
                   (lock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa) == LOCK_IS_NOT_SET))
               {
                  if (fsa[position].job_status[no_burst_array[j]].job_id != job_id)
                  {
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa);
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "GOTCHA, you lousy bug! Job ID's not the same! [%d != %d]",
                                fsa[position].job_status[no_burst_array[j]].job_id,
                                job_id);
                  }
                  else
                  {
                     /* BURST FILES!!! */
                     status = burst_files(no_burst_array[j], position, src_dir, dst_dir);
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa);

                     if (status == SUCCESS)
                     {
                        unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
                        return(YES);
                     }
                  }
               }
            }

            /*
             * Are there any existing bursts?
             */
            while (no_of_bursts > 0)
            {
               int burst_array_position;

               /*
                * Hmm. Failed to create a new burst. Lets try one of
                * the existing bursts.
                */
#ifdef _WITH_FILE_SIZE_CRITERIA
               lowest_bytes = fsa[position].job_status[burst_array[0]].file_size - fsa[position].job_status[burst_array[0]].file_size;
               burst_connection = burst_array[0];
               burst_array_position = 0;
               for (j = 1; j < no_of_bursts; j++)
               {
                  tmp_lowest_bytes = fsa[position].job_status[burst_array[j]].file_size - fsa[position].job_status[burst_array[j]].file_size;
                  if (tmp_lowest_bytes < lowest_bytes)
                  {
                     lowest_bytes = tmp_lowest_bytes;
                     burst_connection = burst_array[j];
                     burst_array_position = j;
                  }
               }
#else
               lowest_burst = fsa[position].job_status[burst_array[0]].burst_counter;
               burst_connection = burst_array[0];
               burst_array_position = 0;
               for (j = 1; j < no_of_bursts; j++)
               {
                  if (fsa[position].job_status[burst_array[j]].burst_counter < lowest_burst)
                  {
                     lowest_burst = fsa[position].job_status[burst_array[j]].burst_counter;
                     burst_connection = burst_array[j];                                    
                     burst_array_position = j;
                  }                           
               }
#endif
               if ((fsa[position].job_status[burst_connection].job_id == job_id) &&
                   ((fsa[position].job_status[burst_connection].connect_status == FTP_BURST_TRANSFER_ACTIVE)
#ifdef _WITH_WMO_SUPPORT
                    || (fsa[position].job_status[burst_connection].connect_status == WMO_BURST_TRANSFER_ACTIVE)
#endif
#ifdef _WITH_SCP1_SUPPORT
                    || (fsa[position].job_status[burst_connection].connect_status == SCP1_BURST_TRANSFER_ACTIVE)) &&
#else
                   ) &&
#endif
                   ((limit == NO) ||
                    (fsa[position].job_status[burst_connection].burst_counter < MAX_NO_OF_BURSTS)) &&
                   (lock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa) == LOCK_IS_NOT_SET))
               {
                  if (fsa[position].job_status[burst_connection].job_id != job_id)
                  {
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "GOTCHA, you lousy bug! Job ID's not the same! [%d != %d]",
                                fsa[position].job_status[burst_connection].job_id,
                                job_id);
                  }
                  else
                  {
                     /* BURST FILES!!! */
                     status = burst_files(burst_connection, position, src_dir, dst_dir);
                     unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);

                     if (status == SUCCESS)
                     {
                        unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
                        return(YES);
                     }
                  }
               }

               for (j = burst_array_position; j < (no_of_bursts - 1); j++)
               {
                  burst_array[j] = burst_array[j + 1];
               }
               no_of_bursts--;
            }
         }
         unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
         return(NO);
      }
      else
      {
         int burst_array_position;

         /*
          * There is no restriction in bursting. First locate a
          * connection that is not bursting then try the bursting ones.
          */
         for (j = 0; j < no_of_no_bursts; j++)
         {
            if ((fsa[position].job_status[no_burst_array[j]].job_id == job_id) &&
                ((fsa[position].job_status[no_burst_array[j]].connect_status == FTP_ACTIVE) ||
                 (fsa[position].job_status[no_burst_array[j]].connect_status == FTP_BURST2_TRANSFER_ACTIVE)
#ifdef _WITH_WMO_SUPPORT
                 || (fsa[position].job_status[no_burst_array[j]].connect_status == WMO_ACTIVE)
#endif
#ifdef _WITH_SCP1_SUPPORT
                 || (fsa[position].job_status[no_burst_array[j]].connect_status == SCP1_ACTIVE)) &&
#else
                ) &&
#endif
                ((limit == NO) ||
                 (fsa[position].job_status[no_burst_array[j]].burst_counter < MAX_NO_OF_BURSTS)) &&
                (lock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa) == LOCK_IS_NOT_SET))
            {
               if (fsa[position].job_status[no_burst_array[j]].job_id != job_id)
               {
                  unlock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa);
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "GOTCHA, you lousy bug! Job ID's not the same! [%d != %d]",
                             fsa[position].job_status[no_burst_array[j]].job_id,
                             job_id);
               }
               else
               {
                  /* BURST FILES!!! */
                  status = burst_files(no_burst_array[j], position, src_dir, dst_dir);
                  unlock_region(fsa_fd, (char *)&fsa[position].job_status[no_burst_array[j]].job_id - (char *)fsa);

                  if (status == SUCCESS)
                  {
                     unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
                     return(YES);
                  }
               }
            }
         }

         /*
          * Hmm. Failed to create a new burst. Lets try one of
          * the existing bursts.
          */
         while (no_of_bursts > 0)
         {
#ifdef _WITH_FILE_SIZE_CRITERIA
            lowest_bytes = fsa[position].job_status[burst_array[0]].file_size - fsa[position].job_status[burst_array[0]].file_size;
            burst_connection = burst_array[0];
            burst_array_position = 0;
            for (j = 1; j < no_of_bursts; j++)
            {
               tmp_lowest_bytes = fsa[position].job_status[burst_array[j]].file_size - fsa[position].job_status[burst_array[j]].file_size;
               if (tmp_lowest_bytes < lowest_bytes)
               {
                  lowest_bytes = tmp_lowest_bytes;
                  burst_connection = burst_array[j];
                  burst_array_position = j;
               }
            }
#else
            lowest_burst = fsa[position].job_status[burst_array[0]].burst_counter;
            burst_connection = burst_array[0];
            burst_array_position = 0;
            for (j = 1; j < no_of_bursts; j++)
            {
               if (fsa[position].job_status[burst_array[j]].burst_counter < lowest_burst)
               {
                  lowest_burst = fsa[position].job_status[burst_array[j]].burst_counter;
                  burst_connection = burst_array[j];
                  burst_array_position = j;
               }
            }
#endif
            if ((fsa[position].job_status[burst_connection].job_id == job_id) &&
                ((fsa[position].job_status[burst_connection].connect_status == FTP_BURST_TRANSFER_ACTIVE)
#ifdef _WITH_WMO_SUPPORT
                 || (fsa[position].job_status[burst_connection].connect_status == WMO_BURST_TRANSFER_ACTIVE)
#endif
#ifdef _WITH_SCP1_SUPPORT
                 || (fsa[position].job_status[burst_connection].connect_status == SCP1_BURST_TRANSFER_ACTIVE)) &&
#else
                ) &&
#endif
                ((limit == NO) ||
                 (fsa[position].job_status[burst_connection].burst_counter < MAX_NO_OF_BURSTS)) &&
                (lock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa) == LOCK_IS_NOT_SET))
            {
               if (fsa[position].job_status[burst_connection].job_id != job_id)
               {
                  unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "GOTCHA, you lousy bug! Job ID's not the same! [%d != %d]",
                             fsa[position].job_status[burst_connection].job_id,
                             job_id);
               }
               else
               {
                  /* BURST FILES!!! */
                  status = burst_files(burst_connection, position, src_dir, dst_dir);
                  unlock_region(fsa_fd, (char *)&fsa[position].job_status[burst_connection].job_id - (char *)fsa);

                  if (status == SUCCESS)
                  {
                     unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
                     return(YES);
                  }
               }
            }

            for (j = burst_array_position; j < (no_of_bursts - 1); j++)
            {
               burst_array[j] = burst_array[j + 1];
            }
            no_of_bursts--;
         }

         unlock_region(fsa_fd, fsa[position].host_alias - (char *)fsa + 1);
         return(NO);
      }
   }
   else
   {
      return(NO);
   }
}


/*++++++++++++++++++++++++++++++ burst_files() ++++++++++++++++++++++++++*/
static int
burst_files(int burst_connection, int position, char *src_dir, char *dst_dir)
{
   int           count = 0,
                 tmp_count;
   char          *from_ptr,
                 *to_ptr,
                 newname[MAX_PATH_LENGTH];
   DIR           *dp;
   struct dirent *p_dir;

   /*
    * Move the files to the job that is currently being transmitted.
    */
   if ((dp = opendir(src_dir)) == NULL)
   {
      char progname[4];

      if (amg_flag == YES)
      {
         progname[0] = 'A';
         progname[1] = 'M';
         progname[2] = 'G';
         progname[3] = '\0';
      }
      else
      {
         progname[0] = 'F';
         progname[1] = 'D';
         progname[2] = '\0';
      }
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Can't access directory <%s> : %s [%s]",
                 src_dir, strerror(errno), progname);

      return(INCORRECT);
   }

   to_ptr = newname;
   tmp_count = 0;
   while (dst_dir[tmp_count] != '\0')
   {
      *to_ptr = dst_dir[tmp_count];
      to_ptr++; tmp_count++;
   }
   if (fsa[position].job_status[burst_connection].error_file == YES)
   {
      if (*(to_ptr - 1) == '/')
      {
         to_ptr--;
      }
      (void)strcpy(to_ptr, ERROR_DIR);
      to_ptr += ERROR_DIR_LENGTH;
      *to_ptr = '/';
      to_ptr++;
      tmp_count = 0;
      while (fsa[position].host_alias[tmp_count] != '\0')
      {
         *to_ptr = fsa[position].host_alias[tmp_count];
         to_ptr++; tmp_count++;
      }
   }
   if (*(to_ptr - 1) != '/')
   {
      *to_ptr = '/';
      to_ptr++;
   }
   tmp_count = 0;
   while (fsa[position].job_status[burst_connection].unique_name[tmp_count] != '\0')
   {
      *to_ptr = fsa[position].job_status[burst_connection].unique_name[tmp_count];
      to_ptr++; tmp_count++;
   }
   if (tmp_count == 0)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Hmmm. No unique name for host %s [%d].",
                 fsa[position].host_dsp_name , burst_connection);
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not close directory <%s> : %s",
                    src_dir, strerror(errno));
      }
      return(INCORRECT);
   }
   *to_ptr++ = '/';
   from_ptr = src_dir + strlen(src_dir);
   *from_ptr++ = '/';

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] != '.')
      {
         (void)strcpy(to_ptr, p_dir->d_name);

         /*
          * We have to handle the case when we send the same
          * file twice. Let's not 'burst' such a file. If we
          * find a file with the same name, don't move it to the
          * transmitting directory. Otherwise the file number and
          * size is wrong in afd_ctrl dialog. Generate a new
          * message (AMG) or leave the old message (FD).
          */
         if (access(newname, F_OK) == 0)
         {
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not close directory <%s> : %s",
                          src_dir, strerror(errno));
            }
            if (count > 0)
            {
               fsa[position].job_status[burst_connection].burst_counter++;
               if (amg_flag == YES)
               {
                  p_afd_status->amg_burst_counter++;
               }
               else
               {
                  p_afd_status->fd_burst_counter++;
               }
            }
            *(from_ptr - 1) = '\0';

            return(INCORRECT);
         }

         (void)strcpy(from_ptr, p_dir->d_name);
         if (rename(src_dir, newname) == -1)
         {
            char progname[4];

            if (amg_flag == YES)
            {
               progname[0] = 'A';
               progname[1] = 'M';
               progname[2] = 'G';
               progname[3] = '\0';
            }
            else
            {
               progname[0] = 'F';
               progname[1] = 'D';
               progname[2] = '\0';
            }
            if (errno == ENOENT)
            {
               /*
                * The target directory is gone. No need to continue here.
                * How can this happen?!
                */
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Could not rename() file %s to %s : %s [%s]",
                          src_dir, newname, strerror(errno), progname);

               /*
                * Lets determine what is gone source or target?
                */
               if (eaccess(src_dir, R_OK) != 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Source file <%s> does not exist!", src_dir);
               }
               *to_ptr = '\0';
               if (eaccess(newname, R_OK) != 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Target directory <%s> does not exist!", newname);
               }
               *from_ptr = '\0';
               if (eaccess(src_dir, R_OK) != 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Source directory <%s> does not exist!", src_dir);
               }
               if (closedir(dp) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not close directory <%s> : %s",
                             src_dir, strerror(errno));
               }
               if (count > 0)
               {
                  fsa[position].job_status[burst_connection].burst_counter++;
                  if (amg_flag == YES)
                  {
                     p_afd_status->amg_burst_counter++;
                  }
                  else
                  {
                     p_afd_status->fd_burst_counter++;
                  }
               }
               *(from_ptr - 1) = '\0';

               return(INCORRECT);
            }
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could not rename() file <%s> to <%s> : %s [%s]",
                       src_dir, newname, strerror(errno), progname);
         }
         else
         {
            count++;
         }
      }
      errno = 0;
   }

   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not readdir() <%s> : %s", src_dir, strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not closedir() <%s> : %s", src_dir, strerror(errno));
   }
   if (count > 0)
   {
      fsa[position].job_status[burst_connection].burst_counter++;
      if (amg_flag == YES)
      {
         p_afd_status->amg_burst_counter++;
      }
      else
      {
         p_afd_status->fd_burst_counter++;
      }
   }
   *(from_ptr - 1) = '\0';

   /* Remove the directory from where the files have been moved */
   if (rmdir(src_dir) == -1)
   {
      char progname[4];

      if (amg_flag == YES)
      {
         progname[0] = 'A';
         progname[1] = 'M';
         progname[2] = 'G';
         progname[3] = '\0';
      }
      else
      {
         progname[0] = 'F';
         progname[1] = 'D';
         progname[2] = '\0';
      }
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rmdir() <%s> : %s [%s]",
                 src_dir, strerror(errno), progname);
      return(INCORRECT);
   }

   return(SUCCESS);
}
#endif /* _BURST_MODE */
