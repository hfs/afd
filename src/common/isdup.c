/*
 *  isdup.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   isdup - checks for duplicates
 **
 ** SYNOPSIS
 **   int  isdup(char *fullname, off_t size, unsigned int id, time_t timeout,
 **              int flag, int rm_flag, int have_hw_crc32, int stay_attached,
 **              int lock)
 **   void isdup_detach(void)
 **
 ** DESCRIPTION
 **   The function isdup() checks if this file is a duplicate.
 **
 ** RETURN VALUES
 **   isdup() returns YES if the file is duplicate, otherwise NO is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.06.2005 H.Kiehl Created
 **   09.03.2006 H.Kiehl Added parameter to remove a CRC.
 **   20.12.2006 H.Kiehl Added option to ignore last suffix of filename.
 **   05.03.2008 H.Kiehl Added option to test for filename and size.
 **   19.11.2012 H.Kiehl Added option to stay attached.
 **   21.11.2012 H.Kiehl Added option to disable locking.
 **   23.11.2012 H.Kiehl Included support for another CRC-32 algoritm
 **                      which also supports SSE4.2 on some CPU's.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;

/* Local global variables. */
static int            cdb_fd = -1,
                      *no_of_crcs;
static struct crc_buf *cdb = NULL;


/*############################### isdup() ###############################*/
int
isdup(char         *fullname,
      off_t        size,
      unsigned int id,
      time_t       timeout,
      int          flag,
      int          rm_flag,
#ifdef HAVE_HW_CRC32
      int          have_hw_crc32,
#endif
      int          stay_attached,
      int          lock)
{
   int          dup,
                i;
   unsigned int crc;
   time_t       current_time;
   char         crcfile[MAX_PATH_LENGTH],
                *ptr;

   if (flag & DC_FILENAME_ONLY)
   {
      char *p_end;

      p_end = ptr = fullname + strlen(fullname);
      while ((*ptr != '/') && (ptr != fullname))
      {
         ptr--;
      }
      if (*ptr == '/')
      {
         ptr++;
         if (flag & DC_CRC32C)
         {
            crc = get_checksum_crc32c(INITIAL_CRC, ptr,
#ifdef HAVE_HW_CRC32
                                      (p_end - ptr), have_hw_crc32);
#else
                                      (p_end - ptr));
#endif
         }
         else
         {
            crc = get_checksum(INITIAL_CRC, ptr, (p_end - ptr));
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Unable to find filename in `%s'."), fullname);
         return(NO);
      }
   }
   else if (flag & DC_FILENAME_AND_SIZE)
        {
           char filename_size[MAX_FILENAME_LENGTH + 1 + MAX_OFF_T_LENGTH + 1];

           ptr = fullname + strlen(fullname);
           while ((*ptr != '/') && (ptr != fullname))
           {
              ptr--;
           }
           if (*ptr == '/')
           {
              ptr++;
              i = 0;
              while ((*ptr != '\0') && (i < MAX_FILENAME_LENGTH))
              {
                 filename_size[i] = *ptr;
                 i++; ptr++;
              }
              filename_size[i++] = ' ';
              (void)memcpy(&filename_size[i], &size, sizeof(off_t));
              if (flag & DC_CRC32C)
              {
                 crc = get_checksum_crc32c(INITIAL_CRC, filename_size,
#ifdef HAVE_HW_CRC32
                                           i + sizeof(off_t), have_hw_crc32);
#else
                                           i + sizeof(off_t));
#endif
              }
              else
              {
                 crc = get_checksum(INITIAL_CRC, filename_size,
                                    i + sizeof(off_t));
              }
           }
           else
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Unable to find filename in `%s'."), fullname);
              return(NO);
           }
        }
   else if (flag & DC_NAME_NO_SUFFIX)
        {
           char *p_end;

           p_end = ptr = fullname + strlen(fullname);
           while ((*ptr != '.') && (*ptr != '/') && (ptr != fullname))
           {
              ptr--;
           }
           if (*ptr == '.')
           {
              p_end = ptr;
           }
           while ((*ptr != '/') && (ptr != fullname))
           {
              ptr--;
           }
           if (*ptr == '/')
           {
              ptr++;
              if (flag & DC_CRC32C)
              {
                 crc = get_checksum_crc32c(INITIAL_CRC, ptr,
#ifdef HAVE_HW_CRC32
                                           (p_end - ptr), have_hw_crc32);
#else
                                           (p_end - ptr));
#endif
              }
              else
              {
                 crc = get_checksum(INITIAL_CRC, ptr, (p_end - ptr));
              }
           }
           else
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Unable to find filename in `%s'."), fullname);
              return(NO);
           }
        }
   else if (flag & DC_FILE_CONTENT)
        {
           int  fd,
                ret;
           char buffer[4096];

           if ((fd = open(fullname, O_RDONLY)) == -1)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to open() `%s' : %s"),
                         fullname, strerror(errno));
              return(NO);
           }

           if (flag & DC_CRC32C)
           {
              ret = get_file_checksum_crc32c(fd, buffer, 4096, 0,
#ifdef HAVE_HW_CRC32
                                             &crc, have_hw_crc32);
#else
                                             &crc);
