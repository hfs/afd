/*
 *  eval_dir_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 1999 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_dir_config - reads the DIR_CONFIG file and evaluates it
 **
 ** SYNOPSIS
 **   int eval_dir_config(size_t db_size)
 **
 ** DESCRIPTION
 **   The function eval_dir_config() reads the DIR_CONFIG file of the
 **   AMG and writes the contents in a compact form to a shared memory
 **   region. This region is then used by other process (eg. dir_check)
 **   to create jobs (messages) for the FD.
 **
 **   An DIR_CONFIG entry consists of the following sections:
 **   directory, files, destination, recipient and options.
 **   The directory entry always marks the beginning of a new section
 **   and contains one user directory. In the files group we specify
 **   which files are to be send from this directory. It can have
 **   more then one entry and wild cards are allowed too. The
 **   destination group contains the two sub groups recipient and
 **   options. Each file group may contain more then one destination
 **   group. In the recipient group is the address of the recipient
 **   for these files with these specific options. The recipient
 **   group may contain more then one recipient. In the option
 **   group all options for this recipient are specified. Here follows
 **   an example entry:
 **
 **   [directory]
 **   /home/afd/data
 **
 **          [files]
 **          a*
 **          file?
 **
 **                  [destination]
 **
 **                          [recipient]
 **                          ftp://user:passwd@hostname:22/dir1/dir2
 **                          mail://user@hostname3
 **
 **                          [options]
 **                          archive 3
 **                          lock DOT
 **                          priority 0
 **
 **   This entry would instruct the AFD to send all files starting with
 **   an a and files starting with file and one more character from
 **   the directory /home/afd/data to two remote hosts (hostname1 and
 **   hostname2) with a priority of 0. All files that have been
 **   transmitted will be archived for three days.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to evaluate the database file
 **   of the AMG. On success the number of user directories is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.05.1995 H.Kiehl Created
 **   15.12.1996 H.Kiehl Changed to URL format
 **   05.06.1997 H.Kiehl Memory for struct dir_group is now allocated with
 **                      malloc().
 **   21.08.1997 H.Kiehl Added support for real hostname, so that the
 **                      message will only contain an alias name.
 **   25.08.1997 H.Kiehl When trying to lock the FSA_ID_FILE, we will
 **                      block until the lock is released.
 **   08.11.1997 H.Kiehl Try create source directory recursively if it does
 **                      not exist.
 **   19.11.1997 H.Kiehl Return of the HOST_CONFIG file!
 **   21.09.1999 H.Kiehl Make it possible to specify ~username as part
 **                      of directory.
 **
 */
DESCR__E_M3

#include <stdio.h>                  /* fprintf()                         */
#include <string.h>                 /* strlen(), strcmp(), strncmp(),    */
                                    /* strcpy(), memmove(), strerror()   */
#include <stdlib.h>                 /* calloc(), realloc(), free(),      */
                                    /* atoi(), malloc()                  */
#include <ctype.h>                  /* isdigit()                         */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>                /* shmat(), shmctl(), shmdt()        */
#include <sys/stat.h>
#include <unistd.h>                 /* access(), getuid()                */
#include <pwd.h>                    /* getpwnam()                        */
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables */
#ifdef _DEBUG
extern FILE             *p_debug_file;
#endif
extern char             dir_config_file[];
extern int              shm_id,     /* The shared memory ID's for the   */
                                    /* sorted data and pointers.        */
                        data_length,/* The size of data for one job.    */
                        sys_log_fd,
                        no_of_hosts;/* The number of remote hosts to    */
                                    /* which files have to be           */
                                    /* transfered.                      */
extern struct host_list *hl;        /* Structure that holds all the     */
                                    /* hosts.                           */


/* Global local variables */
char                    *database = NULL;
int                     job_no;   /* By job number we here mean for */
                                  /* each destination specified!    */
struct p_array          *pp_it;
static char             *p_it = NULL;  /* Start of instant table.        */

/* Local function prototypes */
static void             insert_dir(struct dir_group *),
                        insert_hostname(struct dir_group *),
                        copy_to_shm(void),
                        copy_job(int, int, struct dir_group *),
                        count_new_lines(char *, char *, int *),
                        store_full_path(char *, char *);

/* The following macro checks for spaces and comment */
#define CHECK_SPACE()                                         \
        {                                                     \
           if ((*ptr == ' ') || (*ptr == '\t'))               \
           {                                                  \
              tmp_ptr = ptr;                                  \
              while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t')) \
              {                                               \
                 tmp_ptr++;                                   \
              }                                               \
                                                              \
              switch (*tmp_ptr)                               \
              {                                               \
                 case '#' :  /* Found comment */              \
                             while ((*tmp_ptr != '\n') &&     \
                                    (*tmp_ptr != '\0'))       \
                             {                                \
                                tmp_ptr++;                    \
                             }                                \
                             ptr = tmp_ptr;                   \
                             line_counter++;                  \
                             continue;                        \
                 case '\0':  /* Found end for this entry */   \
                             ptr = tmp_ptr;                   \
                             continue;                        \
                 case '\n':  /* End of line reached */        \
                             ptr = tmp_ptr;                   \
                             continue;                        \
                 default  :  /* option goes on */             \
                             ptr = tmp_ptr;                   \
                             break;                           \
              }                                               \
           }                                                  \
           else if (*ptr == '#')                              \
                {                                             \
                   tmp_ptr = ptr;                             \
                    while ((*tmp_ptr != '\n') &&              \
                           (*tmp_ptr != '\0'))                \
                    {                                         \
                       tmp_ptr++;                             \
                    }                                         \
                    ptr = tmp_ptr;                            \
                    line_counter++;                           \
                    continue;                                 \
                }                                             \
        }


