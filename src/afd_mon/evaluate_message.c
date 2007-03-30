/*
 *  evaluate_message.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006, 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   evaluate_message - evaluates a message send from afdd
 **
 ** SYNOPSIS
 **   int evaluate_message(int *bytes_done)
 **
 ** DESCRIPTION
 **   Evaluates what is returned in msg_str. The first two characters
 **   always describe the content of the message:
 **               IS - Interval summary
 **               NH - Number of hosts
 **               ND - Number of directories
 **               NJ - Number of jobs configured
 **               MC - Maximum number of connections
 **               SR - System Log Radar Information
 **               AM - Status of AMG
 **               FD - Status of FD
 **               AW - Status of archive_watch
 **               WD - remote AFD working directory
 **               AV - AFD version
 **               HL - List of current host alias names and real
 **                    hostnames
 **               DL - List of current directories
 **               EL - Error list for a given host
 **               RH - Receive log history
 **               SH - System log history
 **               TH - Transfer log history
 **             Log message types:
 **               LC - Log capabilities
 **               JD - Job ID data
 **
 ** RETURN VALUES
 **   Returns the following values:
 **     - SUCCESS
 **     - AFDD_SHUTTING_DOWN
 **     - any value above 99 which will be the return code of a command
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.10.2006 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fprintf()                                */
#include <string.h>          /* strcpy(), strcat(), strerror()           */
#include <stdlib.h>          /* strtoul()                                */
#include <ctype.h>           /* isdigit()                                */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "afdddefs.h"

/* Global variables. */
extern int                    afd_no,
                              got_log_capabilities,
                              shift_log_his[],
                              timeout_flag;
extern time_t                 new_hour_time;
extern size_t                 adl_size,
                              ahl_size;
extern char                   adl_file_name[],
                              ahl_file_name[],
                              msg_str[];
extern struct mon_status_area *msa;
extern struct afd_dir_list    *adl;
extern struct afd_host_list   *ahl;


