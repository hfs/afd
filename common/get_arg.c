/*
 *  get_arg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_arg - gets an argument from the command line
 **
 ** SYNOPSIS
 **   int get_arg(int *argc, char *argv[], char *arg,
 **               char *buffer, int buf_length)
 **
 ** DESCRIPTION
 **   The function get_arg() gets the argument 'arg' from the command
 **   line and if 'buffer' is not NULL will store the next argument in
 **   'buffer'. This next argument may NOT start with a '-', otherwise
 **   INCORRECT will be returned. The arguments will be removed from
 **   the argument list.
 **
 ** RETURN VALUES
 **   SUCCESS when the argument was found. Otherwise INCORRECT is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.01.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), NULL                        */
#include <string.h>            /* strcmp(), strcpy()                     */


/*############################# get_arg() ###############################*/
int
get_arg(int *argc, char *argv[], char *arg, char *buffer, int buf_length)
{
   register int i;

   for (i = 1; i < *argc; i++)
   {
      if (strcmp(argv[i], arg) == 0)
      {
         if (buffer != NULL)
         {
            if (((i + 1) < *argc) && (argv[i + 1][0] != '-'))
            {
               /* Check if the buffer is long enough! */
               if (buf_length < strlen(argv[i + 1]))
               {
                  (void)fprintf(stderr,
                                "Buffer for storing value for argument %s to short! (%s %d)\n",
                                argv[i], __FILE__, __LINE__);
                  return(INCORRECT);
               }
               (void)strcpy(buffer, argv[i + 1]);
               if ((i + 2) < *argc)
               {
                  register int j;

                  for (j = i; j < *argc; j++)
                  {
                     argv[j] = argv[j + 2];
                  }
                  argv[j] = NULL;
               }
               else
               {
                  argv[i] = NULL;
               }
               *argc -= 2;

               return(SUCCESS);
            }
            else
            {
               /* Either there are not enough arguments or the next */
               /* argument starts with a dash '-'.                  */
               return(INCORRECT);
            }
         }
         if ((i + 1) < *argc)
         {
            register int j;

            for (j = i; j < *argc; j++)
            {
               argv[j] = argv[j + 1];
            }
            argv[j] = NULL;
         }
         else
         {
            argv[i] = NULL;
         }
         *argc -= 1;

         return(SUCCESS);
      }
   }

   /* Argument NOT found */
   return(INCORRECT);
}