/*########################## eval_dir_config() ##########################*/
int
eval_dir_config(size_t db_size)
{
   int      i,
            j,
#ifdef _DEBUG
            k,
            m,
#endif
            line_counter,        /* When an error is encountered, */
                                 /* this variable holds the line  */
                                 /* where the error occurred.     */
            dc,                  /* Counter for no of directories */
                                 /* found.                        */
            t_dgc = 0,           /* Total number of destination   */
                                 /* groups found.                 */
            t_rc = 0,            /* Total number of recipients    */
                                 /* found.                        */
            unique_file_counter, /* Counter for creating a unique */
                                 /* file name.                    */
            unique_dest_counter; /* Counter for creating a unique */
                                 /* destination name.             */
   char     *ptr,                /* Main pointer that walks       */
                                 /* through buffer.               */
            *tmp_ptr = NULL,     /* */
            *search_ptr = NULL,  /* Pointer used in conjunction   */
                                 /* with the posi() function.     */
            tmp_dir_char = 0,    /* Buffer that holds torn out    */
                                 /* character.                    */
            *end_dir_ptr = NULL, /* Pointer that marks the end of */
                                 /* a directory entry.            */
            other_dir_flag,      /* If set, there is another      */
                                 /* directory entry.              */
            tmp_file_char = 1,   /* Buffer that holds torn out    */
                                 /* character.                    */
            *end_file_ptr = NULL,/* Pointer that marks the end of */
                                 /* a file entry.                 */
            other_file_flag,     /* If set, there is another file */
                                 /* entry.                        */
            tmp_dest_char = 1,   /* Buffer that holds torn out    */
                                 /* character.                    */
            *end_dest_ptr = NULL,/* Pointer that marks the end of */
                                 /* a destination entry.          */
            other_dest_flag,     /* If set, there is another      */
                                 /* destination entry.            */
            prev_user_name[MAX_USER_NAME_LENGTH],
            prev_user_dir[MAX_PATH_LENGTH];
   struct dir_group  *dir;

   /* Allocate memory for the directory structure. */
   if ((dir = (struct dir_group *)malloc(sizeof(struct dir_group))) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   dir->file = NULL;
   prev_user_name[0] = '\0';

   /* read database file and store it into memory */
   database = NULL;
   if (read_file(dir_config_file, &database) == INCORRECT)
   {
      exit(INCORRECT);
   }

   /* Create temporal storage area for job */
   if ((p_it = calloc(db_size, sizeof(char))) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not allocate memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      free((void *)dir);
      return(INCORRECT);
   }

   /* Initialise variables */
   pp_it = NULL;
   job_no = data_length = 0;
   line_counter = 0;
   unique_file_counter = 0;
   unique_dest_counter = 0;
   dc = 0;
   ptr = database;

   /* Read each directory entry one by one until we reach the end */
   while ((search_ptr = posi(ptr, DIR_IDENTIFIER)) != NULL)
   {
      count_new_lines(ptr, search_ptr, &line_counter);

      if (*(search_ptr - strlen(DIR_IDENTIFIER) - 2) != '\n')
      {
         ptr = search_ptr;
         continue;
      }

      if (*(search_ptr - 1) != '\n')
      {
         /* Ignore any data directly behind the identifier */
         while ((*search_ptr != '\n') && (*search_ptr != '\0'))
         {
            search_ptr++;
         }
         search_ptr++;
         line_counter++;
      }
      ptr = search_ptr;

#ifdef _LINE_COUNTER_TEST
      (void)rec(sys_log_fd, INFO_SIGN,
                "Found DIR_IDENTIFIER at line %d\n", line_counter);
#endif

      /* Initialise directory structure */
      (void)memset((void *)dir, 0, sizeof(struct dir_group));

      /*
       ********************* Read directory ********************
       */
      /* Check if there is a directory */
      if (*ptr == '\n')
      {
         /* Generate warning, that last dir entry does not */
         /* have a destination.                            */
         (void)rec(sys_log_fd, WARN_SIGN,
                   "In line %d, directory entry does not have a directory. (%s %d)\n",
                   line_counter, __FILE__, __LINE__);
         line_counter++;
         ptr++;

         continue;
      }

      /* store directory name */
      i = 0;
      dir->option[0] = '\0';
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         CHECK_SPACE();
         if ((i > 0) &&
             ((*(ptr - 1) == '\t') || (*(ptr - 1) == ' ')))
         {
            int ii = 0;

            while ((*ptr != '\n') && (*ptr != '\0') &&
                   (ii < MAX_DIR_OPTION_LENGTH))
            {
               CHECK_SPACE();
               if ((ii > 0) &&
                   ((*(ptr - 1) == '\t') || (*(ptr - 1) == ' ')))
               {
                  dir->option[ii] = ' ';
                  ii++;
               }
               dir->option[ii] = *ptr;
               ii++; ptr++;
            }
            if (ii > 0)
            {
               dir->option[ii] = '\0';
            }
         }
         else
         {
            dir->location[i] = *ptr;
            i++; ptr++;
         }
      }

      line_counter++;

      /* Lets resolve any tilde signs ~. */
      if (dir->location[0] == '~')
      {
         char tmp_char,
              tmp_location[MAX_PATH_LENGTH],
              *tmp_ptr = dir->location;

         while ((*tmp_ptr != '/') && (*tmp_ptr != '\n') &&
                (*tmp_ptr != '\0') && (*tmp_ptr != ' ') &&
                (*tmp_ptr != '\t'))
         {
            tmp_ptr++;
         }
         tmp_char = *tmp_ptr;
         *tmp_ptr = '\0';
         if ((prev_user_name[0] == '\0') ||
             (strcmp(dir->location, prev_user_name) != 0))
         {
            struct passwd *pwd;

            if (*(tmp_ptr - 1) == '~')
            {
               if ((pwd = getpwuid(getuid())) == NULL)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Cannot find working directory for user with the user ID %d in /etc/passwd (ignoring directory) : %s (%s %d)\n",
                            getuid(), strerror(errno),
                            __FILE__, __LINE__);
                  *tmp_ptr = tmp_char;
                  continue;
               }
            }
            else
            {
               if ((pwd = getpwnam(&dir->location[1])) == NULL)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Cannot find users %s working directory in /etc/passwd (ignoring directory) : %s (%s %d)\n",
                            &dir->location[1], strerror(errno),
                            __FILE__, __LINE__);
                  *tmp_ptr = tmp_char;
                  continue;
               }
            }
            (void)strcpy(prev_user_name, dir->location);
            (void)strcpy(prev_user_dir, pwd->pw_dir);
         }
         *tmp_ptr = tmp_char;
         (void)strcpy(tmp_location, prev_user_dir);
         if (*tmp_ptr == '/')
         {
            (void)strcat(tmp_location, tmp_ptr);
         }
         (void)strcpy(dir->location, tmp_location);
      }

      /* Now lets check if this directory does exist and if we */
      /* do have enough permissions to work in this directory. */
      if (access(dir->location, R_OK | W_OK | X_OK) < 0)
      {
         if (errno == ENOENT)
         {
            int  ii = 0,
                 new_size,
                 dir_length,
                 dir_counter = 0,
                 failed_to_create_dir = NO,
                 error_condition = 0;
            char **dir_ptr = NULL;

            do
            {
               if ((ii % 10) == 0)
               {
                  new_size = ((ii / 10) + 1) * 10 * sizeof(char *);
                  if ((dir_ptr = realloc(dir_ptr, new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "Could not realloc() memory : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               dir_length = strlen(dir->location);
               dir_ptr[ii] = dir->location + dir_length;
               while (dir_length > 0)
               {
                  dir_ptr[ii]--; dir_length--;
                  if (*dir_ptr[ii] == '/')
                  {
                     *dir_ptr[ii] = '\0';
                     dir_counter++; ii++;
                     break;
                  }
               }
               if (dir_length <= 0)
               {
                  break;
               }
            } while (((error_condition = access(dir->location, R_OK | W_OK | X_OK)) < 0) &&
                     (errno == ENOENT));

            if ((error_condition < 0) && (errno != ENOENT))
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Cannot access directory %s at line %d (Ignoring this entry) : %s (%s %d)\n",
                         dir->location, line_counter, strerror(errno),
                         __FILE__, __LINE__);
               if (dir_ptr != NULL)
               {
                  free(dir_ptr);
               }
               continue;
            }

            /*
             * Try to create all missing directory names recursively.
             */
            for (ii = (dir_counter - 1); ii >= 0; ii--)
            {
               *dir_ptr[ii] = '/';
               if (mkdir(dir->location, DIR_MODE) == -1)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to create directory %s at line %d (Ignoring this entry) : %s (%s %d)\n",
                            dir->location, line_counter, strerror(errno),
                            __FILE__, __LINE__);
                  failed_to_create_dir = YES;
                  break;
               }
            }
            if (dir_ptr != NULL)
            {
               free(dir_ptr);
            }
            if (failed_to_create_dir == NO)
            {
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Created directory %s at line %d (%s %d)\n",
                         dir->location, line_counter, __FILE__, __LINE__);
            }
            else
            {
               continue;
            }
         }
         else
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Cannot access directory %s at line %d (Ignoring this entry) : %s (%s %d)\n",
                      dir->location, line_counter, strerror(errno),
                      __FILE__, __LINE__);
            continue;
         }
      }

      /* Before we go on, we have to search for the beginning of */
      /* the next directory entry so we can mark the end for     */
      /* this directory entry.                                   */
      if ((end_dir_ptr = posi(ptr, DIR_IDENTIFIER)) != NULL)
      {
         /* First save char we encounter here */
         tmp_dir_char = *end_dir_ptr;

         /* Now mark end of this directory entry */
         *end_dir_ptr = '\0';

         /* Set flag that we found another entry */
         other_dir_flag = YES;
      }
      else
      {
         other_dir_flag = NO;
      }
         
      /*
       ********************** Read filenames *******************
       */
      /* position ptr after FILE_IDENTIFIER */
      dir->fgc = 0;
      while ((search_ptr = posi(ptr, FILE_IDENTIFIER)) != NULL)
      {
         if (dir->fgc != 0)
         {
            line_counter++;
         }
         count_new_lines(ptr, search_ptr, &line_counter);

         tmp_ptr = search_ptr - strlen(FILE_IDENTIFIER) - 2;
         while ((*tmp_ptr != '\n') && (*tmp_ptr != '#'))
         {
            tmp_ptr--;
         }
         if (*tmp_ptr == '#')
         {
            ptr = search_ptr;
            continue;
         }

#ifdef _LINE_COUNTER_TEST
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Found FILE_IDENTIFIER at line %d\n", line_counter);
#endif

         if ((dir->fgc % FG_BUFFER_STEP_SIZE) == 0)
         {
            char   *ptr_start;
            size_t new_size;

            /* Calculate new size of file group buffer */
            new_size = ((dir->fgc / FG_BUFFER_STEP_SIZE) + 1) * FG_BUFFER_STEP_SIZE * sizeof(struct file_group);

            if ((dir->file = (struct file_group *)realloc(dir->file, new_size)) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Could not realloc() memory : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }

            if (dir->fgc > (FG_BUFFER_STEP_SIZE - 1))
            {
               ptr_start = (char *)(dir->file) + (dir->fgc * sizeof(struct file_group));
            }
            else
            {
               ptr_start = (char *)dir->file;
            }
            (void)memset(ptr_start, 0, (FG_BUFFER_STEP_SIZE * sizeof(struct file_group)));
         }

         ptr = --search_ptr;

         /* store file-group name */
         if (*ptr != '\n') /* Is there a file group name ? */
         {
            /* Store file group name */
            i = 0;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               CHECK_SPACE();
               dir->file[dir->fgc].file_group_name[i] = *ptr;
               i++; ptr++;
            }

            /* Did we already reach end of current file group? */
            if (*ptr == '\0')
            {
               /* Generate warning that this file entry is faulty */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "In line %d, directory %s does not have a destination entry. (%s %d)\n",
                         line_counter, dir->location, __FILE__, __LINE__);

               /* To read the next file entry, put back the char   */
               /* that was torn out to mark end of this file       */
               /* entry.                                           */
               if (tmp_file_char != 1)
               {
                  *end_file_ptr = tmp_file_char;
               }

               /* No need to carry on with this file entry */
               continue;
            }
            else
            {
               line_counter++;
            }

            /* Make sure that a file group name was defined */
            if (dir->file[dir->fgc].file_group_name[0] == '\0')
            {
               /* Create a unique file group name */
               (void)sprintf(dir->file[dir->fgc].file_group_name,
                             "FILE_%d", unique_file_counter);
               unique_file_counter++;
            }
         }
         else /* No file group name defined */
         {
            /* Create a unique file group name */
            (void)sprintf(dir->file[dir->fgc].file_group_name,
                          "FILE_%d", unique_file_counter);
            unique_file_counter++;
         }

         /* Before we go on, we have to search for the beginning of */
         /* the next file group entry so we can mark the end for   */
         /* this file entry.                                       */
         if ((end_file_ptr = posi(ptr, FILE_IDENTIFIER)) != NULL)
         {
            /* First save char we encounter here */
            tmp_file_char = *end_file_ptr;
   
            /* Now mark end of this file group entry */
            *end_file_ptr = '\0';

            /* Set flag that we found another entry */
            other_file_flag = YES;
         }
         else
         {
            other_file_flag = NO;
         }

         /* store file names */
         dir->file[dir->fgc].fc = 0;
         ptr++;
         if (*ptr == '\n') /* Are any files specified */
         {
            /* We assume that all files in this */
            /* directory should be send         */
            dir->file[dir->fgc].files[0][0] = '*';
            dir->file[dir->fgc].fc++;

            line_counter++;
         }
         else /* Yes, files are specified */
         {
            /* Read each filename line by line,  */
            /* until we encounter an empty line. */
            do
            {
#ifdef _WITH_DYNAMIC_NO_OF_FILTERS
               if ((dir->file[dir->fgc].fc % 5) == 0)
               {
                  if (dir->file[dir->fgc].fc == 0)
                  {
                     RT_ARRAY(dir->file[dir->fgc].files, 5,
                              MAX_FILENAME_LENGTH, char);
                  }
                  else
                  {
                     int new_fc = ((dir->file[dir->fgc].fc / 5) + 1) * 5;

                     REALLOC_RT_ARRAY(dir->file[dir->fgc].files,
                                      new_fc, MAX_FILENAME_LENGTH, char);
                  }
               }
#else
               if (dir->file[dir->fgc].fc >= MAX_NO_FILES)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Maximum number of files (%d) at line %d reached. Ignoring following file filter entries. (%s %d)\n",
                            MAX_NO_FILES, line_counter,
                            __FILE__, __LINE__);
                  line_counter--;
                  break;
               }