#endif
           }
           else
           {
              ret = get_file_checksum(fd, buffer, 4096, 0, &crc);
           }
           if (ret != SUCCESS)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to determine checksum for `%s'."), fullname);
              (void)close(fd);
              return(NO);
           }

           if (close(fd) == -1)
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         _("Failed to close() `%s' : %s"),
                         fullname, strerror(errno));
           }
        }
   else if (flag & DC_FILE_CONT_NAME)
        {
           char *p_end;

           p_end = ptr = fullname + strlen(fullname);
           while ((*ptr != '/') && (ptr != fullname))
           {
              ptr--;
           }
           if (*ptr == '/')
           {
              int  fd,
                   ret;
              char buffer[4096];

              ptr++;
              (void)memcpy(buffer, ptr, (p_end - ptr));

              if ((fd = open(fullname, O_RDONLY)) == -1)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Failed to open() `%s' : %s"),
                            fullname, strerror(errno));
                 return(NO);
              }

              if (flag & DC_CRC32C)
              {
                 ret = get_file_checksum_crc32c(fd, buffer, 4096, (p_end - ptr),
#ifdef HAVE_HW_CRC32
                                                &crc, have_hw_crc32);
#else
                                                &crc);
#endif
              }
              else
              {
                 ret = get_file_checksum(fd, buffer, 4096, (p_end - ptr), &crc);
              }
              if (ret != SUCCESS)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Failed to determine checksum for `%s'."),
                            fullname);
                 (void)close(fd);
                 return(NO);
              }

              if (close(fd) == -1)
              {
                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                            _("Failed to close() `%s' : %s"),
                            fullname, strerror(errno));
              }
           }
           else
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Unable to find filename in `%s'."), fullname);
              return(NO);
           }
        }

   current_time = time(NULL);
   if ((stay_attached == NO) || (cdb == NULL))
   {
      size_t new_size;

      /*
       * Map to checksum file of this job. If it does not exist create it.
       */
      (void)sprintf(crcfile, "%s%s%s/%x", p_work_dir, AFD_FILE_DIR, CRC_DIR, id);
      new_size = (CRC_STEP_SIZE * sizeof(struct crc_buf)) + AFD_WORD_OFFSET;
      if ((ptr = attach_buf(crcfile, &cdb_fd, &new_size,
                            (lock == YES) ? "isdup()" : NULL,
                            FILE_MODE, YES)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to mmap() `%s' : %s"), crcfile, strerror(errno));
         return(NO);
      }
      no_of_crcs = (int *)ptr;

      /* Check that time has sane value (it's not initialized). */
      if ((*(time_t *)(ptr + SIZEOF_INT + 4) < 100000) ||
          (*(time_t *)(ptr + SIZEOF_INT + 4) > (current_time - DUPCHECK_MIN_CHECK_TIME)))
      {
         *(time_t *)(ptr + SIZEOF_INT + 4) = current_time;
      }

      ptr += AFD_WORD_OFFSET;
      cdb = (struct crc_buf *)ptr;
   }
#ifdef THIS_DOES_NOT_WORK
   else if (lock == YES)
        {
#ifdef LOCK_DEBUG
           lock_region_w(cdb_fd, 0, __FILE__, __LINE__);
#else
           lock_region_w(cdb_fd, 0);
#endif
        }
