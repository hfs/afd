/*
 *  show_shm.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_shm - shows contents of all shared memory region in the AMG
 **
 ** SYNOPSIS
 **   void show_shm(FILE *output)
 **
 ** DESCRIPTION
 **   The function show_shm() is used for debugging only. It shows
 **   the contents of all shared memory regions of the AFD. The
 **   contents is written to the file descriptor 'output'.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.10.1995 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* fprintf()                            */
#include <string.h>             /* strcpy(), memcpy(), strerror(),      */
                                /* free()                               */
#include <stdlib.h>             /* calloc()                             */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

extern int  shm_id,                    /* The shared memory ID's for    */
                                       /* the sorted data and pointers. */
            data_length;               /* The size of data for each     */
                                       /* main sorted data and pointers.*/


#ifdef _DEBUG
/*############################# show_shm() #############################*/
void
show_shm(FILE *output)
{
   int            i,
                  j,
                  k,
                  no_of_jobs,
                  value,
                  size;
   char           *ptr = NULL,
                  *tmp_ptr = NULL,
                  *p_offset = NULL,
                  *p_shm = NULL,
                  region[4];
   struct p_array *p_ptr;

   /* Show what is stored in the shared memory area */
   (void)fprintf(output, "\n\n=================> Contents of shared memory area <=================\n");
   (void)fprintf(output, "                   ==============================\n");

   (void)strcpy(region, "IT");

   /* First show which table we are showing */
   (void)fprintf(output, "\n\n------------------------> Contents of %3s <-------------------------\n", region);
   (void)fprintf(output, "                          ---------------\n");

   if (data_length > 0)
   {
      /* Attach to shared memory regions */
      if ((p_shm = shmat(shm_id, 0, 0)) == (void *) -1)
      {
         (void)fprintf(stderr, "ERROR  : Could not attach to shared memory region : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      ptr = p_shm;

      /* First get the no of jobs */
      no_of_jobs = *(int *)ptr;
      ptr += sizeof(int);

      /* Create and copy pointer array */
      size = no_of_jobs * sizeof(struct p_array);
      if ((tmp_ptr = calloc(no_of_jobs, sizeof(struct p_array))) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : Could not allocate memory : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      p_ptr = (struct p_array *)tmp_ptr;
      memcpy(p_ptr, ptr, size);
      p_offset = ptr + size;

      /* Now show each job */
      for (j = 0; j < no_of_jobs; j++)
      {
         /* Show directory, priority and job type */
         (void)fprintf(output, "Directory          : %s\n", (int)(p_ptr[j].ptr[1]) + p_offset);
         (void)fprintf(output, "Priority           : %c\n", *((int)(p_ptr[j].ptr[0]) + p_offset));

         /* Show files to be send */
         value = atoi((int)(p_ptr[j].ptr[2]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[3]) + p_offset;
         for (k = 0; k < value; k++)
         {
            (void)fprintf(output, "File            %3d: %s\n", k + 1, ptr);
            while (*(ptr++) != '\0')
               ;
         }

         /* Show recipient(s) */
         value = atoi((int)(p_ptr[j].ptr[8]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[9]) + p_offset;
         for (k = 0; k < value; k++)
         {
            (void)fprintf(output, "Recipient       %3d: %s\n", k + 1, ptr);
            while (*(ptr++) != '\0')
               ;
         }

         /* Show local options */
         value = atoi((int)(p_ptr[j].ptr[4]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[5]) + p_offset;
         for (k = 0; k < value; k++)
         {
            (void)fprintf(output, "Local option    %3d: %s\n", k + 1, ptr);
            while (*(ptr++) != '\0')
               ;
         }

         /* Show standard options */
         value = atoi((int)(p_ptr[j].ptr[6]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[7]) + p_offset;
         for (k = 0; k < value; k++)
         {
            (void)fprintf(output, "Standard option %3d: %s\n", k + 1, ptr);
            while (*(ptr++) != '\0')
               ;
         }

         (void)fprintf(output, ">------------------------------------------------------------------------<\n\n");
      }

      /* Detach shared memory regions */
      if (shmdt(p_shm) < 0)
      {
         (void)fprintf(stderr, "WARNING: eval_dir_config() could not detach shared memory region. (%s %d)\n", __FILE__, __LINE__);
      }

      /* Free memory for pointer array */
      free(tmp_ptr);
   }
   else
   {
      (void)fprintf(output, "\n                >>>>>>>>>> NO DATA FOR %s <<<<<<<<<<\n\n", region);
   }

   return;
}
#endif