#endif

               /* Store file name */
               i = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  CHECK_SPACE();

                  /* Store file name */
                  dir->file[dir->fgc].files[dir->file[dir->fgc].fc][i] = *ptr;
                  ptr++; i++;
               }

               if (i != 0)
               {
                  line_counter++;
                  dir->file[dir->fgc].fc++;
               }
               ptr++;

               /* Check for a dummy empty line. */
               if (*ptr != '\n')
               {
                  search_ptr = ptr;
                  while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                  {
                     search_ptr++;
                  }
                  ptr = search_ptr;
               }
            } while (*ptr != '\n');
         }

         /*
          ******************** Read destinations ******************
          */
         /* position ptr after DESTINATION_IDENTIFIER */
         ptr++;
         dir->file[dir->fgc].dgc = 0;
         while ((search_ptr = posi(ptr, DESTINATION_IDENTIFIER)) != NULL)
         {
            if (dir->file[dir->fgc].dgc != 0)
            {
               ptr++;
            }
            count_new_lines(ptr, search_ptr, &line_counter);

            tmp_ptr = search_ptr - strlen(DESTINATION_IDENTIFIER) - 2;
            while ((*tmp_ptr != '\n') && (*tmp_ptr != '#'))
            {
               tmp_ptr--;
            }
            if (*tmp_ptr == '#')
            {
               ptr = search_ptr;
               continue;
            }
            ptr = --search_ptr;

#ifdef _LINE_COUNTER_TEST
            (void)rec(sys_log_fd, INFO_SIGN,
                      "Found DESTINATION_IDENTIFIER at line %d\n",
                      line_counter);
#endif

            /* store destination group name */
            if (*ptr != '\n') /* Is there a destination group name ? */
            {
               /* Store group name of destination */
               i = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  CHECK_SPACE();
                  dir->file[dir->fgc].\
                          dest[dir->file[dir->fgc].dgc].\
                          dest_group_name[i] = *ptr;
                  i++; ptr++;
               }

               /* Check if we encountered end of destination entry */
               if (*ptr == '\0')
               {
                  /* Generate warning message, that no destination */
                  /* has been defined.                             */
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Directory %s at line %d does not have a destination entry for file group no. %d. (%s %d)\n",
                            dir->location, line_counter, dir->fgc,
                            __FILE__, __LINE__);

                  /* To read the next destination entry, put back the */
                  /* char that was torn out to mark end of this       */
                  /* destination entry.                               */
                  if (tmp_dest_char != 1)
                  {
                     *end_dest_ptr = tmp_dest_char;
                  }

                  continue; /* Go to next destination entry */
               }
            }
            else /* No destination group name defined */
            {
               /* Create a unique destination group name */
               (void)sprintf(dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].\
                             dest_group_name, "DEST_%d", unique_dest_counter);
               unique_dest_counter++;
            }
            ptr++;

            /* Before we go on, we have to search for the beginning of */
            /* the next destination entry so we can mark the end for  */
            /* this destination entry.                                */
            if ((end_dest_ptr = posi(ptr, DESTINATION_IDENTIFIER)) != NULL)
            {
               /* First save char we encounter here */
               tmp_dest_char = *end_dest_ptr;
   
               /* Now mark end of this destination entry */
               *end_dest_ptr = '\0';

               /* Set flag that we found another entry */
               other_dest_flag = YES;
            }
            else
            {
               other_dest_flag = NO;
            }

            /*++++++++++++++++++++++ Read recipient +++++++++++++++++++++*/
            /* position ptr after RECIPIENT_IDENTIFIER */
            if ((search_ptr = posi(ptr, RECIPIENT_IDENTIFIER)) != NULL)
            {
               count_new_lines(ptr, search_ptr, &line_counter);

               if (*(search_ptr - 1) != '\n')
               {
                  /* Ignore any data directly behind the identifier */
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
                  line_counter++;
               }
               while (*search_ptr == '#')
               {
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
                  line_counter++;
               }
               ptr = search_ptr;

#ifdef _LINE_COUNTER_TEST
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Found RECIPIENT_IDENTIFIER at line %d\n",
                         line_counter);
