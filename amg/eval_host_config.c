/*
 *  eval_host_config.c - Part of AFD, an automatic file distribution program.
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
 **   eval_host_config - reads the HOST_CONFIG file and stores the
 **                      contents in the structure host_list
 **
 ** SYNOPSIS
 **   int eval_host_config(void)
 **
 ** DESCRIPTION
 **   This function reads the HOST_CONFIG file which has the following
 **   format:
 **     AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB
 **     |   |   |   |  |  |  |  |  |  |   |  |  |
 **     |   |   |   |  |  |  |  |  |  |   |  |  +--> Number of no bursts
 **     |   |   |   |  |  |  |  |  |  |   |  +-----> Transfer timeout
 **     |   |   |   |  |  |  |  |  |  |   +--------> File size offset
 **     |   |   |   |  |  |  |  |  |  +------------> Successful retries
 **     |   |   |   |  |  |  |  |  +---------------> Transfer block size
 **     |   |   |   |  |  |  |  +------------------> Retry interval
 **     |   |   |   |  |  |  +---------------------> Max. errors
 **     |   |   |   |  |  +------------------------> Allowed transfers
 **     |   |   |   |  +---------------------------> Host toggle
 **     |   |   |   +------------------------------> Proxy name
 **     |   |   +----------------------------------> Real hostname 2
 **     |   +--------------------------------------> Real hostname 1
 **     +------------------------------------------> Alias hostname
 **
 **   The data is the stored in the global structure host_list. 
 **   Any value that is not set is initialised with its default value.
 **
 ** RETURN VALUES
 **   Will return YES when it finds an incorrect or incomplete entry.
 **   Otherwise it will exit() with INCORRECT when it fails to allocate
 **   memory for the host_list structure.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.11.1997 H.Kiehl Created
 **   02.03.1998 H.Kiehl Put host switching information into HOST_CONFIG.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>                  /* access(), F_OK                  */
#include <stdlib.h>                  /* atoi(), exit()                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int              sys_log_fd,
                        no_of_hosts;
extern char             host_config_file[];
extern struct host_list *hl;         /* Structure that holds all the     */
                                     /* hosts.                           */


