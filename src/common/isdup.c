/*
 *  isdup.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   int isdup(char *fullname, off_t size, unsigned int id, time_t timeout,
 **             int flag, int rm_flag)
 **
 ** DESCRIPTION
 **   The function isdup() checks if this file is a duplicate.
 **
 ** RETURN VALUES
 **   If n is not reached SUCCESS will be returned, otherwise -1 is
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


/*############################### isdup() ###############################*/
int
isdup(char         *fullname,
      off_t        size,
      unsigned int id,
      time_t       timeout,
      int          flag,
      int          rm_flag)
{
   int            dup,
                  fd,
                  i,
                  *no_of_crcs;
   unsigned int   crc;
   size_t         new_size;
   time_t         current_time;
   char           crcfile[MAX_PATH_LENGTH],
                  *ptr;
   struct crc_buf *cdb;

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
         crc = get_checksum(ptr, (p_end - ptr));
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
              crc = get_checksum(filename_size, i + sizeof(off_t));
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
              crc = get_checksum(ptr, (p_end - ptr));
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
           char buffer[4096];

           if ((fd = open(fullname, O_RDONLY)) == -1)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to open() `%s' : %s"),
                         fullname, strerror(errno));
              return(NO);
           }

           if (get_file_checksum(fd, buffer, 4096, 0, &crc) != SUCCESS)
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

              if (get_file_checksum(fd, buffer, 4096,
                                    (p_end - ptr), &crc) != SUCCESS)
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

   /*
    * Map to checksum file of this job. If it does not exist create it.
    */
   (void)sprintf(crcfile, "%s%s%s/%x", p_work_dir, AFD_FILE_DIR, CRC_DIR, id);
   new_size = (CRC_STEP_SIZE * sizeof(struct crc_buf)) + AFD_WORD_OFFSET;
   if ((ptr = attach_buf(crcfile, &fd, &new_size, "isdup()",
                         FILE_MODE, YES)) == (caddr_t) -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to mmap() `%s' : %s"), crcfile, strerror(errno));
      return(NO);
   }
   current_time = time(NULL);
   no_of_crcs = (int *)ptr;

   /* Check check time has sane value (it's not initialized). */
   if ((*(time_t *)(ptr + SIZEOF_INT + 4) < 100000) ||
       (*(time_t *)(ptr + SIZEOF_INT + 4) > (current_time - DUPCHECK_CHECK_TIME)))
   {
      *(time_t *)(ptr + SIZEOF_INT + 4) = current_time;
   }
   ptr += AFD_WORD_OFFSET;
   cdb = (struct crc_buf *)ptr;
   dup = NO;

   if (rm_flag != YES)
   {
      /*
       * At certain intervalls always check if the crc values we have
       * stored have not timed out. If so remove them from the list.
       */
      if (current_time > *(time_t *)(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 4))
      {
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
         *(time_t *)(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 4) =
                                             ((time(NULL) / DUPCHECK_CHECK_TIME) *
                                              DUPCHECK_CHECK_TIME) +
                                             DUPCHECK_CHECK_TIME;
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
            new_size = (((*no_of_crcs / CRC_STEP_SIZE) + 1) *
                        CRC_STEP_SIZE * sizeof(struct crc_buf)) +
                       AFD_WORD_OFFSET;
            ptr = (char *)cdb - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("mmap_resize() error : %s"), strerror(errno));
               (void)close(fd);
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

   unmap_data(fd, (void *)&cdb);

   return(dup);
}