#endif

               /* read the address for each recipient  */
               /* line by line, until we encounter an  */
               /* empty line.                          */
               dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc = 0;

               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  /* Check if this is a comment line. */
                  if (*ptr == '#')
                  {
                     tmp_ptr = ptr;
                      while ((*tmp_ptr != '\n') &&
                             (*tmp_ptr != '\0'))
                      {
                         tmp_ptr++;
                      }
                      ptr = tmp_ptr + 1;
                      line_counter++;
                      continue;
                  }

                  if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc >= MAX_NO_RECIPIENT)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Number of recipients exceeded at line %d! Only %d recipients per destination entry. Ignoring entries! (%s %d)\n",
                               line_counter + 1, MAX_NO_RECIPIENT,
                               __FILE__, __LINE__);
                     break;
                  }

                  /* Store recipient */
                  i = 0;
                  tmp_ptr = search_ptr = ptr;
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     if ((*ptr == ' ') || (*ptr == '\t'))
                     {
                        tmp_ptr = ptr;
                        while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t'))
                        {
                           tmp_ptr++;
                        }

                        switch (*tmp_ptr)
                        {
                           case '#' :  /* Found comment */
                                       while ((*tmp_ptr != '\n') &&
                                              (*tmp_ptr != '\0'))
                                       {
                                          tmp_ptr++;
                                       }
                                       ptr = tmp_ptr;
                                       line_counter++;
                                       continue;
                           case '\0':  /* Found end for this entry */
                                       ptr = tmp_ptr;
                                       continue;
                           case '\n':  /* End of line reached */
                                       ptr = tmp_ptr;
                                       continue;
                           default  :  /* option goes on */
                                       ptr = tmp_ptr;
                                       break;
                        }
                     }
                     dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].\
                             recipient[dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].rc][i] = *ptr;
                     ptr++; i++;
                  }
                  ptr++;
                  if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc != 0)
                  {
                     line_counter++;
                  }

                  /* Make sure that we did read a line. */
                  if (i != 0)
                  {
                     /* Check if we can extract the hostname. */
                     if (get_hostname(dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].\
                                      recipient[dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].rc]) == NULL)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to locate hostname in recipient string %s. Ignoring the recipient at line %d. (%s %d)\n",
                                  dir->file[dir->fgc].\
                                  dest[dir->file[dir->fgc].dgc].\
                                  recipient[dir->file[dir->fgc].\
                                  dest[dir->file[dir->fgc].dgc].rc],
                                  line_counter, __FILE__, __LINE__);
                     }
                     else
                     {
                        /* Check if sheme/protocol is correct */
                        while ((*search_ptr == '\t') || (*search_ptr == ' '))
                        {
                           search_ptr++;
                        }
                        j = 0;
                        while ((j < i) && (*search_ptr != ':'))
                        {
                           search_ptr++; j++;
                        }
                        if ((*search_ptr != ':') || ((j < i) &&
                            ((*(search_ptr + 1) != '/') || (*(search_ptr + 2) != '/'))))
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Failed to determine sheme/protocol. Ignoring this recipient at line %d. (%s %d)\n",
                                     line_counter, __FILE__, __LINE__);
                        }
                        else
                        {
                           *search_ptr = '\0';
                           if (strcmp(search_ptr - j, FTP_SHEME) != 0)
                           {
                              if (strcmp(search_ptr - j, LOC_SHEME) != 0)
                              {
#ifdef _WITH_WMO_SUPPORT
                                 if (strcmp(search_ptr - j, WMO_SHEME) != 0)
                                 {
#endif
#ifdef _WITH_MAP_SUPPORT
                                    if (strcmp(search_ptr - j, MAP_SHEME) != 0)
                                    {
#endif
                                       if (strcmp(search_ptr - j, SMTP_SHEME) != 0)
                                       {
                                          (void)rec(sys_log_fd, WARN_SIGN,
                                                    "Unknown sheme <%s>. Ignoring recipient at line %d. (%s %d)\n",
                                                    search_ptr - j, line_counter,
                                                    __FILE__, __LINE__);
                                       }
                                       else
                                       {
                                          dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                          t_rc++;
                                       }
#ifdef _WITH_MAP_SUPPORT
                                    }
                                    else
                                    {
                                       dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                       t_rc++;
                                    }
#endif
#ifdef _WITH_WMO_SUPPORT
                                 }
                                 else
                                 {
                                    dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                    t_rc++;
                                 }
#endif
                              }
                              else
                              {
                                 char *p_user_start,
                                      *tmp_search_ptr;

                                 /*
                                  * Enter the complete path. So if the user
                                  * only specifies the path relative to the
                                  * users directory, get that users home
                                  * directory. That can be done in two ways:
                                  * either by using the users home directory
                                  * from the user specified in the user
                                  * field or by using ~username in the
                                  * directory part. The tilde username
                                  * version will be the one taken when
                                  * both are specified.
                                  */
                                 tmp_search_ptr = dir->file[dir->fgc].\
                                                            dest[dir->file[dir->fgc].dgc].\
                                                            recipient[dir->file[dir->fgc].\
                                                            dest[dir->file[dir->fgc].dgc].rc] +
                                                            strlen(LOC_SHEME) +
                                                            3;
                                 p_user_start = tmp_search_ptr;
                                 while ((*tmp_search_ptr != ':') &&
                                        (*tmp_search_ptr != '@') &&
                                        (*tmp_search_ptr != '#') &&
                                        (*tmp_search_ptr != '\n') &&
                                        (*tmp_search_ptr != '\0'))
                                 {
                                    tmp_search_ptr++;
                                 }
                                 if ((*tmp_search_ptr == ':') ||
                                     (*tmp_search_ptr == '@'))
                                 {
                                     char *p_user_end = tmp_search_ptr;

                                     while ((*tmp_search_ptr != '@') &&
                                            (*tmp_search_ptr != '#') &&
                                            (*tmp_search_ptr != '\n') &&
                                            (*tmp_search_ptr != '\0'))
                                     {
                                        tmp_search_ptr++;
                                     }
                                     if (*tmp_search_ptr == '@')
                                     {
                                        while ((*tmp_search_ptr != '/') &&
                                               (*tmp_search_ptr != ' ') &&
                                               (*tmp_search_ptr != '#') &&
                                               (*tmp_search_ptr != ';') &&
                                               (*tmp_search_ptr != '\t') &&
                                               (*tmp_search_ptr != '\n') &&
                                               (*tmp_search_ptr != '\0'))
                                        {
                                           tmp_search_ptr++;
                                        }
                                        if ((*tmp_search_ptr == '/') &&
                                            ((*(tmp_search_ptr + 1) == '~') ||
                                             ((*(tmp_search_ptr + 2) == '~') &&
                                              (*(tmp_search_ptr + 1) == '/'))))
                                        {
                                           int  kk = 0;
                                           char *p_start_dir = tmp_search_ptr,
                                                user_name[MAX_USER_NAME_LENGTH];

                                           while ((*tmp_search_ptr == '/') ||
                                                  (*tmp_search_ptr == '~'))
                                           {
                                              tmp_search_ptr++;
                                           }
                                           while ((*tmp_search_ptr != '/') &&
                                                  (*tmp_search_ptr != ';') &&
                                                  (*tmp_search_ptr != '#') &&
                                                  (*tmp_search_ptr != ' ') &&
                                                  (*tmp_search_ptr != '\n') &&
                                                  (*tmp_search_ptr != '\t') &&
                                                  (*tmp_search_ptr != '\0') &&
                                                  (kk < MAX_USER_NAME_LENGTH))
                                           {
                                              user_name[kk] = *tmp_search_ptr;
                                              tmp_search_ptr++; kk++;
                                           }
                                           if (kk > 0)
                                           {
                                              user_name[kk] = '\0';
                                              store_full_path(user_name,
                                                              p_start_dir);
                                           }
                                           else
                                           {
                                              char tmp_char = *p_user_end;

                                              *p_user_end = '\0';
                                              store_full_path(p_user_start,
                                                              p_start_dir);
                                              *p_user_end = tmp_char;
                                           }
                                        }
                                        else if (((*tmp_search_ptr == '/') &&
                                                 (*(tmp_search_ptr + 1) != '/')) ||
                                                 (*tmp_search_ptr == '#') ||
                                                 (*tmp_search_ptr == ' ') ||
                                                 (*tmp_search_ptr == ';') ||
                                                 (*tmp_search_ptr == '\t') ||
                                                 (*tmp_search_ptr == '\n') ||
                                                 (*tmp_search_ptr == '\0'))
                                             {
                                                char tmp_char = *p_user_end;

                                                *p_user_end = '\0';
                                                store_full_path(p_user_start,
                                                                tmp_search_ptr);
                                                *p_user_end = tmp_char;
                                             }
                                     }
                                 }
                                 dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                 t_rc++;
                              }
                           }
                           else
                           {
                              dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                              t_rc++;
                           }
                           *search_ptr = ':';
                        }
                     }
                  } /* if (i != 0) */

                  /* Check for a dummy empty line. */
                  if (*ptr != '\n')
                  {
                     search_ptr = ptr;
                     while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                     {
                        search_ptr++;
                     }
                     ptr = search_ptr;
                  }
               }

               line_counter++;
            }

            /* Make sure that at least one recipient was defined */
            if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc == 0)
            {
               /* Generate warning message */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "No recipient specified for %s at line %d. (%s %d)\n",
                         dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].dest_group_name,
                         line_counter, __FILE__, __LINE__);

               /* To read the next destination entry, put back the */
               /* char that was torn out to mark end of this       */
               /* destination entry.                               */
               if (other_dest_flag == YES)
               {
                  *end_dest_ptr = tmp_dest_char;
               }

               /* Try to read the next destination */
               continue;
            }
   
            /*++++++++++++++++++++++ Read options ++++++++++++++++++++++++*/
            /* position ptr after OPTION_IDENTIFIER */
            if ((search_ptr = posi(ptr, OPTION_IDENTIFIER)) != NULL)
            {
               count_new_lines(ptr, search_ptr, &line_counter);

               if (*(search_ptr - 1) != '\n')
               {
                  /* Ignore any data directly behind the identifier */
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
                  line_counter++;
               }
               ptr = search_ptr;

#ifdef _LINE_COUNTER_TEST
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Found OPTION_IDENTIFIER at line %d\n",
                         line_counter);
