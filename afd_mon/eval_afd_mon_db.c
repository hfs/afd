/*
 *  eval_afd_mon_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_afd_mon_db - reads the AFD_MON_CONFIG file and stores the
 **                     contents in the structure mon_status_area
 **
 ** SYNOPSIS
 **   void eval_afd_mon_db(struct mon_status_area **nml)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.1998 H.Kiehl Created
 **   24.01.1999 H.Kiehl Added remote user name.
 **
 */
DESCR__E_M3

#include <string.h>             /* strerror(), strcpy()                  */
#include <ctype.h>              /* isdigit()                             */
#include <stdlib.h>             /* malloc(), free(), atoi()              */
#include <errno.h>
#include "mondefs.h"
#include "afdddefs.h"

#define MEM_STEP_SIZE 20

/* External global variables */
extern int  sys_log_fd,
            no_of_afds;
extern char afd_mon_db_file[];


/*++++++++++++++++++++++++++ eval_afd_mon_db() ++++++++++++++++++++++++++*/
void
eval_afd_mon_db(struct mon_list **nml)
{
   int    i;
   size_t new_size;
   char   *afdbase = NULL,
          number[MAX_INT_LENGTH + 1],
          *ptr;

   /* Read the contents of the AFD_MON_CONFIG file into a buffer. */
   if (read_file(afd_mon_db_file, &afdbase) == INCORRECT)
   {
      exit(INCORRECT);
   }

   ptr = afdbase;
   no_of_afds = 0;

   /*
    * Cut off any comments before the AFD names come.
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
      else
      {
         while (*ptr == '\n')
         {
            ptr++;
         }
      }

      /* Check if buffer for afd list structure is large enough. */
      if ((no_of_afds % MEM_STEP_SIZE) == 0)
      {
         char *ptr_start;

         /* Calculate new size for afd list */
         new_size = ((no_of_afds / MEM_STEP_SIZE) + 1) *
                    MEM_STEP_SIZE * sizeof(struct mon_status_area);

         /* Now increase the space */
         if ((*nml = realloc(*nml, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not reallocate memory for AFD monitor list : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /* Initialise the new memory area */
         if (no_of_afds > (MEM_STEP_SIZE - 1))
         {
            ptr_start = (char *)(*nml) + (no_of_afds * sizeof(struct mon_status_area));
         }
         else
         {
            ptr_start = (char *)(*nml);
         }
         new_size = MEM_STEP_SIZE * sizeof(struct mon_status_area);
         /* offset = (int)(*nml + (no_of_afds / MEM_STEP_SIZE)); */
         (void)memset(ptr_start, 0, new_size);
      }

      /* Store AFD alias */
      i = 0;
      (*nml)[no_of_afds].convert_username[0][0] = '\0';
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i <= MAX_AFDNAME_LENGTH))
      {
         (*nml)[no_of_afds].afd_alias[i] = *ptr;
         ptr++; i++;
      }
      if ((i == (MAX_AFDNAME_LENGTH + 1)) && ((*ptr != ' ') || (*ptr != '\t')))
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for AFD alias name %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   (*nml)[no_of_afds].afd_alias, MAX_AFDNAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      (*nml)[no_of_afds].afd_alias[i] = '\0';
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (void)strcpy((*nml)[no_of_afds].hostname, (*nml)[no_of_afds].afd_alias);
         (*nml)[no_of_afds].port = atoi(DEFAULT_AFD_PORT_NO);
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real Hostname */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         (*nml)[no_of_afds].hostname[i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for real hostname for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   (*nml)[no_of_afds].afd_alias, MAX_REAL_HOSTNAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      (*nml)[no_of_afds].hostname[i] = '\0';
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].port = atoi(DEFAULT_AFD_PORT_NO);
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store TCP port number */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in TCP port field for AFD %s, using default %s. (%s %d)\n",
                      (int)*ptr, (*nml)[no_of_afds].afd_alias,
                      DEFAULT_AFD_PORT_NO, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         (*nml)[no_of_afds].port = atoi(DEFAULT_AFD_PORT_NO);
      }
      else if (i == MAX_INT_LENGTH)
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value allowed transfers to large (>%d characters) for AFD %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, (*nml)[no_of_afds].afd_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %s.\n",
                        DEFAULT_AFD_PORT_NO);
              (*nml)[no_of_afds].port = atoi(DEFAULT_AFD_PORT_NO);
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*nml)[no_of_afds].port = atoi(number);
           }

      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store poll interval */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit(*ptr))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Non numeric character <%d> in poll interval field for AFD %s, using default %s. (%s %d)\n",
                      (int)*ptr, (*nml)[no_of_afds].afd_alias,
                      DEFAULT_POLL_INTERVAL, __FILE__, __LINE__);

            /* Ignore this entry */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
      }
      else if (i == MAX_INT_LENGTH)
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Numeric value for poll interval to large (>%d characters) for AFD %s to store as integer. (%s %d)\n",
                        MAX_INT_LENGTH, (*nml)[no_of_afds].afd_alias,
                        __FILE__, __LINE__);
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Setting it to the default value %s.\n",
                        DEFAULT_POLL_INTERVAL);
              (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*nml)[no_of_afds].poll_interval = atoi(number);
           }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Convert Username */
      i = 0;
      while ((*ptr != '\0') && !((*ptr == '-') && (*(ptr + 1) == '>')) &&
             (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (i < MAX_USER_NAME_LENGTH))
      {
         (*nml)[no_of_afds].convert_username[0][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_USER_NAME_LENGTH)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Maximum length for username for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                   (*nml)[no_of_afds].afd_alias, MAX_USER_NAME_LENGTH,
                   __FILE__, __LINE__);
         while ((*ptr != '\0') && !((*ptr == '-') && (*(ptr + 1) == '>')) &&
                (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n'))
         {
            ptr++;
         }
      }
      (*nml)[no_of_afds].convert_username[0][i] = '\0';
      if ((*ptr == '-') && (*(ptr + 1) == '>'))
      {
         i = 0;
         ptr += 2;
         while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                (*ptr != '\0') && (i < MAX_USER_NAME_LENGTH))
         {
            (*nml)[no_of_afds].convert_username[1][i] = *ptr;
            ptr++; i++;
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Maximum length for username for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters. (%s %d)\n",
                      (*nml)[no_of_afds].afd_alias, MAX_USER_NAME_LENGTH,
                      __FILE__, __LINE__);
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
         (*nml)[no_of_afds].convert_username[1][i] = '\0';
      }
      else
      {
         (*nml)[no_of_afds].convert_username[0][0] = '\0';
         (*nml)[no_of_afds].convert_username[1][0] = '\0';
      }

      /* Ignore the rest of the line. We have everything we need. */
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         ptr++;
      }
      while (*ptr == '\n')
      {
         ptr++;
      }
      no_of_afds++;
   } while (*ptr != '\0');

   if (afdbase != NULL)
   {
      free(afdbase);
   }

   return;
}