#endif /* THIS_DOES_NOT_WORK */
   dup = NO;

   if (rm_flag != YES)
   {
      /*
       * At certain intervalls always check if the crc values we have
       * stored have not timed out. If so remove them from the list.
       */
      if (current_time > *(time_t *)((char *)cdb + SIZEOF_INT + 4))
      {
         time_t dupcheck_check_time;

         if (timeout > DUPCHECK_MAX_CHECK_TIME)
         {
            dupcheck_check_time = DUPCHECK_MAX_CHECK_TIME;
         }
         else if (timeout < DUPCHECK_MIN_CHECK_TIME)
              {
                 dupcheck_check_time = DUPCHECK_MIN_CHECK_TIME;
              }
              else
              {
                 dupcheck_check_time = timeout;
              }

         for (i = 0; i < *no_of_crcs; i++)
         {
            if (cdb[i].timeout <= current_time)
            {
               int end_pos = i;

               while ((++end_pos <= *no_of_crcs) &&
                      (cdb[end_pos - 1].timeout <= current_time))
               {
                  /* Nothing to be done. */;
               }
               if (end_pos <= *no_of_crcs)
               {
                  size_t move_size = (*no_of_crcs - (end_pos - 1)) *
                                     sizeof(struct crc_buf);

                  (void)memmove(&cdb[i], &cdb[end_pos - 1], move_size);
               }
               (*no_of_crcs) -= (end_pos - 1 - i);
            }
         }
         *(time_t *)((char *)cdb + SIZEOF_INT + 4) =
                                      ((time(NULL) / dupcheck_check_time) *
                                       dupcheck_check_time) +
                                      dupcheck_check_time;
      }

      if (timeout > 0L)
      {
         for (i = 0; i < *no_of_crcs; i++)
         {
            if ((crc == cdb[i].crc) && (flag == cdb[i].flag))
            {
               if (current_time <= cdb[i].timeout)
               {
                  dup = YES;
               }
               else
               {
                  dup = NEITHER;
               }
               cdb[i].timeout = current_time + timeout;
               break;
            }
         }
      }

      if (dup == NO)
      {
         if ((*no_of_crcs != 0) &&
             ((*no_of_crcs % CRC_STEP_SIZE) == 0))
         {
            size_t new_size;

            new_size = (((*no_of_crcs / CRC_STEP_SIZE) + 1) *
                        CRC_STEP_SIZE * sizeof(struct crc_buf)) +
                       AFD_WORD_OFFSET;
            ptr = (char *)cdb - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(cdb_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("mmap_resize() error : %s"), strerror(errno));
               (void)close(cdb_fd);
               return(NO);
            }
            no_of_crcs = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            cdb = (struct crc_buf *)ptr;
         }
         cdb[*no_of_crcs].crc = crc;
         cdb[*no_of_crcs].flag = flag;
         cdb[*no_of_crcs].timeout = current_time + timeout;
         (*no_of_crcs)++;
      }
      else if (dup == NEITHER)
           {
              dup = NO;
           }
   }
   else
   {
      if (timeout > 0L)
      {
         for (i = 0; i < *no_of_crcs; i++)
         {
            if ((crc == cdb[i].crc) && (flag == cdb[i].flag))
            {
               if (i <= *no_of_crcs)
               {
                  size_t move_size = (*no_of_crcs - (i - 1)) *
                                     sizeof(struct crc_buf);

                  (void)memmove(&cdb[i], &cdb[i - 1], move_size);
               }
               (*no_of_crcs) -= 1;
               break;
            }
         }
      }
   }

   if (stay_attached == NO)
   {
      unmap_data(cdb_fd, (void *)&cdb);
      cdb_fd = -1;

      /* Note: cdb is set to NULL by unmap_data(). */
   }
#ifdef THIS_DOES_NOT_WORK
   else if (lock == YES)
        {
#ifdef LOCK_DEBUG
           unlock_region(cdb_fd, 0, __FILE__, __LINE__);
#else                                                                   
           unlock_region(cdb_fd, 0);
#endif
        }
#endif /* THIS_DOES_NOT_WORK */

   return(dup);
}


/*############################ isdup_detach() ###########################*/
void
isdup_detach(void)
{
   if (cdb != NULL)
   {
      unmap_data(cdb_fd, (void *)&cdb);
      cdb_fd = -1;
   }

   return;
}