#endif

               /* Read each option line by line, until */
               /* we encounter an empty line.          */
               dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  /* Store option */
                  i = 0;
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     CHECK_SPACE();
                     if ((i > 0) &&
                         ((*(ptr - 1) == '\t') || (*(ptr - 1) == ' ')))
                     {
                        dir->file[dir->fgc].\
                                dest[dir->file[dir->fgc].dgc].\
                                options[dir->file[dir->fgc].\
                                dest[dir->file[dir->fgc].dgc].oc][i] = ' ';
                        i++;
                     }
                     dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].\
                             options[dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].oc][i] = *ptr;
                     ptr++; i++;
                  }

                  ptr++;

                  /* Make sure that we did read a line. */
                  if (i != 0)
                  {
                     line_counter++;
                     dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc++;
                  }

                  /* Check for a dummy empty line. */
                  if (*ptr != '\n')
                  {
                     search_ptr = ptr;
                     while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                     {
                        search_ptr++;
                     }
                     ptr = search_ptr;
                  }
               } /* while ((*ptr != '\n') && (*ptr != '\0')) */
            }

            /* To read the next destination entry, put back the */
            /* char that was torn out to mark end of this       */
            /* destination entry.                               */
            if (other_dest_flag == YES)
            {
               *end_dest_ptr = tmp_dest_char;
            }

            dir->file[dir->fgc].dgc++;
            t_dgc++;
         } /* while () searching for DESTINATION_IDENTIFIER */

         /* Check if a destination was defined */
         if (dir->file[dir->fgc].dgc == 0)
         {
            /* Generate error message */
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Directory %s does not have a destination entry for file group no. %d. (%s %d)\n",
                      dir->location, dir->fgc, __FILE__, __LINE__);

            /* Reduce file counter, since this one is faulty */
            dir->fgc--;
         }

         /* To read the next file entry, put back the char   */
         /* that was torn out to mark end of this file       */
         /* entry.                                           */
         if (other_file_flag == YES)
         {
            *end_file_ptr = tmp_file_char;
         }

         dir->fgc++;
         if (*ptr == '\0')
         {
            break;
         }
         else
         {
            ptr++;
         }
      } /* while () searching for FILE_IDENTIFIER */

      /* Check special case when no file identifier is found */
      if (dir->fgc == 0)
      {
         /* We assume that all files in this dir should be send */
         dir->file[dir->fgc].files[0][0] = '*';
      }

      /* To read the next directory entry, put back the char */
      /* that was torn out to mark end of this directory     */
      /* entry.                                              */
      if (other_dir_flag == YES)
      {
         *end_dir_ptr = tmp_dir_char;
      }

      /* Check if a destination was defined for the last directory */
      if (dir->file[0].dest[0].rc == 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "In line %d, no destination defined. (%s %d)\n",
                   line_counter, __FILE__, __LINE__);
      }
      else
      {
         /* increase directory counter */
         dc++;

#ifdef _DEBUG
         (void)fprintf(p_debug_file, "\n\n=================> Contents of directory struct %4d<=================\n", dc);
         (void)fprintf(p_debug_file, "                   =================================\n");

         /* print directory name */
         (void)fprintf(p_debug_file, "%3d: >%s<\n", i + 1, dir->location);

         /* print contents of each file group */
         for (j = 0; j < dir->fgc; j++)
         {
            /* print file group name */
            (void)fprintf(p_debug_file, "    >%s<\n", dir->file[j].file_group_name);

            /* print the name of each file */
            for (k = 0; k < dir->file[j].fc; k++)
            {
               (void)fprintf(p_debug_file, "\t%3d: %s\n", k + 1,
                             dir->file[j].files[k]);
            }

            /* print all destinations */
            for (k = 0; k < dir->file[j].dgc; k++)
            {
               /* first print destination group name */
               (void)fprintf(p_debug_file, "\t\t%3d: >%s<\n", k + 1,
                             dir->file[j].dest[k].dest_group_name);

               /* show all recipient's */
               (void)fprintf(p_debug_file, "\t\t\tRecipients:\n");
               for (m = 0; m < dir->file[j].dest[k].rc; m++)
               {
                  (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                dir->file[j].dest[k].recipient[m]);
               }

               /* show all options */
               (void)fprintf(p_debug_file, "\t\t\tOptions:\n");
               for (m = 0; m < dir->file[j].dest[k].oc; m++)
               {
                  (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                dir->file[j].dest[k].options[m]);
               }
            }
         }
#endif

         /* Insert directory into temporary memory */
         insert_dir(dir);

         /* Insert hostnames into temporary memory */
         insert_hostname(dir);

#ifdef _WITH_DYNAMIC_NO_OF_FILTERS
         for (j = 0; j < dir->fgc; j++)
         {
            FREE_RT_ARRAY(dir->file[j].files);
         }
#endif
         free(dir->file);
         dir->file = NULL;
      }
   }

   /* See if there are any valid directory entries */
   if (dc == 0)
   {
      /* We assume there is no entry in database   */
      /* or we have finished reading the database. */
      free(database);
      free(p_it);
      if (pp_it != NULL)
      {
         free(pp_it);
      }
      free((void *)dir);
      return(NO_VALID_ENTRIES);
   }

   /* Now copy data and pointers in their relevant */
   /* shared memory areas.                         */
   copy_to_shm();

   /* Don't forget to create the FSA (Filetransfer */
   /* Status Area).                                */
   create_fsa();

   /* free memory we allocated */
   free(database);
   free(p_it);
   if (pp_it != NULL)
   {
      free(pp_it);
   }
   free((void *)dir);

   /* Tell user what we have found in DIR_CONFIG */
   if (dc > 1)
   {
      (void)rec(sys_log_fd, INFO_SIGN,
                "Found %d directory entries with %d recipients in %d destinations.\n",
                dc, t_rc, t_dgc);
   }
   else if ((dc == 1) && (t_rc == 1))
        {
           (void)rec(sys_log_fd, INFO_SIGN,
                    "Found %d directory entry with %d recipient in %d destination.\n",
                     dc, t_rc, t_dgc);
        }
        else if ((dc == 1) && (t_rc > 1) && (t_dgc == 1))
             {
                (void)rec(sys_log_fd, INFO_SIGN,
                          "Found %d directory entry with %d recipients in %d destination.\n",
                          dc, t_rc, t_dgc);
             }
             else
             {
                (void)rec(sys_log_fd, INFO_SIGN,
                          "Found %d directory entry with %d recipients in %d destinations.\n",
                          dc, t_rc, t_dgc);
             }

   return(dc);
}