/*######################### evaluate_message() ##########################*/
int
evaluate_message(int *bytes_done)
{
   int  ret;
   char *ptr,
        *ptr_start;

   /* Evaluate what is returned and store in MSA. */
   ptr = msg_str;
   for (;;)
   {
      if (*ptr == '\0')
      {
         break;
      }
      ptr++;
   }
   *bytes_done = ptr - msg_str + 2;
   ptr = msg_str;
   if ((*ptr == 'I') && (*(ptr + 1) == 'S'))
   {
      ptr += 3;
      ptr_start = ptr;
      while ((*ptr != ' ') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ' ')
      {
         /* Store the number of files still to be send. */
         *ptr = '\0';
         msa[afd_no].fc = (unsigned int)strtoul(ptr_start, NULL, 10);
         ptr++;
         ptr_start = ptr;
         while ((*ptr != ' ') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == ' ')
         {
            /* Store the number of bytes still to be send. */
            *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
            msa[afd_no].fs = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
            msa[afd_no].fs = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
            msa[afd_no].fs = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
            ptr++;
            ptr_start = ptr;
            while ((*ptr != ' ') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == ' ')
            {
               /* Store the transfer rate. */
               *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
               msa[afd_no].tr = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
               msa[afd_no].tr = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
               msa[afd_no].tr = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
               if (msa[afd_no].tr > msa[afd_no].top_tr[0])
               {
                  msa[afd_no].top_tr[0] = msa[afd_no].tr;
                  msa[afd_no].top_tr_time = msa[afd_no].last_data_time;
               }
               ptr++;
               ptr_start = ptr;
               while ((*ptr != ' ') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == ' ')
               {
                  /* Store file rate. */
                  *ptr = '\0';
                  msa[afd_no].fr = (unsigned int)strtoul(ptr_start, NULL, 10);
                  if (msa[afd_no].fr > msa[afd_no].top_fr[0])
                  {
                     msa[afd_no].top_fr[0] = msa[afd_no].fr;
                     msa[afd_no].top_fr_time = msa[afd_no].last_data_time;
                  }
                  ptr++;
                  ptr_start = ptr;
                  while ((*ptr != ' ') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == ' ')
                  {
                     /* Store error counter. */
                     *ptr = '\0';
                     msa[afd_no].ec = (unsigned int)strtoul(ptr_start, NULL, 10);
                     ptr++;
                     ptr_start = ptr;
                     while ((*ptr != ' ') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == ' ')
                     {
                        /* Store error hosts. */
                        *ptr = '\0';
                        msa[afd_no].host_error_counter = atoi(ptr_start);
                        ptr++;
                        ptr_start = ptr;
                        while ((*ptr != ' ') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == ' ')
                        {
                           /* Store number of transfers. */
                           *ptr = '\0';
                           msa[afd_no].no_of_transfers = atoi(ptr_start);
                           if (msa[afd_no].no_of_transfers > msa[afd_no].top_no_of_transfers[0])
                           {
                              msa[afd_no].top_no_of_transfers[0] = msa[afd_no].no_of_transfers;
                              msa[afd_no].top_not_time = msa[afd_no].last_data_time;
                           }
                           ptr++;
                           ptr_start = ptr;
                           while ((*ptr != ' ') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\0')
                           {
                              /* Store number of jobs in queue. */
                              msa[afd_no].jobs_in_queue = atoi(ptr_start);
                           }
                           else if (*ptr == ' ')
                                {
                                   /* Store number of jobs in queue. */
                                   *ptr = '\0';
                                   msa[afd_no].jobs_in_queue = atoi(ptr_start);
                                   ptr++;
                                   ptr_start = ptr;
                                   while ((*ptr != ' ') && (*ptr != '\0'))
                                   {
                                      ptr++;
                                   }
                                   if (*ptr == ' ')
                                   {
                                      /* Store files send. */
                                      *ptr = '\0';
                                      msa[afd_no].files_send[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                      ptr++;
                                      ptr_start = ptr;
                                      while ((*ptr != ' ') && (*ptr != '\0'))
                                      {
                                         ptr++;
                                      }
                                      if (*ptr == ' ')
                                      {
                                         /* Store bytes send. */
                                         *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
                                         msa[afd_no].bytes_send[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
                                         ptr++;
                                         ptr_start = ptr;
                                         while ((*ptr != ' ') && (*ptr != '\0'))
                                         {
                                            ptr++;
                                         }
                                         if (*ptr == ' ')
                                         {
                                            /* Store number of connections. */
                                            *ptr = '\0';
                                            msa[afd_no].connections[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                            ptr++;
                                            ptr_start = ptr;
                                            while ((*ptr != ' ') && (*ptr != '\0'))
                                            {
                                               ptr++;
                                            }
                                            if (*ptr == ' ')
                                            {
                                               /* Store number of total errors. */
                                               *ptr = '\0';
                                               msa[afd_no].total_errors[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                               ptr++;
                                               ptr_start = ptr;
                                               while ((*ptr != ' ') && (*ptr != '\0'))
                                               {
                                                  ptr++;
                                               }
                                               if (*ptr == ' ')
                                               {
                                                  /* Store files received. */
                                                  *ptr = '\0';
                                                  msa[afd_no].files_received[CURRENT_SUM] = (unsigned int)strtoul(ptr_start, NULL, 10);
                                                  ptr++;
                                                  ptr_start = ptr;
                                                  while ((*ptr != ' ') && (*ptr != '\0'))
                                                  {
                                                     ptr++;
                                                  }
                                                  if ((*ptr == ' ') || (*ptr == '\0'))
                                                  {
                                                     /* Store bytes received. */
                                                     *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
# else
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoull(ptr_start, NULL, 10);
# endif
#else
                                                     msa[afd_no].bytes_received[CURRENT_SUM] = (u_off_t)strtoul(ptr_start, NULL, 10);
#endif
                                                  }
                                               }
                                            }
                                         }
                                      }
                                   }
                                }
                        }
                     }
                  }
               }
            }
         }
         if ((msa[afd_no].special_flag & SUM_VAL_INITIALIZED) == 0)
         {
            int i;

            for (i = 1; i < SUM_STORAGE; i++)
            {
               msa[afd_no].bytes_send[i]         = msa[afd_no].bytes_send[CURRENT_SUM];
               msa[afd_no].bytes_received[i]     = msa[afd_no].bytes_received[CURRENT_SUM];
               msa[afd_no].files_send[i]         = msa[afd_no].files_send[CURRENT_SUM];
               msa[afd_no].files_received[i]     = msa[afd_no].files_received[CURRENT_SUM];
               msa[afd_no].connections[i]        = msa[afd_no].connections[CURRENT_SUM];
               msa[afd_no].total_errors[i]       = msa[afd_no].total_errors[CURRENT_SUM];
               msa[afd_no].log_bytes_received[i] = msa[afd_no].log_bytes_received[CURRENT_SUM];
            }
            msa[afd_no].special_flag ^= SUM_VAL_INITIALIZED;
         }
         ret = SUCCESS;
      }
#ifdef _DEBUG_PRINT
      (void)fprintf(stderr, "IS %d %d %d %d %d %d %d\n",
                    msa[afd_no].fc, msa[afd_no].fs, msa[afd_no].tr,
                    msa[afd_no].fr, msa[afd_no].ec,
                    msa[afd_no].host_error_counter,
                    msa[afd_no].jobs_in_queue);
#endif
   }
   /*
    * Status of AMG (AM)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'M'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].amg = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AM %d\n", msa[afd_no].amg);
#endif
        }
   /*
    * Status of FD (FD)
    */
   else if ((*ptr == 'F') && (*(ptr + 1) == 'D'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].fd = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "FD %d\n", msa[afd_no].fd);
#endif
        }
   /*
    * Status of archive watch (AW)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'W'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].archive_watch = (char)atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "AW %d\n", msa[afd_no].archive_watch);
#endif
        }
   /*
    * Number of hosts (NH)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'H'))
        {
           int new_no_of_hosts;

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_hosts = atoi(ptr_start);
           if ((new_no_of_hosts != msa[afd_no].no_of_hosts) ||
               (ahl == NULL))
           {
              int  fd;
              char *ahl_ptr;

              if (ahl != NULL)
              {
#ifdef HAVE_MMAP
                 if (munmap((void *)ahl, ahl_size) == -1)
#else
                 if (munmap_emu((void *)ahl) == -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "munmap() error : %s", strerror(errno));
                 }
              }
              ahl_size = new_no_of_hosts * sizeof(struct afd_host_list);
              msa[afd_no].no_of_hosts = new_no_of_hosts;
              if ((ahl_ptr = attach_buf(ahl_file_name, &fd, ahl_size,
                                        NULL, FILE_MODE, NO)) == (caddr_t) -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to mmap() %s : %s",
                            ahl_file_name, strerror(errno));
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "close() error : %s", strerror(errno));
                 }
                 ahl = (struct afd_host_list *)ahl_ptr;
              }
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "NH %d\n", msa[afd_no].no_of_hosts);
#endif
        }
   /*
    * Number of directories (ND)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'D'))
        {
           int new_no_of_dirs;

           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           new_no_of_dirs = atoi(ptr_start);
           if ((new_no_of_dirs != msa[afd_no].no_of_dirs) ||
               (adl == NULL))
           {
              int  fd;
              char *adl_ptr;

              if (adl != NULL)
              {
#ifdef HAVE_MMAP
                 if (munmap((void *)adl, adl_size) == -1)
#else
                 if (munmap_emu((void *)adl) == -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "munmap() error : %s", strerror(errno));
                 }
              }
              adl_size = new_no_of_dirs * sizeof(struct afd_dir_list);
              msa[afd_no].no_of_dirs = new_no_of_dirs;
              if ((adl_ptr = attach_buf(adl_file_name, &fd, adl_size,
                                        NULL, FILE_MODE, NO)) == (caddr_t) -1)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to mmap() %s : %s",
                            adl_file_name, strerror(errno));
              }
              else
              {
                 if (close(fd) == -1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "close() error : %s", strerror(errno));
                 }
                 adl = (struct afd_dir_list *)adl_ptr;
              }
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "ND %d\n", msa[afd_no].no_of_dirs);
#endif
        }
   /*
    * Maximum number of connections. (MC)
    */
   else if ((*ptr == 'M') && (*(ptr + 1) == 'C'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].max_connections = atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "MC %d\n", msa[afd_no].max_connections);
#endif
        }
   /*
    * Number of jobs. (NJ)
    */
   else if ((*ptr == 'N') && (*(ptr + 1) == 'J'))
        {
           ptr += 3;
           ptr_start = ptr;
           while (*ptr != '\0')
           {
              ptr++;
           }
           msa[afd_no].no_of_jobs = atoi(ptr_start);
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "NJ %d\n", msa[afd_no].no_of_jobs);
#endif
        }
   /*
    * System log radar (SR)
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'R'))
        {
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              register int i = 0;
#ifdef _DEBUG_PRINT
              int  length = 2;
              char fifo_buf[(LOG_FIFO_SIZE * 4) + 4];

              fifo_buf[0] = 'S';
              fifo_buf[1] = 'R';
#endif

              /* Store system log entry counter. */
              *ptr = '\0';
              msa[afd_no].sys_log_ec = (unsigned int)strtoul(ptr_start, NULL, 10);
              ptr++;
              while ((*ptr != '\0') && (i < LOG_FIFO_SIZE))
              {
                 msa[afd_no].sys_log_fifo[i] = *ptr - ' ';
                 if (msa[afd_no].sys_log_fifo[i] > COLOR_POOL_SIZE)
                 {
                    mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                            "Reading garbage for Transfer Log Radar entry <%d>",
                            (int)msa[afd_no].sys_log_fifo[i]);
                    msa[afd_no].sys_log_fifo[i] = NO_INFORMATION;
                 }
#ifdef _DEBUG_PRINT
                 length += sprintf(&fifo_buf[length], " %d",
                                   (int)msa[afd_no].sys_log_fifo[i]);
#endif
                 ptr++; i++;
              }
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
           }
           ret = SUCCESS;
        }
   /*
    * Error list (EL)
    */
   else if ((*ptr == 'E') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: EL <host_number> <error code 1> ... <error code n> */
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int i, k = 0,
                  pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_hosts)
              {
                 ptr = ptr + 1;
                 ptr_start = ptr;
                 while ((*ptr != ' ') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 if (*ptr == ' ')
                 {
                    k = 1;
                    *ptr = '\0';
                    ahl[pos].error_history[0] = (unsigned char)atoi(ptr_start);

                    do
                    {
                       ptr = ptr + 1;
                       ptr_start = ptr;
                       while ((*ptr != ' ') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       if ((*ptr == ' ') && (k < ERROR_HISTORY_LENGTH))
                       {
                          *ptr = '\0';
                          ahl[pos].error_history[k] = (unsigned char)atoi(ptr_start);
                          k++;
                       }
                    } while (*ptr != '\0');
                 }
                 for (i = k; i < ERROR_HISTORY_LENGTH; i++)
                 {
                    ahl[pos].error_history[k] = 0;
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "EL %d %d",
                               pos, ahl[pos].error_history[0]);
                 for (i = 0; i < k; i++)
                 {
                    (void)fprintf(stderr, " %d",
                                  (int)ahl[pos].error_history[i]);
                 }
                 (void)fprintf(stderr, "\n");
#endif
              }
              else
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Hmmm. Trying to insert at position %d, but there are only %d hosts.",
                         pos, msa[afd_no].no_of_hosts);
              }
           }
           ret = SUCCESS;
        }
   /*
    * Host list (HL)
    */
   else if ((*ptr == 'H') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: HL <host_number> <host alias> <real hostname 1> [<real hostname 2>] */
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int i = 0,
                  pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_hosts)
              {
                 ptr = ptr + 1;
                 while ((*ptr != ' ') && (*ptr != '\0') &&
                        (i < MAX_HOSTNAME_LENGTH))
                 {
                    ahl[pos].host_alias[i] = *ptr;
                    ptr++; i++;
                 }
                 if (*ptr == ' ')
                 {
                    ahl[pos].host_alias[i] = '\0';
                    i = 0;
                    ptr = ptr + 1;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                    {
                       ahl[pos].real_hostname[0][i] = *ptr;
                       ptr++; i++;
                    }
                    ahl[pos].real_hostname[0][i] = '\0';
                    if (*ptr == ' ')
                    {
                       i = 0;
                       ptr = ptr + 1;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
                       {
                          ahl[pos].real_hostname[1][i] = *ptr;
                          ptr++; i++;
                       }
                       ahl[pos].real_hostname[1][i] = '\0';
                    }
                    else
                    {
                       ahl[pos].real_hostname[1][0] = '\0';
                    }
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "HL %d %s %s %s\n",
                               pos, ahl[pos].host_alias,
                               ahl[pos].real_hostname[0],
                               ahl[pos].real_hostname[1]);
#endif
              }
              else
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Hmmm. Trying to insert host at position %d, but there are only %d hosts.",
                         pos, msa[afd_no].no_of_hosts);
              }
           }
           ret = SUCCESS;
        }
   /*
    * Directory list (DL)
    */
   else if ((*ptr == 'D') && (*(ptr + 1) == 'L'))
        {
           /* Syntax: DL <dir_number> <dir ID> <dir alias> <dir name> [<original dir name>] */
           ptr += 3;
           ptr_start = ptr;
           while ((*ptr != ' ') && (*ptr != '\0'))
           {
              ptr++;
           }
           if (*ptr == ' ')
           {
              int i = 0,
                  pos;

              *ptr = '\0';
              pos = atoi(ptr_start);
              if (pos < msa[afd_no].no_of_dirs)
              {
                 ptr = ptr + 1;
                 ptr_start = ptr;
                 while ((*ptr != ' ') && (*ptr != '\0') &&
                        (i < MAX_INT_LENGTH))
                 {
                    ptr++; i++;
                 }
                 if (*ptr == ' ')
                 {
                    *ptr = '\0';
                    adl[pos].dir_id = (unsigned int)strtoul(ptr_start, NULL, 16);

                    i = 0;
                    ptr = ptr + 1;
                    while ((*ptr != ' ') && (*ptr != '\0') &&
                           (i < MAX_DIR_ALIAS_LENGTH))
                    {
                       adl[pos].dir_alias[i] = *ptr;
                       ptr++; i++;
                    }
                    if (*ptr == ' ')
                    {
                       adl[pos].dir_alias[i] = '\0';
                       i = 0;
                       ptr = ptr + 1;
                       while ((*ptr != ' ') && (*ptr != '\0') &&
                              (i < (MAX_PATH_LENGTH - 1)))
                       {
                          adl[pos].dir_name[i] = *ptr;
                          ptr++; i++;
                       }
                       adl[pos].dir_name[i] = '\0';
                       if (*ptr == ' ')
                       {
                          i = 0;
                          ptr = ptr + 1;
                          while ((*ptr != ' ') && (*ptr != '\0') &&
                                 (i < (MAX_PATH_LENGTH - 1)))
                          {
                             adl[pos].orig_dir_name[i] = *ptr;
                             ptr++; i++;
                          }
                          adl[pos].orig_dir_name[i] = '\0';
                       }
                       else
                       {
                          adl[pos].orig_dir_name[0] = '\0';
                       }
                    }
                 }
#ifdef _DEBUG_PRINT
                 (void)fprintf(stderr, "DL %d %s %x %s %s\n",
                               pos, adl[pos].dir_id, adl[pos].dir_alias,
                               adl[pos].dir_name, adl[pos].orig_dir_name);
#endif
              }
              else
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Hmmm. Trying to insert directory at position %d, but there are only %d directories.",
                         pos, msa[afd_no].no_of_dirs);
              }
           }
           ret = SUCCESS;
        }
   /*
    * Receive Log History (RH)
    */
   else if ((*ptr == 'R') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'R';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[RECEIVE_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[RECEIVE_HISTORY],
                                 &msa[afd_no].log_history[RECEIVE_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[RECEIVE_HISTORY] = DONE;
                }

           /* Syntax: RH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[RECEIVE_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[RECEIVE_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for Receive Log History <%d>",
                         (int)msa[afd_no].log_history[RECEIVE_HISTORY][i]);
                 msa[afd_no].log_history[RECEIVE_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[RECEIVE_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * Transfer Log History (TH)
    */
   else if ((*ptr == 'T') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'T';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[TRANSFER_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[TRANSFER_HISTORY],
                                 &msa[afd_no].log_history[TRANSFER_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[TRANSFER_HISTORY] = DONE;
                }

           /* Syntax: TH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[TRANSFER_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[TRANSFER_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for Transfer Log History <%d>",
                         (int)msa[afd_no].log_history[TRANSFER_HISTORY][i]);
                 msa[afd_no].log_history[TRANSFER_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[TRANSFER_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * System Log History (SH)
    */
   else if ((*ptr == 'S') && (*(ptr + 1) == 'H'))
        {
           register int i,
                        his_length_received;
#ifdef _DEBUG_PRINT
           int  length = 2;
           char fifo_buf[(MAX_LOG_HISTORY * 4) + 4];

           fifo_buf[0] = 'S';
           fifo_buf[1] = 'H';
#endif
           his_length_received = *bytes_done - 2 - 3;
           if (his_length_received > MAX_LOG_HISTORY)
           {
              his_length_received = MAX_LOG_HISTORY;
           }
           else if ((his_length_received < MAX_LOG_HISTORY) &&
                    (shift_log_his[SYSTEM_HISTORY] == NO) &&
                    (msa[afd_no].last_data_time >= new_hour_time))
                {
                   (void)memmove(msa[afd_no].log_history[SYSTEM_HISTORY],
                                 &msa[afd_no].log_history[SYSTEM_HISTORY][1],
                                 (MAX_LOG_HISTORY - his_length_received));
                   shift_log_his[SYSTEM_HISTORY] = DONE;
                }

           /* Syntax: SH <history data> */;
           ptr += 3;
           for (i = (MAX_LOG_HISTORY - his_length_received); i < MAX_LOG_HISTORY; i++)
           {
              msa[afd_no].log_history[SYSTEM_HISTORY][i] = *ptr - ' ';
              if (msa[afd_no].log_history[SYSTEM_HISTORY][i] > COLOR_POOL_SIZE)
              {
                 mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                         "Reading garbage for System Log History <%d>",
                         (int)msa[afd_no].log_history[SYSTEM_HISTORY][i]);
                 msa[afd_no].log_history[SYSTEM_HISTORY][i] = NO_INFORMATION;
              }
              ptr++;
#ifdef _DEBUG_PRINT
              length += sprintf(&fifo_buf[length], " %d",
                                (int)msa[afd_no].log_history[SYSTEM_HISTORY][i]);
#endif
           }
           ret = SUCCESS;
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", fifo_buf);
#endif
        }
   /*
    * Log capabilities of remote AFD. (LC)
    */
   else if ((*ptr == 'L') && (*(ptr + 1) == 'C'))
        {
           if ((*bytes_done - 3) < MAX_INT_LENGTH)
           {
              msa[afd_no].log_capabilities = atoi(ptr + 3);
              got_log_capabilities = YES;
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "LC %d\n", msa[afd_no].log_capabilities);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Log capabilities is %d bytes long, but can handle only %d bytes.",
                      *bytes_done - 3, MAX_INT_LENGTH);
           }
           ret = SUCCESS;
        }
   /*
    * AFD Version Number (AV)
    */
   else if ((*ptr == 'A') && (*(ptr + 1) == 'V'))
        {
           if ((*bytes_done - 3) < MAX_VERSION_LENGTH)
           {
              (void)strcpy(msa[afd_no].afd_version, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "AV %s\n", msa[afd_no].afd_version);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Version is %d Bytes long, but can handle only %d Bytes.",
                      *bytes_done - 3, MAX_VERSION_LENGTH);
           }
           ret = SUCCESS;
        }
   /*
    * Remote AFD working directory. (WD)
    */
   else if ((*ptr == 'W') && (*(ptr + 1) == 'D'))
        {
           if ((*bytes_done - 3) < MAX_PATH_LENGTH)
           {
              (void)strcpy(msa[afd_no].r_work_dir, ptr + 3);
#ifdef _DEBUG_PRINT
              (void)fprintf(stderr, "WD %s\n", msa[afd_no].r_work_dir);
#endif
           }
           else
           {
              mon_log(WARN_SIGN, __FILE__, __LINE__, 0L, msg_str,
                      "Path is %d Bytes long, but can handle only %d Bytes.",
                      *bytes_done - 3, MAX_PATH_LENGTH);
           }
           ret = SUCCESS;
        }
   else if ((isdigit(*ptr)) && (isdigit(*(ptr + 1))) &&
            (isdigit(*(ptr + 2))) && (*(ptr + 3) == '-'))
        {
           ret = ((*ptr - '0') * 100) + ((*(ptr + 1) - '0') * 10) +
                 (*(ptr + 2) - '0');
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif
        }
   else if (strcmp(ptr, AFDD_SHUTDOWN_MESSAGE) == 0)
        {
#ifdef _DEBUG_PRINT
           (void)fprintf(stderr, "%s\n", ptr);
#endif
           mon_log(WARN_SIGN, NULL, 0, 0L, NULL,
                   "========> AFDD SHUTDOWN <========");
           ret = AFDD_SHUTTING_DOWN;
           timeout_flag = ON;
           (void)tcp_quit();
           timeout_flag = OFF;
           msa[afd_no].connect_status = DISCONNECTED;
        }
        else
        {
           mon_log(ERROR_SIGN, __FILE__, __LINE__, 0L, msg_str,
                   "Failed to evaluate message.");
           ret = UNKNOWN_MESSAGE;
        }

   return(ret);
}
