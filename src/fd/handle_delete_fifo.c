/*
 *  handle_delete_fifo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_delete_fifo - handles the deleting of jobs/files
 **
 ** SYNOPSIS
 **   void handle_delete_fifo(int    delete_jobs_fd,
 **                           size_t fifo_size,
 **                           char   *file_dir)
 **
 ** DESCRIPTION
 **   The function handle_delete_fifo handles the deleting of single files,
 **   all files from a certain host and all files belonging to a message/
 **   job. Data comes from a fifo and has a the following format:
 **
 **     DELETE_ALL_JOBS_FROM_HOST: <type><hast alias>\0
 **     DELETE_MESSAGE           : <type><message name>\0
 **     DELETE_SINGLE_FILE       : <type><message name + file name>\0
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.01.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd,
                                  *no_msg_queued,
                                  no_of_hosts;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct connection          *connection;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif


/*######################### handle_delete_fifo() ########################*/
void
handle_delete_fifo(int delete_jobs_fd, size_t fifo_size, char *file_dir)
{
   static int  del_bytes_buffered,
               del_bytes_read = 0;
   static char *del_fifo_buffer = NULL,
               *del_read_ptr;

   if (del_fifo_buffer == NULL)
   {
      if ((del_fifo_buffer = malloc(fifo_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d bytes] : %s",
                    fifo_size, strerror(errno));
         exit(INCORRECT);
      }
   }

   if (del_bytes_read == 0)
   {
      del_bytes_buffered = 0;
   }
   else
   {
      (void)memmove(del_fifo_buffer, del_read_ptr, del_bytes_read);
      del_bytes_buffered = del_bytes_read;
   }
   del_read_ptr = del_fifo_buffer;

   if ((del_bytes_read = read(delete_jobs_fd,
                              &del_fifo_buffer[del_bytes_buffered],
                              (fifo_size - del_bytes_buffered))) > 0)
   {
      int    length;
      time_t now;
      char   *p_str_end;

      now = time(NULL);
      del_bytes_read += del_bytes_buffered;
      do
      {
         if ((*del_read_ptr == DELETE_ALL_JOBS_FROM_HOST) ||
             (*del_read_ptr == DELETE_MESSAGE) ||
             (*del_read_ptr == DELETE_SINGLE_FILE))
         {
            p_str_end = del_read_ptr + 1;
            while ((*p_str_end != '\0') &&
                   ((p_str_end - del_read_ptr) < (del_bytes_read - 1)))
            {
               p_str_end++;
            }
            if (*p_str_end == '\0')
            {
               p_str_end++;
               length = p_str_end - (del_read_ptr + 1);
            }
            else
            {
               length = 0;
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Unknown identifier %d, deleting everything from fifo buffer.",
                       (int)*del_read_ptr);
            del_bytes_read = 0;
            length = 0;
         }
         if (length > 0)
         {
            int    i;
            if (*del_read_ptr == DELETE_ALL_JOBS_FROM_HOST)
            {
               int  fsa_pos,
                    j;
               char *p_host_stored;

               del_read_ptr++;
               del_bytes_read--;
               for (i = 0; i < *no_msg_queued; i++)
               {
                  if (qb[i].msg_name[0] != '\0')
                  {
                     p_host_stored = mdb[qb[i].pos].host_name;
                  }
                  else
                  {
                     p_host_stored = fra[qb[i].pos].host_alias;
                  }
                  if (CHECK_STRCMP(p_host_stored, del_read_ptr) == 0)
                  {
                     /*
                      * Kill the job when it is currently
                      * distributing data.
                      */
                     if (qb[i].pid > 0)
                     {
                        if (kill(qb[i].pid, SIGKILL) < 0)
                        {
                           if (errno != ESRCH)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         "Failed to kill transfer job to %s (%d) : %s",
                                         p_host_stored,
                                         qb[i].pid, strerror(errno));
                           }
                        }
                        else
                        {
                           remove_connection(&connection[qb[i].connect_pos],
                                             NO, now);
                        }
                     }

                     if (qb[i].msg_name[0] != '\0')
                     {
                        char *ptr;

                        ptr = file_dir + strlen(file_dir);
                        (void)strcpy(ptr, qb[i].msg_name);
#ifdef _DELETE_LOG
                        remove_files(file_dir, mdb[qb[i].pos].fsa_pos,
                                     mdb[qb[i].pos].job_id, USER_DEL);
#else
                        remove_files(file_dir, -1);
#endif
                        *ptr = '\0';
                        fsa_pos = mdb[qb[i].pos].fsa_pos;
                     }
                     else
                     {
                        fsa_pos = fra[qb[i].pos].fsa_pos;
                     }

                     if (qb[i].pid < 1)
                     {
                        ABS_REDUCE(fsa_pos);
                     }
                     remove_msg(i);
                     if (i < *no_msg_queued)
                     {
                        i--;
                     }
                  }
               } /* for (i = 0; i < *no_msg_queued; i++) */

               /*
                * Hmmm. Best is to reset ALL values, so we do not
                * need to start and stop the FD to sort out any
                * problems in the FSA.
                */
               if ((fsa_pos = get_host_position(fsa, del_read_ptr,
                                                no_of_hosts)) != INCORRECT)
               {
                  fsa[fsa_pos].total_file_counter = 0;
                  fsa[fsa_pos].total_file_size = 0;
                  fsa[fsa_pos].active_transfers = 0;
                  fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
                  fsa[fsa_pos].mc_nack_counter = 0;
                  fsa[fsa_pos].mc_ctrl_per_process = fsa[fsa_pos].mc_ct_rate_limit = fsa[fsa_pos].transfer_rate_limit;
                  fsa[fsa_pos].error_counter = 0;
                  fsa[fsa_pos].jobs_queued = 0;
                  for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
                  {
                     fsa[fsa_pos].job_status[j].no_of_files = 0;
                     fsa[fsa_pos].job_status[j].proc_id = -1;
                     fsa[fsa_pos].job_status[j].connect_status = DISCONNECT;
                     fsa[fsa_pos].job_status[j].file_name_in_use[0] = '\0';
                  }
                  for (j = 0; j < ERROR_HISTORY_LENGTH; j++)
                  {
                     fsa[fsa_pos].error_history[j] = 0;
                  }
               }
            }
            else if (*del_read_ptr == DELETE_MESSAGE)
                 {
                    del_read_ptr++;
                    del_bytes_read--;
                    for (i = 0; i < *no_msg_queued; i++)
                    {
                       if (CHECK_STRCMP(qb[i].msg_name, del_read_ptr) == 0)
                       {
                          char *ptr;

                          /*
                           * Kill the job when it is currently
                           * distributing data.
                           */
                          if (qb[i].pid > 0)
                          {
                             if (kill(qb[i].pid, SIGKILL) < 0)
                             {
                                if (errno != ESRCH)
                                {
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to kill transfer job to %s (%d) : %s",
                                              mdb[qb[i].pos].host_name,
                                              qb[i].pid, strerror(errno));
                                }
                             }
                             else
                             {
                                remove_connection(&connection[qb[i].connect_pos],
                                                  NO, now);
                             }
                          }

                          ptr = file_dir + strlen(file_dir);
                          (void)strcpy(ptr, qb[i].msg_name);
#ifdef _DELETE_LOG
                          remove_files(file_dir, mdb[qb[i].pos].fsa_pos,
                                       mdb[qb[i].pos].job_id, USER_DEL);
#else
                          remove_files(file_dir, -1);
#endif
                          *ptr = '\0';

                          if (qb[i].pid < 1)
                          {
                             ABS_REDUCE(mdb[qb[i].pos].fsa_pos);
                          }
                          remove_msg(i);
                          break;
                       }
                    } /* for (i = 0; i < *no_msg_queued; i++) */
                 }
            else if (*del_read_ptr == DELETE_SINGLE_FILE)
                 {
                    char *p_end;

                    del_read_ptr++;
                    del_bytes_read--;
                    p_end = del_read_ptr;
                    while ((*p_end != '/') && (*p_end != '\0'))
                    {
                       p_end++; /* Away with job ID. */
                    }
                    if (*p_end == '/')
                    {
                       p_end++;
                       while ((*p_end != '/') && (*p_end != '\0'))
                       {
                          p_end++; /* Away with the dir number. */
                       }
                       if (*p_end == '/')
                       {
                          p_end++;
                          while ((*p_end != '_') && (*p_end != '\0'))
                          {
                             p_end++; /* Away with date */
                          }
                          if (*p_end == '_')
                          {
                             p_end++;
                             while ((*p_end != '_') && (*p_end != '\0'))
                             {
                                p_end++; /* Away with unique number */
                             }
                             if (*p_end == '_')
                             {
                                p_end++;
                                while ((*p_end != '/') && (*p_end != '\0'))
                                {
                                   p_end++; /* Away with split job counter */
                                }
                                if (*p_end == '/')
                                {
                                   char tmp_char;

                                   tmp_char = *p_end;
                                   *p_end = '\0';
                                   for (i = 0; i < *no_msg_queued; i++)
                                   {
                                      if (CHECK_STRCMP(qb[i].msg_name, del_read_ptr) == 0)
                                      {
                                         /*
                                          * Do not delete when file
                                          * is currently being processed.
                                          */
                                         if (qb[i].pid == PENDING)
                                         {
                                            char        *ptr;
                                            struct stat stat_buf;

                                            *p_end = tmp_char;
                                            ptr = file_dir +
                                                  strlen(file_dir);
                                            (void)strcpy(ptr, del_read_ptr);
                                            if (stat(file_dir, &stat_buf) != -1)
                                            {
                                               if (unlink(file_dir) == -1)
                                               {
                                                  if (errno != ENOENT)
                                                  {
                                                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                                "Failed to unlink() `%s' : %s",
                                                                file_dir, strerror(errno));
                                                  }
                                               }
                                               else
                                               {
#ifdef _DELETE_LOG
                                                  size_t dl_real_size;
#endif
                                                  off_t        lock_offset;

                                                  qb[i].files_to_send--;
                                                  qb[i].file_size_to_send -= stat_buf.st_size;
#ifdef _DELETE_LOG
                                                  (void)strcpy(dl.file_name, p_end + 1);
                                                  if (mdb[qb[i].pos].fsa_pos > -1)
                                                  {
                                                     lock_offset = AFD_WORD_OFFSET +
                                                                   (mdb[qb[i].pos].fsa_pos * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
                                                     lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                                                     lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
                                                     fsa[mdb[qb[i].pos].fsa_pos].total_file_counter -= 1;
#ifdef _VERIFY_FSA
                                                     if (fsa[mdb[qb[i].pos].fsa_pos].total_file_counter < 0)
                                                     {
                                                        system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                   "Total file counter for host `%s' less then zero. Correcting.",
                                                                   fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                        fsa[mdb[qb[i].pos].fsa_pos].total_file_counter = 0;
                                                     }
#endif
                                                     fsa[mdb[qb[i].pos].fsa_pos].total_file_size -= stat_buf.st_size;
#ifdef _VERIFY_FSA
                                                     if (fsa[mdb[qb[i].pos].fsa_pos].total_file_size < 0)
                                                     {
                                                        system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                   "Total file size for host `%s' overflowed. Correcting.",
                                                                   fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                        fsa[mdb[qb[i].pos].fsa_pos].total_file_size = 0;
                                                     }
                                                     else if ((fsa[mdb[qb[i].pos].fsa_pos].total_file_counter == 0) &&
                                                              (fsa[mdb[qb[i].pos].fsa_pos].total_file_size > 0))
                                                          {
                                                             system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                        "fc for host `%s' is zero but fs is not zero. Correcting.",
                                                                        fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name);
                                                             fsa[mdb[qb[i].pos].fsa_pos].total_file_size = 0;
                                                          }
#endif
#ifdef LOCK_DEBUG
                                                     unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
                                                     unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif

                                                     (void)sprintf(dl.host_name, "%-*s %x",
                                                                   MAX_HOSTNAME_LENGTH,
                                                                   fsa[mdb[qb[i].pos].fsa_pos].host_dsp_name,
                                                                   USER_DEL);
                                                  }
                                                  else
                                                  {
                                                     (void)sprintf(dl.host_name, "%-*s %x",
                                                                   MAX_HOSTNAME_LENGTH,
                                                                   "-",
                                                                   USER_DEL);
                                                  }
                                                  *dl.file_size = stat_buf.st_size;
                                                  *dl.job_number = mdb[qb[i].pos].job_id;
                                                  *dl.file_name_length = strlen(p_end + 1);
                                                  (void)strcpy((dl.file_name + *dl.file_name_length + 1), "FD");
                                                  dl_real_size = *dl.file_name_length + dl.size + 2 /* FD */;
                                                  if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                                  {
                                                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                                "write() error : %s", strerror(errno));
                                                  }
#endif
                                                  if (qb[i].files_to_send == 0)
                                                  {
                                                     fsa[mdb[qb[i].pos].fsa_pos].jobs_queued--;
                                                     if (i != (*no_msg_queued - 1))
                                                     {
                                                        (void)memmove(&qb[i], &qb[i + 1],
                                                                      sizeof(struct queue_buf));
                                                     }
                                                     (*no_msg_queued)--;
                                                  }
                                               }
                                            }
                                            else
                                            {
                                               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                          "Failed to stat() `%s' : %s",
                                                          file_dir, strerror(errno));
                                            }
                                            *ptr = '\0';
                                         }
                                         break;
                                      }
                                   } /* for (i = 0; i < *no_msg_queued; i++) */
                                }
                                else
                                {
                                   system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                              "Reading garbage on FD delete fifo (%c).",
                                              *p_end);
                                }
                             }
                             else
                             {
                                system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                           "Reading garbage on FD delete fifo (%c).",
                                           *p_end);
                             }
                          }
                          else
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Reading garbage on FD delete fifo (%c).",
                                        *p_end);
                          }
                       }
                       else
                       {
                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Reading garbage on FD delete fifo (%c).",
                                     *p_end);
                       }
                    }
                    else
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "Reading garbage on FD delete fifo (%c).",
                                  *p_end);
                    }
                 }
                 else
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Unknown identifier %d, deleting everything from fifo buffer.",
                               (int)*del_read_ptr);
                    del_bytes_read = 0;
                 }
            del_bytes_read -= length;
            del_read_ptr = p_str_end;
         } /* if (length > 0) */
      } while ((del_bytes_read > 0) && (length != 0));
   }
   else if (del_bytes_read == -1)
        {
           system_log(ERROR_SIGN, __FILE__, __LINE__,
                      "read() error : %s", strerror(errno));
           del_bytes_read = 0;
        }

   return;
}