/*+++++++++++++++++++++++++ insert_hostname() ++++++++++++++++++++++++++*/
static void
insert_hostname(struct dir_group *dir)
{
   int           i,
                 j,
                 k,
                 m;
   char          *hostname = NULL,
                 *ptr,
                 host_alias[MAX_HOSTNAME_LENGTH + 1],
                 new;
   unsigned char protocol = FTP_FLAG;

   for (i = 0; i < dir->fgc; i++)
   {
      for (j = 0; j < dir->file[i].dgc; j++)
      {
         for (k = 0; k < dir->file[i].dest[j].rc; k++)
         {
            /* Extract only hostname */
            if ((hostname = get_hostname(dir->file[i].dest[j].recipient[k])) == NULL)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to extract hostname %s. (%s %d)\n",
                         dir->file[i].dest[j].recipient[k],
                         __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (memcmp(dir->file[i].dest[j].recipient[k], FTP_SHEME, FTP_SHEME_LENGTH) != 0)
            {
               if (memcmp(dir->file[i].dest[j].recipient[k], LOC_SHEME, LOC_SHEME_LENGTH) != 0)
               {
#ifdef _WITH_WMO_SUPPORT
                  if (memcmp(dir->file[i].dest[j].recipient[k], WMO_SHEME, WMO_SHEME_LENGTH) != 0)
                  {
#endif
#ifdef _WITH_MAP_SUPPORT
                     if (memcmp(dir->file[i].dest[j].recipient[k], MAP_SHEME, MAP_SHEME_LENGTH) != 0)
                     {
#endif
                        if (memcmp(dir->file[i].dest[j].recipient[k], SMTP_SHEME, SMTP_SHEME_LENGTH) == 0)
                        {
                           protocol = SMTP_FLAG;
                        }
#ifdef _WITH_MAP_SUPPORT
                     }
                     else
                     {
                        protocol = MAP_FLAG;
                     }
#endif
#ifdef _WITH_WMO_SUPPORT
                  }
                  else
                  {
                     protocol = WMO_FLAG;
                  }
#endif
               }
               else
               {
                  protocol = LOC_FLAG;
               }
            }
            else
            {
               protocol = FTP_FLAG;
            }

            t_hostname(hostname, host_alias);
            ptr = host_alias;
            while ((*ptr != STATIC_TOGGLE_OPEN) &&
                   (*ptr != AUTO_TOGGLE_OPEN) && (*ptr != '\0'))
            {
               ptr++;
            }
            if ((*ptr == STATIC_TOGGLE_OPEN) || (*ptr == AUTO_TOGGLE_OPEN))
            {
               *ptr = '\0';
            }

            /* Check if host already exists */
            new = YES;
            for (m = 0; m < no_of_hosts; m++)
            {
               if (strcmp(hl[m].host_alias, host_alias) == 0)
               {
                  new = NO;

                  if (hl[m].fullname[0] == '\0')
                  {
                     (void)strcpy(hl[m].fullname, hostname);
                  }
                  hl[m].in_dir_config = YES;
                  hl[m].protocol |= protocol;
                  break;
               }
            }

            /* Add host to host list if it is new */
            if (new == YES)
            {
               /* Check if buffer for host list structure is large enough. */
               if ((no_of_hosts % HOST_BUF_SIZE) == 0)
               {
                  int new_size,
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
                  (void)memset((char *)hl + offset, 0, new_size);
               }

               /* Store hostname */
               (void)strcpy(hl[no_of_hosts].host_alias, host_alias);
               (void)strcpy(hl[no_of_hosts].fullname, hostname);
               hl[no_of_hosts].real_hostname[0][0] = '\0';
               hl[no_of_hosts].real_hostname[1][0] = '\0';
               hl[no_of_hosts].proxy_name[0]       = '\0';
               hl[no_of_hosts].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
               hl[no_of_hosts].max_errors          = DEFAULT_MAX_ERRORS;
               hl[no_of_hosts].retry_interval      = DEFAULT_RETRY_INTERVAL;
               hl[no_of_hosts].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
               hl[no_of_hosts].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
               hl[no_of_hosts].file_size_offset    = DEFAULT_FILE_SIZE_OFFSET;
               hl[no_of_hosts].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
               hl[no_of_hosts].number_of_no_bursts = DEFAULT_NO_OF_NO_BURSTS;
               hl[no_of_hosts].in_dir_config       = YES;
               hl[no_of_hosts].protocol            = protocol;

               no_of_hosts++;
            }
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ insert_dir() ++++++++++++++++++++++++++++*/
static void
insert_dir(struct dir_group *dir)
{
   int i,
       j;

   for (i = 0; i < dir->fgc; i++)
   {
      for (j = 0; j < dir->file[i].dgc; j++)
      {
         copy_job(i, j, dir);
      } /* next destination group */
   } /* next file group */

   return;
}


/*----------------------------- copy_job() -----------------------------*/
/*                              ----------                              */
/* Description: This function copies one job into the temporary memory  */
/*              p_it created in the top level function                  */
/*              eval_dir_config(). Each recipient in the DIR_CONFIG is  */
/*              counted as one job. Data is stored are as followed: job */
/*              type, priority, directory, no. of files/filters,        */
/*              file/filters, no. loc. opt., loc. options, no. std.     */
/*              opt., std. options and recipient. Additionally a        */
/*              pointer array is created that always keeps a pointer    */
/*              offset to each of the above listed items. Since all     */
/*              data for each recipient in a recipient group is the     */
/*              same, except for the recipient itself, we just store a  */
/*              new pointer array for each new recipient.               */
/*----------------------------------------------------------------------*/
static void
copy_job(int              file_no,
         int              dest_no,
         struct dir_group *dir)
{
   int            i,
                  j,
                  k,
                  offset,
                  options,       /* Counts no. of local options found.  */
                  priority,
                  length;        /* Storage for length of RENAME_ID.    */
   size_t         new_size;
   char           buffer[MAX_INT_LENGTH],
                  *ptr = NULL,     /* Pointer where data is to be       */
                                   /* stored.                           */
                  *p_start = NULL, /* Points to start of local options. */
                  *p_offset = NULL,
                  *p_loption[LOCAL_OPTION_POOL_SIZE] =
                  {
                     RENAME_ID,
                     EXEC_ID,
                     TIME_ID,
                     BASENAME_ID,
                     EXTENSION_ID,
                     ADD_PREFIX_ID,
                     DEL_PREFIX_ID,
#ifdef _WITH_AFW2WMO
                     AFW2WMO_ID,
#endif /* _WITH_AFW2WMO */
                     TIFF2GTS_ID,
                     GTS2TIFF_ID,
                     EXTRACT_ID,
                     ASSEMBLE_ID,
                     TAR_ID,
                     UNTAR_ID,
                     COMPRESS_ID,
                     UNCOMPRESS_ID,
                     GZIP_ID,
                     GUNZIP_ID,
                     HOLD_ID
                  };
   struct p_array *p_ptr = NULL;

   /* Check if the buffer for pointers is large enough */
   if ((job_no % PTR_BUF_SIZE) == 0)
   {
      /* Calculate new size of pointer buffer */
      new_size = ((job_no / PTR_BUF_SIZE) + 1) * PTR_BUF_SIZE * sizeof(struct p_array);

      /* Increase the space for pointers by PTR_BUF_SIZE */
      if ((pp_it = realloc(pp_it, new_size)) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not allocate memory for pointer buffer : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Set pointer array */
   p_ptr = pp_it;

   /* Position the data pointer */
   ptr = p_it + data_length;
   p_offset = p_it;

   /* Insert priority */
   priority = DEFAULT_PRIORITY;
   for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
   {
      if (strncmp(dir->file[file_no].dest[dest_no].options[i], PRIORITY_ID, strlen(PRIORITY_ID)) == 0)
      {
         priority = dir->file[file_no].dest[dest_no].options[i][strlen(PRIORITY_ID) + 1];

         /* Remove priority, since it is no longer needed */
         for (j = i; j <= dir->file[file_no].dest[dest_no].oc; j++)
         {
            (void)strcpy(dir->file[file_no].dest[dest_no].options[j],
                         dir->file[file_no].dest[dest_no].options[j + 1]);
         }
         dir->file[file_no].dest[dest_no].oc--;

         break;
      }
   }
   *ptr = priority;
   p_ptr[job_no].ptr[0] = (char *)(ptr - p_offset);
   ptr++;

   /* Insert directory */
   if ((file_no == 0) && (dest_no == 0))
   {
      if (dir->option[0] == '\0')
      {
         offset = sprintf(ptr, "%s", dir->location);
      }
      else
      {
         offset = sprintf(ptr, "%s%c%c%s",
                          dir->location, '\000', '\001', dir->option);
      }
      p_ptr[job_no].ptr[1] = (char *)(ptr - p_offset);
      ptr += offset + 1;
   }
   else
   {
      p_ptr[job_no].ptr[1] = p_ptr[job_no - 1].ptr[1];
   }

   /* Insert files */
   p_ptr[job_no].ptr[2] = (char *)(ptr - p_offset);
   offset = sprintf(ptr, "%d", dir->file[file_no].fc);
   ptr += offset + 1;
   p_ptr[job_no].ptr[3] = (char *)(ptr - p_offset);
   if (dest_no == 0)
   {
      for (i = 0; i < dir->file[file_no].fc; i++)
      {
         offset = sprintf(ptr, "%s", dir->file[file_no].files[i]);
         ptr += offset + 1;
      }
      ptr++;
   }
   else
   {
      p_ptr[job_no].ptr[3] = p_ptr[job_no - 1].ptr[3];
   }

   /* Insert local options. These are the options that AMG */
   /* has to handle. They are:                             */
   /*        - priority       (special option)             */
   /*        - hold           (special option)             */
   /*        - tar and untar                               */
   /*        - compress and uncompress                     */
   /*        - gzip and gunzip                             */
   /*        - rename                                      */
   /*        - exec                                        */
   /*        - basename                                    */
   /*        - prefix                                      */
   /*        - tiff2gts and gts2tiff                       */
   /*        - assemble                                    */
   /*        - extract                                     */
   /*        - time                                        */
   p_ptr[job_no].ptr[4] = (char *)(ptr - p_offset);
   if (dir->file[file_no].dest[dest_no].oc > 0)
   {
      p_start = ptr;
      options = 0;
      for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
      {
         for (k = 0; k < LOCAL_OPTION_POOL_SIZE; k++)
         {
            length = strlen(p_loption[k]);
            if (strncmp(dir->file[file_no].dest[dest_no].options[i], p_loption[k], length) == 0)
            {
               /* save the local option in shared memory region */
               offset = sprintf(ptr, "%s", dir->file[file_no].
                                dest[dest_no].options[i]) + 1;
               ptr += offset;
               options++;

               /* remove the option */
               for (j = i; j <= dir->file[file_no].dest[dest_no].oc; j++)
               {
                  (void)strcpy(dir->file[file_no].dest[dest_no].options[j],
                               dir->file[file_no].dest[dest_no].options[j + 1]);
               }
               dir->file[file_no].dest[dest_no].oc--;
               i--;

               break;
            }
         }
      }
      if (options > 0)
      {
         ptr++;
         offset = sprintf(buffer, "%d", options) + 1;

         /* Now move local options 'offset' Bytes forward   */
         /* so we can store the no. of local options before */
         /* the actual data.                                */
         memmove((p_start + offset), p_start, (ptr - p_start));
         ptr++;

         /* Store position of data for local options */
         p_ptr[job_no].ptr[5] = (char *)(p_start + offset - p_offset);

         /* Store number of local options */
         (void)strcpy(p_start, buffer);
      }
      else
      {
         /* No local options */
         *ptr = '0';
         ptr++;
         *ptr = '\0';
         ptr++;
         p_ptr[job_no].ptr[5] = NULL;
      }

      /* Insert standard options */
      p_ptr[job_no].ptr[6] = (char *)(ptr - p_offset);
      offset = sprintf(ptr, "%d", dir->file[file_no].dest[dest_no].oc);
      ptr += offset + 1;
      p_ptr[job_no].ptr[7] = (char *)(ptr - p_offset);

      if (dir->file[file_no].dest[dest_no].oc > 0)
      {
         for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
         {
            ptr += sprintf(ptr, "%s\n",
                           dir->file[file_no].dest[dest_no].options[i]);
         }
         *(ptr - 1) = '\0';

         /*
          * NOTE: Here we insert a new-line after each option, except
          *       for the the last one. When creating the message, all
          *       that needs to be done is to copy the string
          *       db[i].soptions. (see process dir_config)
          */
      }
      else /* No standard options */
      {
         p_ptr[job_no].ptr[7] = NULL;
      }
   }
   else /* No local and standard options */
   {
      /* No local options */
      *ptr = '0';
      ptr++;
      *ptr = '\0';
      ptr++;
      p_ptr[job_no].ptr[5] = NULL;

      /* No standard options */
      p_ptr[job_no].ptr[6] = (char *)(ptr - p_offset);
      *ptr = '0';
      ptr++;
      *ptr = '\0';
      ptr++;
      p_ptr[job_no].ptr[7] = NULL;
   }

   /* Insert recipient */
   p_ptr[job_no].ptr[8] = (char *)(ptr - p_offset);
   offset = sprintf(ptr, "%s", dir->file[file_no].dest[dest_no].recipient[0]);
   ptr += offset + 1;
   ptr++;

   /* increase job number */
   job_no++;

   /*
    * Since each recipient counts as one job, create and store
    * the data for each recipient. To save memory lets not always
    * copy all the data. This can be done by just saving the
    * recipient, since this is the only information that differs
    * for this recipient group.
    */
   for (i = 1; i < dir->file[file_no].dest[dest_no].rc; i++)
   {
      /* Check if the buffer for pointers is large enough */
      if ((job_no % PTR_BUF_SIZE) == 0)
      {
         /* Calculate new size of pointer buffer */
         new_size = ((job_no / PTR_BUF_SIZE) + 1) * PTR_BUF_SIZE * sizeof(struct p_array);

         /* Increase the space for pointers by PTR_BUF_SIZE */
         if ((pp_it = realloc(pp_it, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not allocate memory for pointer buffer : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /* Set pointer array */
         p_ptr = pp_it;
      }

      (void)memcpy(&p_ptr[job_no], &p_ptr[job_no - i], sizeof(struct p_array));
      p_ptr[job_no].ptr[8] = (char *)(ptr - p_offset);
      offset = sprintf(ptr, "%s",
                       dir->file[file_no].dest[dest_no].recipient[i]);
      ptr += offset + 1;

      /* increase job number */
      job_no++;
   } /* for (i = 1; i < dir->file[file_no].dest[dest_no].rc; i++) */

   /* Save data length */
   data_length = (ptr - p_offset) * sizeof(char);

   return;
}


/*++++++++++++++++++++++++++++++ copy_to_shm() ++++++++++++++++++++++++++*/
/*                               -------------                           */
/* Description: Creates a shared memory area and copies the number of    */
/*              jobs, the pointer array and the data into this area.     */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
copy_to_shm(void)
{
   int            size,
                  size_ptr_array;
   char           *p_shm_reg,
                  *ptr,
                  *p_data;
   struct p_array *p_ptr;

   /* Do not forget to remove the old regions */
   if (shm_id > -1)  /* Was area created?  */
   {
      if (shmctl(shm_id, IPC_RMID, 0) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not remove shared memory region %d : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* First test if there is any data at all for */
   /* this job type.                             */
   if (data_length > 0)
   {
      int loop_counter = 0;

      p_ptr = pp_it;
      p_data = p_it;

      /* First calculate the size for each region */
      size_ptr_array = job_no * sizeof(struct p_array);
      size = sizeof(int) + data_length + size_ptr_array + 1;

      /* Create shared memory regions */
      while (((shm_id = shmget(IPC_PRIVATE,
                               size,
                               IPC_CREAT | IPC_EXCL | SHM_R | SHM_W)) == -1) &&
             (errno == EEXIST))
      {
         if (loop_counter == 5)
         {
            break;
         }
         loop_counter++;
         (void)sleep(1);
      }

      if (shm_id == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not create shared memory region : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);

         exit(INCORRECT);
      }

      /* Attach to shared memory regions */
      if ((void *)(p_shm_reg = shmat(shm_id, 0, 0)) == (void *) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not attach to shared memory region : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         exit(INCORRECT);
      }

      ptr = p_shm_reg;

      /* Copy number of pointers into shared memory */
      *(int*)ptr = job_no;
      ptr += sizeof(int);

      /* Copy pointer array into shared memory */
      memcpy(ptr, p_ptr, size_ptr_array);
      ptr += size_ptr_array;

      /* Copy data into shared memory region */
      memcpy(ptr, p_data, data_length);

      /* Detach shared memory regions */
      if (shmdt(p_shm_reg) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not detach shared memory region %d : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      /* We did not create a shared memory area. */
      shm_id = -1;
   }

   return;
}


/*++++++++++++++++++++++++ count_new_lines() ++++++++++++++++++++++++++++*/
static void
count_new_lines(char *p_start, char *p_end, int *line_counter)
{
   while (p_start != p_end)
   {
      if (*p_start == '\n')
      {
         *line_counter += 1;
      }
      p_start++;
   }
   if (*p_start == '\n')
   {
      *line_counter += 1;
   }

   return;
}


/*++++++++++++++++++++++++++ store_full_path() ++++++++++++++++++++++++++*/
static void
store_full_path(char *p_user, char *p_path)
{
   struct passwd *pwd;

   if ((pwd = getpwnam(p_user)) == NULL)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Cannot find users %s working directory in /etc/passwd : %s (%s %d)\n",
                p_user, strerror(errno),
                __FILE__, __LINE__);
   }
   else
   {
      if ((p_path[0] == '/') && (p_path[1] != '\0'))
      {
         char *p_path_start = p_path,
              tmp_path[MAX_PATH_LENGTH];

         if ((p_path[1] == '~') || (p_path[2] == '~'))
         {
            p_path_start += 2;
            while ((*p_path_start != '/') && (*p_path_start != ';') &&
                   (*p_path_start != '#') && (*p_path_start != ' ') &&
                   (*p_path_start != '\t') && (*p_path_start != '\n') &&
                   (*p_path_start != '\0'))
            {
               p_path_start++;
            }
         }
         (void)strcpy(tmp_path, p_path_start);
         (void)strcpy(p_path + 1, pwd->pw_dir);
         (void)strcat(p_path + 1, tmp_path);
      }
      else
      {
         *p_path = '/';
         (void)strcpy(p_path + 1, pwd->pw_dir);
      }
   }

   return;
}