/*+++++++++++++++++++++++++ eval_host_config() ++++++++++++++++++++++++++*/
int
eval_host_config(void)
{
   int    i,
          error_flag = NO;
   size_t new_size,
          offset;
   char   *ptr,
          *hostbase = NULL,
          number[MAX_INT_LENGTH + 1];

   /* Ensure that the HOST_CONFIG file does exist. */
   if (access(host_config_file, F_OK) == -1)
   {
      /* The file does not exist. Don't care since we will create */
      /* when we return from eval_dir_config().                   */
      return(NO_ACCESS);
   }

   /* Read the contents of the HOST_CONFIG file into a buffer. */
   if (read_file(host_config_file, &hostbase) == INCORRECT)
   {
      exit(INCORRECT);
   }

   ptr = hostbase;
   no_of_hosts = 0;

   /*
    * Cut off any comments before the hostname come.
    */
   for (;;)
   {
      if ((*ptr != '\n') && (*ptr != '#') &&
          (*ptr != ' ') && (*ptr != '\t'))
      {
         break;
      }
      while ((*ptr != '\0') && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == '\n')
      {
         while (*ptr == '\n')
         {
            ptr++;
         }
      }
   }

   do
   {
      if (*ptr == '#')
      {
         /* Check if line is a comment */
         while ((*ptr != '\0') && (*ptr != '\n'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            continue;
         }
         else
         {
            break;
         }
      }

      /* Check if buffer for host list structure is large enough. */
      if ((no_of_hosts % HOST_BUF_SIZE) == 0)
      {
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
         (void)memset((char *)hl + offset, 0, new_size);
      }

      /* Store host alias */
      i = 0;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_HOSTNAME_LENGTH))
      {
         hl[no_of_hosts].host_alias[i] = *ptr;
         ptr++; i++;
      }
      if ((i == MAX_HOSTNAME_LENGTH) && (*ptr != ':'))
      {
         error_flag = YES;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for host alias name %s exceeded in HOST_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   hl[no_of_hosts].host_alias, MAX_HOSTNAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      hl[no_of_hosts].host_alias[i] = '\0';
      hl[no_of_hosts].fullname[0] = '\0';
      hl[no_of_hosts].in_dir_config = (char)NO;
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].real_hostname[0][0] = '\0';
         hl[no_of_hosts].real_hostname[1][0] = '\0';
         hl[no_of_hosts].proxy_name[0]       = '\0';
         hl[no_of_hosts].host_toggle_str[0]  = '\0';
         hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real hostname 1 */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         hl[no_of_hosts].real_hostname[0][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         error_flag = YES;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for real hostname 1 for %s exceeded in HOST_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   hl[no_of_hosts].host_alias, MAX_REAL_HOSTNAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      hl[no_of_hosts].real_hostname[0][i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].real_hostname[1][0] = '\0';
         hl[no_of_hosts].proxy_name[0]       = '\0';
         hl[no_of_hosts].host_toggle_str[0]  = '\0';
         hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real hostname 2 */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         hl[no_of_hosts].real_hostname[1][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         error_flag = YES;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for real hostname 2 for %s exceeded in HOST_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   hl[no_of_hosts].host_alias, MAX_REAL_HOSTNAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      hl[no_of_hosts].real_hostname[1][i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].proxy_name[0]       = '\0';
         hl[no_of_hosts].host_toggle_str[0]  = '\0';
         hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Host Toggle */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_TOGGLE_STR_LENGTH))
      {
         hl[no_of_hosts].host_toggle_str[i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_TOGGLE_STR_LENGTH)
      {
         error_flag = YES;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for the host toggle string %s exceeded in HOST_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   hl[no_of_hosts].host_alias, MAX_TOGGLE_STR_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      hl[no_of_hosts].host_toggle_str[i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].proxy_name[0]       = '\0';
         hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Proxy Name */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_PROXY_NAME_LENGTH))
      {
         hl[no_of_hosts].proxy_name[i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_PROXY_NAME_LENGTH)
      {
         error_flag = YES;
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for proxy name for host %s exceeded in HOST_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   hl[no_of_hosts].host_alias, MAX_PROXY_NAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      hl[no_of_hosts].proxy_name[i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Allowed Transfers */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in allowed transfers field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_NO_PARALLEL_JOBS, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].allowed_transfers = DEFAULT_NO_PARALLEL_JOBS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value allowed transfers to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_NO_PARALLEL_JOBS);
              hl[no_of_hosts].allowed_transfers = DEFAULT_NO_PARALLEL_JOBS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if ((hl[no_of_hosts].allowed_transfers = atoi(number)) > MAX_NO_PARALLEL_JOBS)
              {
                 error_flag = YES;
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Maximum number of parallel (%d) transfers exceeded for %s. Value found in HOST_CONFIG %d (%s %d)\n",
                           MAX_NO_PARALLEL_JOBS, hl[no_of_hosts].host_alias,
                           hl[no_of_hosts].allowed_transfers, __FILE__, __LINE__);
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Setting it to the maximum value %d.\n",
                           MAX_NO_PARALLEL_JOBS);
                 hl[no_of_hosts].allowed_transfers = MAX_NO_PARALLEL_JOBS;
              }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Max Errors */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in max error field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_MAX_ERRORS, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].max_errors = DEFAULT_MAX_ERRORS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for max errors to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_MAX_ERRORS);
              hl[no_of_hosts].max_errors = DEFAULT_MAX_ERRORS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              hl[no_of_hosts].max_errors = atoi(number);
           }
           
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Retry Interval */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in retry interval field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_RETRY_INTERVAL, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].retry_interval = DEFAULT_RETRY_INTERVAL;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for retry interval to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_RETRY_INTERVAL);
              hl[no_of_hosts].retry_interval = DEFAULT_RETRY_INTERVAL;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              hl[no_of_hosts].retry_interval = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Transfer Block size */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in transfer block size field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_TRANSFER_BLOCKSIZE, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].transfer_blksize = DEFAULT_TRANSFER_BLOCKSIZE;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for transfer block size to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_TRANSFER_BLOCKSIZE);
              hl[no_of_hosts].transfer_blksize = DEFAULT_TRANSFER_BLOCKSIZE;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if ((hl[no_of_hosts].transfer_blksize = atoi(number)) > MAX_TRANSFER_BLOCKSIZE)
              {
                 error_flag = YES;
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Transfer block size for host %s to large (%d Bytes) setting it to %d Bytes. (%s %d)\n",
                           hl[no_of_hosts].host_alias,
                           hl[no_of_hosts].transfer_blksize, 
                           MAX_TRANSFER_BLOCKSIZE, __FILE__, __LINE__);
                 hl[no_of_hosts].transfer_blksize = MAX_TRANSFER_BLOCKSIZE;
              }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Successful Retries */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in successful retries field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_SUCCESSFUL_RETRIES, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for successful retries to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_SUCCESSFUL_RETRIES);
              hl[no_of_hosts].successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              hl[no_of_hosts].successful_retries = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store File Size Offset */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if ((isdigit(*ptr)) || (*ptr == '-'))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in file size offset field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_FILE_SIZE_OFFSET, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].file_size_offset = (char)DEFAULT_FILE_SIZE_OFFSET;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for file size offset to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_FILE_SIZE_OFFSET);
              hl[no_of_hosts].file_size_offset = (char)DEFAULT_FILE_SIZE_OFFSET;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              hl[no_of_hosts].file_size_offset = (char)atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS */
         hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Transfer Timeout */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in transfer timeout field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_TRANSFER_TIMEOUT, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for transfer timeout to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_TRANSFER_TIMEOUT);
              hl[no_of_hosts].transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              hl[no_of_hosts].transfer_timeout = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS */
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_hosts++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Number of no Bursts */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in number of no bursts field for host %s, using default %d. (%s %d)\n",
                      (int)*ptr, hl[no_of_hosts].host_alias,
                      DEFAULT_NO_OF_NO_BURSTS, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for number of no bursts to large (>%d characters) for host %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, hl[no_of_hosts].host_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %d.\n",
                        DEFAULT_NO_OF_NO_BURSTS);
              hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if ((hl[no_of_hosts].number_of_no_bursts = (unsigned char)atoi(number)) > hl[no_of_hosts].allowed_transfers)
              {
                 error_flag = YES;
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Number of no bursts for host %s is larger (%d) then allowed transfers. (%s %d)\n",
                           hl[no_of_hosts].host_alias,
                           hl[no_of_hosts].number_of_no_bursts, 
                           __FILE__, __LINE__);
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Setting it to %d.\n",
                            hl[no_of_hosts].number_of_no_bursts);
                 hl[no_of_hosts].transfer_blksize = hl[no_of_hosts].allowed_transfers;
              }
           }

      /* Ignore the rest of the line. We have everything we need. */
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         ptr++;
      }
      while (*ptr == '\n')
      {
         ptr++;
      }
      no_of_hosts++;
   } while (*ptr != '\0');

   free(hostbase);

   return(error_flag);
}
