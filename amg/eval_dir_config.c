/*
 *  eval_dir_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Deutscher Wetterdienst (DWD),
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
 **   int eval_dir_config(size_t db_size, int *dc)
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
 **   14.02.2000 H.Kiehl Additional shared memory area for remote
 **                      directories.
 **   26.03.2000 H.Kiehl Addition of new header field DIR_OPTION_IDENTIFIER.
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
#include <unistd.h>                 /* geteuid(), R_OK, W_OK, X_OK       */
#include <pwd.h>                    /* getpwnam()                        */
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

#define LOCALE_DIR 0
#define REMOTE_DIR 1

/* External global variables */
#ifdef _DEBUG
extern FILE                *p_debug_file;
#endif
extern char                dir_config_file[],
                           *p_work_dir;
extern int                 dnb_fd,
                           shm_id,     /* The shared memory ID's for the */
                                       /* sorted data and pointers.      */
                           data_length,/* The size of data for one job.  */
                           sys_log_fd,
                           *no_of_dir_names,
                           no_of_hosts;/* The number of remote hosts to  */
                                       /* which files have to be         */
                                       /* transfered.                    */
extern mode_t              create_source_dir_mode;
extern struct host_list    *hl;        /* Structure that holds all the   */
                                       /* hosts.                         */
extern struct dir_name_buf *dnb;

/* Global variables */
struct dir_data            *dd = NULL;

/* Global local variables */
char                       *database = NULL;
int                        job_no;   /* By job number we here mean for   */
                                     /* each destination specified!      */
struct p_array             *pp;
static char                *p_t = NULL;   /* Start of directory table.   */

/* Local function prototypes */
static int                 check_hostname_list(char *, int),
                           count_new_lines(char *, char *);
static void                copy_to_shm(void),
                           copy_job(int, int, struct dir_group *),
                           insert_dir(struct dir_group *),
                           insert_hostname(struct dir_group *),
                           store_full_path(char *, char *);

#define RECIPIENT_STEP_SIZE 10
/* #define _LINE_COUNTER_TEST */

/* The following macro checks for spaces and comment */
#define CHECK_SPACE()                                       \
        {                                                   \
           if ((*ptr == ' ') || (*ptr == '\t'))             \
           {                                                \
              tmp_ptr = ptr;                                \
              while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t')) \
              {                                             \
                 tmp_ptr++;                                 \
              }                                             \
              switch (*tmp_ptr)                             \
              {                                             \
                 case '#' :  /* Found comment */            \
                    while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0')) \
                    {                                       \
                       tmp_ptr++;                           \
                    }                                       \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 case '\0':  /* Found end for this entry */ \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 case '\n':  /* End of line reached */      \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 default  :  /* option goes on */           \
                    ptr = tmp_ptr;                          \
                    break;                                  \
              }                                             \
           }                                                \
           else if (*ptr == '#')                            \
                {                                           \
                   tmp_ptr = ptr;                           \
                    while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0')) \
                    {                                       \
                       tmp_ptr++;                           \
                    }                                       \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                }                                           \
        }


/*########################## eval_dir_config() ##########################*/
int
eval_dir_config(size_t db_size, int *dc)
{
   int      i,
            j,
#ifdef _DEBUG
            k,
            m,
#endif
            ret,                 /* Return value.                 */
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
   static int       did_fd,
                    did_number;
   struct dir_group *dir;

   /* Allocate memory for the directory structure. */
   if ((dir = (struct dir_group *)malloc(sizeof(struct dir_group))) == NULL)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   dir->file = NULL;
   prev_user_name[0] = '\0';

   /* Read database file and store it into memory. */
   database = NULL;
   if (read_file(dir_config_file, &database) == INCORRECT)
   {
      exit(INCORRECT);
   }

   /* Create temporal storage area for job. */
   if ((p_t = calloc(db_size, sizeof(char))) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not allocate memory : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      free((void *)dir);
      return(INCORRECT);
   }

   if (dnb == NULL)
   {
      size_t      size = (DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
                         AFD_WORD_OFFSET;
      char        dir_name_file[MAX_PATH_LENGTH],
                  id_file[MAX_PATH_LENGTH],
                  *p_dir_buf;
      struct stat stat_buf;

      /*
       * Map to the directory name database.
       */
      (void)strcpy(dir_name_file, p_work_dir);
      (void)strcat(dir_name_file, FIFO_DIR);
      (void)strcpy(id_file, dir_name_file);
      (void)strcat(id_file, DIR_ID_FILE);
      (void)strcat(dir_name_file, DIR_NAME_FILE);
      if ((p_dir_buf = attach_buf(dir_name_file, &dnb_fd,
                            size, "AMG")) == (caddr_t) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Failed to mmap() to %s : %s (%s %d)\n",
                   dir_name_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_dir_names = (int *)p_dir_buf;
      p_dir_buf += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)p_dir_buf;

      /*
       * Get the highest current dir ID.
       */
      did_number = 0;
      if ((stat(id_file, &stat_buf) == -1) ||
          (stat_buf.st_size != sizeof(int)))
      {
         if ((errno == ENOENT) || (stat_buf.st_size != sizeof(int)))
         {
            if ((did_fd = open(id_file,
                               O_RDWR | O_CREAT | O_TRUNC,
                               S_IRUSR | S_IWUSR)) == -1)
            {
               (void)rec(sys_log_fd, FATAL_SIGN,
                         "Failed to open() and create %s : %s (%s %d)\n",
                         id_file, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (*no_of_dir_names > 0)
            {
               int i;

               for (i = 0; i < *no_of_dir_names; i++)
               {
                  if (dnb[i].dir_id > did_number)
                  {
                     did_number = dnb[i].dir_id;
                  }
               }
               did_number++;
            }
         }
         else
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to stat() %s : %s (%s %d)\n",
                      id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         if ((did_fd = open(id_file, O_RDWR)) == -1)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (read(did_fd, &did_number, sizeof(int)) != sizeof(int))
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "read() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if ((did_number > 2147483647) || (did_number < 0))
      {
         did_number = 0;
      }
   } /* if (dnb == NULL) */

   /* Lock the dir_name_buf structure so we do not get caught */
   /* when the FD is removing a directory.                    */
   lock_region_w(dnb_fd, (off_t)1);

   /* Initialise variables. */
   pp = NULL;
   job_no = 0;
   data_length = 0;
   unique_file_counter = 0;
   unique_dest_counter = 0;
   *dc = 0;
   ptr = database;

   /* Read each directory entry one by one until we reach the end. */
   while ((search_ptr = posi(ptr, DIR_IDENTIFIER)) != NULL)
   {
#ifdef _LINE_COUNTER_TEST
      (void)rec(sys_log_fd, INFO_SIGN,
                "Found DIR_IDENTIFIER at line %d\n",
                count_new_lines(database, search_ptr));
#endif
      tmp_ptr = search_ptr - strlen(DIR_IDENTIFIER) - 2;
      if ((tmp_ptr > database) && (*tmp_ptr != '\n'))
      {
         ptr = search_ptr;
         continue;
      }

      /* Initialise directory structure. */
      (void)memset((void *)dir, 0, sizeof(struct dir_group));

      /* Check if an alias is specified for this directory. */
      i = 0;
      if (*(search_ptr - 1) != '\n')
      {
         /* Ignore any data directly behind the identifier. */
         while ((*search_ptr != '\n') && (*search_ptr != '\0'))
         {
            if (*search_ptr == '#')
            {
               while ((*search_ptr != '\n') && (*search_ptr != '\0'))
               {
                  search_ptr++;
               }
            }
            else if ((*search_ptr == ' ') || (*search_ptr == '\t'))
                 {
                    search_ptr++;
                 }
                 else
                 {
                    dir->alias[i] = *search_ptr;
                    search_ptr++; i++;
                    if (i == MAX_DIR_ALIAS_LENGTH)
                    {
                       /* No more space left, so ignore rest. */
                       while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                       {
                          search_ptr++;
                       }
                    }
                 }
         }
         dir->alias[i] = '\0';
         search_ptr++;
      }
      ptr = search_ptr;

      /*
       ********************* Read directory ********************
       */
      /* Check if there is a directory. */
      if (*ptr == '\n')
      {
         /* Generate warning, that last dir entry does not */
         /* have a destination.                            */
         (void)rec(sys_log_fd, WARN_SIGN,
                   "In line %d, directory entry does not have a directory. (%s %d)\n",
                   count_new_lines(database, search_ptr), __FILE__, __LINE__);
         ptr++;

         continue;
      }

      /* Store directory name. */
      i = 0;
      dir->option[0] = '\0';
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         if (*ptr == '\\')
         {
            dir->location[i] = *(ptr + 1);
            i++;
            ptr += 2;
         }
         else
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
                     while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0'))
                     {
                        tmp_ptr++;
                     }
                     ptr = tmp_ptr;
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
            if ((i > 0) &&
                (((*(ptr - 1) == '\t') || (*(ptr - 1) == ' ')) &&
                  ((i < 2) || (*(ptr - 2) != '\\'))))
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
      }
      if ((*ptr == '\n') && (i > 0))
      {
         ptr++;
      }

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
             (CHECK_STRCMP(dir->location, prev_user_name) != 0))
         {
            struct passwd *pwd;

            if (*(tmp_ptr - 1) == '~')
            {
               if ((pwd = getpwuid(geteuid())) == NULL)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Cannot find working directory for user with the user ID %d in /etc/passwd (ignoring directory) : %s (%s %d)\n",
                            geteuid(), strerror(errno), __FILE__, __LINE__);
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
         dir->protocol = LOC;
      }
      else if (dir->location[0] == '/')
           {
              dir->type = LOCALE_DIR;
              dir->protocol = LOC;
           }
      else if ((dir->location[0] == 'f') && (dir->location[1] == 't') &&
               (dir->location[2] == 'p') && (dir->location[3] == ':') &&
               (dir->location[4] == '/') && (dir->location[5] == '/'))
           {
              dir->type = REMOTE_DIR;
              dir->protocol = FTP;
              (void)strcpy(dir->url, dir->location);

              /*
               * Cut away the URL and make the directory look like a
               * real directory of the following format:
               * $AFD_WORK_DIR/files/incoming/<scheme>/<user>@<hostname>/[<user>/]<remote dir>
               */
              if (create_remote_dir(dir->url, dir->location) == INCORRECT)
              {
                 continue;
              }
           }
           else
           {
              (void)rec(sys_log_fd, WARN_SIGN,
                        "Unknown or unsupported scheme, ignoring directory %s (%s %d)\n",
                        dir->location, __FILE__, __LINE__);
              continue;
           }

      /* Now lets check if this directory does exist and if we */
      /* do have enough permissions to work in this directory. */
      if (eaccess(dir->location, R_OK | W_OK | X_OK) < 0)
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
            } while (((error_condition = eaccess(dir->location, R_OK | W_OK | X_OK)) < 0) &&
                     (errno == ENOENT));

            if ((error_condition < 0) && (errno != ENOENT))
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Cannot access directory %s at line %d (Ignoring this entry) : %s (%s %d)\n",
                         dir->location, count_new_lines(database, ptr - 1),
                         strerror(errno), __FILE__, __LINE__);
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
               if (mkdir(dir->location, create_source_dir_mode) == -1)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to create directory %s at line %d (Ignoring this entry) : %s (%s %d)\n",
                            dir->location, count_new_lines(database, ptr - 1),
                            strerror(errno), __FILE__, __LINE__);
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
                         dir->location, count_new_lines(database, ptr - 1),
                         __FILE__, __LINE__);
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
                      dir->location, count_new_lines(database, ptr - 1),
                      strerror(errno), __FILE__, __LINE__);
            continue;
         }
      }

      /* Before we go on, we have to search for the beginning of */
      /* the next directory entry so we can mark the end for     */
      /* this directory entry.                                   */
      if ((end_dir_ptr = posi(ptr, DIR_IDENTIFIER)) != NULL)
      {
         /* First save char we encounter here. */
         tmp_dir_char = *end_dir_ptr;

         /* Now mark end of this directory entry. */
         *end_dir_ptr = '\0';

         /* Set flag that we found another entry. */
         other_dir_flag = YES;
      }
      else
      {
         other_dir_flag = NO;
      }

      /*
       ****************** Read Directory Options ***************
       */
      if ((search_ptr = posi(ptr, DIR_OPTION_IDENTIFIER)) != NULL)
      {
         int length = 0;

#ifdef _LINE_COUNTER_TEST
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Found DIR_OPTION_IDENTIFIER at line %d\n",
                   count_new_lines(database, search_ptr));
#endif
         if (*(search_ptr - 1) != '\n')
         {
            /* Ignore any data directly behind the identifier. */
            while ((*search_ptr != '\n') && (*search_ptr != '\0'))
            {
               search_ptr++;
            }
            search_ptr++;
         }
         while (*search_ptr == '#')
         {
            while ((*search_ptr != '\n') && (*search_ptr != '\0'))
            {
               search_ptr++;
            }
            search_ptr++;
         }
         ptr = search_ptr;

         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            if (*ptr != '\n')
            {
               /* Check if this is a comment line. */
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while (*ptr == '\n')
                  {
                     ptr++;
                  }
                  continue;
               }

               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  dir->dir_options[length] = *ptr;
                  ptr++; length++;
               }
               dir->dir_options[length++] = '\n';
               if (*ptr == '\n')
               {
                  ptr++;
               }
            }
         }
         dir->dir_options[length] = '\0';
      }
      else
      {
         dir->dir_options[0] = '\0';
      }

      /*
       ********************** Read filenames *******************
       */
      /* Position ptr after FILE_IDENTIFIER. */
      dir->fgc = 0;
      while ((search_ptr = posi(ptr, FILE_IDENTIFIER)) != NULL)
      {
#ifdef _LINE_COUNTER_TEST
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Found FILE_IDENTIFIER at line %d\n",
                   count_new_lines(database, search_ptr));
#endif
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

         if ((dir->fgc % FG_BUFFER_STEP_SIZE) == 0)
         {
            char   *ptr_start;
            size_t new_size;

            /* Calculate new size of file group buffer. */
            new_size = ((dir->fgc / FG_BUFFER_STEP_SIZE) + 1) *
                        FG_BUFFER_STEP_SIZE * sizeof(struct file_group);

            if (dir->fgc == 0)
            {
               if ((dir->file = (struct file_group *)malloc(new_size)) == NULL)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not malloc() memory : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
            else
            {
               if ((dir->file = (struct file_group *)realloc(dir->file, new_size)) == NULL)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not realloc() memory : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
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

         /* Store file-group name. */
         if (*ptr != '\n') /* Is there a file group name ? */
         {
            /* Store file group name. */
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
               /* Generate warning that this file entry is faulty. */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "In line %d, directory %s does not have a destination entry. (%s %d)\n",
                         count_new_lines(database, search_ptr),
                         dir->location, __FILE__, __LINE__);

               /* To read the next file entry, put back the char    */
               /* that was torn out to mark end of this file entry. */
               if (tmp_file_char != 1)
               {
                  *end_file_ptr = tmp_file_char;
               }

               /* No need to carry on with this file entry. */
               continue;
            }

            /* Make sure that a file group name was defined. */
            if (dir->file[dir->fgc].file_group_name[0] == '\0')
            {
               /* Create a unique file group name. */
               (void)sprintf(dir->file[dir->fgc].file_group_name,
                             "FILE_%d", unique_file_counter);
               unique_file_counter++;
            }
         }
         else /* No file group name defined. */
         {
            /* Create a unique file group name. */
            (void)sprintf(dir->file[dir->fgc].file_group_name,
                          "FILE_%d", unique_file_counter);
            unique_file_counter++;
         }

         /* Before we go on, we have to search for the beginning of */
         /* the next file group entry so we can mark the end for   */
         /* this file entry.                                       */
         if ((end_file_ptr = posi(ptr, FILE_IDENTIFIER)) != NULL)
         {
            /* First save char we encounter here. */
            tmp_file_char = *end_file_ptr;
   
            /* Now mark end of this file group entry. */
            *end_file_ptr = '\0';

            /* Set flag that we found another entry. */
            other_file_flag = YES;
         }
         else
         {
            other_file_flag = NO;
         }

         /* Store file names. */
         dir->file[dir->fgc].fc = 0;
         ptr++;
         if (*ptr == '\n') /* Are any files specified? */
         {
            /* We assume that all files in this */
            /* directory should be send.        */
            dir->file[dir->fgc].files[0] = '*';
            dir->file[dir->fgc].fc++;
            memset(&dir->file[dir->fgc].files[1], 0, MAX_FILE_MASK_BUFFER - 1);
         }
         else /* Yes, files are specified. */
         {
            int last_end = 0,
                total_length = 0;

            /* Read each filename line by line,  */
            /* until we encounter an empty line. */
            do
            {
               /* Store file name. */
               i = 0;
               while ((*ptr != '\n') && (*ptr != '\0') &&
                      ((total_length + i) < MAX_FILE_MASK_BUFFER))
               {
                  CHECK_SPACE();

                  /* Store file name. */
                  dir->file[dir->fgc].files[total_length + i] = *ptr;
                  ptr++; i++;
               }

               if ((total_length + i) >= MAX_FILE_MASK_BUFFER)
               {
                  if ((last_end == 0) && (dir->file[dir->fgc].fc == 0))
                  {
                     dir->file[dir->fgc].files[0] = '*';
                     dir->file[dir->fgc].fc++;
                     memset(&dir->file[dir->fgc].files[1], 0,
                            MAX_FILE_MASK_BUFFER - 1);
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "File mask entry at line %d to long, max buffer size is %d. (%s %d)\n",
                               count_new_lines(database, ptr),
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     memset(&dir->file[dir->fgc].files[last_end], 0,
                            MAX_FILE_MASK_BUFFER - last_end);
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "To many file mask at line %d, max buffer size is %d. (%s %d)\n",
                               count_new_lines(database, ptr),
                               __FILE__, __LINE__);
                  }
                  break;
               }
               if (i != 0)
               {
                  dir->file[dir->fgc].files[total_length + i] = '\0';
                  total_length += i + 1;
                  dir->file[dir->fgc].fc++;
                  last_end = total_length;
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
            (void)memset(&dir->file[dir->fgc].files[total_length], 0,
                         MAX_FILE_MASK_BUFFER - total_length);
         }

         /*
          ******************** Read destinations ******************
          */
         /* Position ptr after DESTINATION_IDENTIFIER. */
         ptr++;
         dir->file[dir->fgc].dgc = 0;
         while ((search_ptr = posi(ptr, DESTINATION_IDENTIFIER)) != NULL)
         {
#ifdef _LINE_COUNTER_TEST
            (void)rec(sys_log_fd, INFO_SIGN,
                      "Found DESTINATION_IDENTIFIER at line %d\n",
                      count_new_lines(database, search_ptr - 1));
#endif
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

            if ((dir->file[dir->fgc].dgc % DG_BUFFER_STEP_SIZE) == 0)
            {
               char   *ptr_start;
               size_t new_size;

               /* Calculate new size of destination group buffer. */
               new_size = ((dir->file[dir->fgc].dgc / DG_BUFFER_STEP_SIZE) + 1) * DG_BUFFER_STEP_SIZE * sizeof(struct dest_group);

               if ((dir->file[dir->fgc].dest = (struct dest_group *)realloc(dir->file[dir->fgc].dest, new_size)) == NULL)
               {
                  (void)rec(sys_log_fd, FATAL_SIGN,
                            "Could not realloc() memory : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }

               if (dir->file[dir->fgc].dgc > (DG_BUFFER_STEP_SIZE - 1))
               {
                  ptr_start = (char *)(dir->file[dir->fgc].dest) + (dir->file[dir->fgc].dgc * sizeof(struct dest_group));
               }
               else
               {
                  ptr_start = (char *)dir->file[dir->fgc].dest;
               }
               (void)memset(ptr_start, 0, (DG_BUFFER_STEP_SIZE * sizeof(struct dest_group)));
            }

            /* Store destination group name. */
            if (*ptr != '\n') /* Is there a destination group name? */
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

               /* Check if we encountered end of destination entry. */
               if (*ptr == '\0')
               {
                  /* Generate warning message, that no destination */
                  /* has been defined.                             */
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Directory %s at line %d does not have a destination entry for file group no. %d. (%s %d)\n",
                            dir->location, count_new_lines(database, ptr),
                            dir->fgc, __FILE__, __LINE__);

                  /* To read the next destination entry, put back the */
                  /* char that was torn out to mark end of this       */
                  /* destination entry.                               */
                  if (tmp_dest_char != 1)
                  {
                     *end_dest_ptr = tmp_dest_char;
                  }

                  continue; /* Go to next destination entry. */
               }
            }
            else /* No destination group name defined. */
            {
               /* Create a unique destination group name. */
               (void)sprintf(dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].\
                             dest_group_name, "DEST_%d", unique_dest_counter);
               unique_dest_counter++;
            }
            ptr++;

            /* Before we go on, we have to search for the beginning of */
            /* the next destination entry so we can mark the end for   */
            /* this destination entry.                                 */
            if ((end_dest_ptr = posi(ptr, DESTINATION_IDENTIFIER)) != NULL)
            {
               /* First save char we encounter here. */
               tmp_dest_char = *end_dest_ptr;
   
               /* Now mark end of this destination entry. */
               *end_dest_ptr = '\0';

               /* Set flag that we found another entry. */
               other_dest_flag = YES;
            }
            else
            {
               other_dest_flag = NO;
            }

            /*++++++++++++++++++++++ Read recipient +++++++++++++++++++++*/
            /* Position ptr after RECIPIENT_IDENTIFIER. */
            if ((search_ptr = posi(ptr, RECIPIENT_IDENTIFIER)) != NULL)
            {
#ifdef _LINE_COUNTER_TEST
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Found RECIPIENT_IDENTIFIER at line %d\n",
                         count_new_lines(database, search_ptr));
#endif
               if (*(search_ptr - 1) != '\n')
               {
                  /* Ignore any data directly behind the identifier. */
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
               }
               while (*search_ptr == '#')
               {
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
               }
               ptr = search_ptr;

               /* Read the address for each recipient line by */
               /* line, until we encounter an empty line.     */
               dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc = 0;

               RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                        RECIPIENT_STEP_SIZE, MAX_RECIPIENT_LENGTH, char);

               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  /* Check if this is a comment line. */
                  if (*ptr == '#')
                  {
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     ptr++;

                     goto check_dummy_line;
                  }

                  /* Store recipient. */
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
                           case '#' : /* Found comment. */
                              while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0'))
                              {
                                 tmp_ptr++;
                              }
                              ptr = tmp_ptr;
                              continue;

                           case '\0': /* Found end for this entry. */
                              ptr = tmp_ptr;
                              continue;

                           case '\n': /* End of line reached. */
                              ptr = tmp_ptr;
                              continue;

                           default  : /* We are at begining of a recipient */
                                      /* line, so let us continue.         */
                              search_ptr = ptr = tmp_ptr;
                              break;
                        }
                     }
                     dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].\
                             recipient[dir->file[dir->fgc].\
                             dest[dir->file[dir->fgc].dgc].rc][i] = *ptr;
                     ptr++; i++;
                  }
                  dir->file[dir->fgc].\
                          dest[dir->file[dir->fgc].dgc].\
                          recipient[dir->file[dir->fgc].\
                          dest[dir->file[dir->fgc].dgc].rc][i] = '\0';
                  ptr++;

                  /* Make sure that we did read a line. */
                  if (i != 0)
                  {
                     char real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1];

                     /* Check if we can extract the hostname. */
                     if (get_hostname(dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].\
                                      recipient[dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].rc],
                                      real_hostname) == INCORRECT)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to locate hostname in recipient string %s. Ignoring the recipient at line %d. (%s %d)\n",
                                  dir->file[dir->fgc].\
                                  dest[dir->file[dir->fgc].dgc].\
                                  recipient[dir->file[dir->fgc].\
                                  dest[dir->file[dir->fgc].dgc].rc],
                                  count_new_lines(database, search_ptr),
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        /* Check if sheme/protocol is correct. */
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
                                     count_new_lines(database, search_ptr),
                                     __FILE__, __LINE__);
                        }
                        else
                        {
                           *search_ptr = '\0';
                           if (CHECK_STRCMP(search_ptr - j, FTP_SHEME) != 0)
                           {
                              if (CHECK_STRCMP(search_ptr - j, LOC_SHEME) != 0)
                              {
#ifdef _WITH_SCP1_SUPPORT
                                 if (CHECK_STRCMP(search_ptr - j, SCP1_SHEME) != 0)
                                 {
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
                                    if (CHECK_STRCMP(search_ptr - j, WMO_SHEME) != 0)
                                    {
#endif
#ifdef _WITH_MAP_SUPPORT
                                       if (CHECK_STRCMP(search_ptr - j, MAP_SHEME) != 0)
                                       {
#endif
                                          if (CHECK_STRCMP(search_ptr - j, SMTP_SHEME) != 0)
                                          {
                                             (void)rec(sys_log_fd, WARN_SIGN,
                                                       "Unknown sheme <%s>. Ignoring recipient at line %d. (%s %d)\n",
                                                       search_ptr - j, count_new_lines(database, search_ptr),
                                                       __FILE__, __LINE__);
                                          }
                                          else
                                          {
                                             dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                             t_rc++;
                                             if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                             {
                                                int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                                REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                                 new_size, MAX_RECIPIENT_LENGTH, char);
                                             }
                                          }
#ifdef _WITH_MAP_SUPPORT
                                       }
                                       else
                                       {
                                          dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                          t_rc++;
                                          if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                          {
                                             int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                             REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                              new_size, MAX_RECIPIENT_LENGTH, char);
                                          }
                                       }
#endif
#ifdef _WITH_WMO_SUPPORT
                                    }
                                    else
                                    {
                                       dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                       t_rc++;
                                       if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                       {
                                          int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                          REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                           new_size, MAX_RECIPIENT_LENGTH, char);
                                       }
                                    }
#endif
#ifdef _WITH_SCP1_SUPPORT
                                 }
                                 else
                                 {
                                    dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                    t_rc++;
                                    if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                    {
                                       int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                       REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                        new_size, MAX_RECIPIENT_LENGTH, char);
                                    }
                                 }
#endif /* _WITH_SCP1_SUPPORT */
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
                                 if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                 {
                                    int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                    REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                     new_size, MAX_RECIPIENT_LENGTH, char);
                                 }
                              }
                           }
                           else
                           {
                              dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                              t_rc++;
                              if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                              {
                                 int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc / RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE;

                                 REALLOC_RT_ARRAY(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].recipient,
                                                  new_size, MAX_RECIPIENT_LENGTH, char);
                              }
                           }
                           *search_ptr = ':';
                        }
                     }
                  } /* if (i != 0) */

check_dummy_line:
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
            }

            /* Make sure that at least one recipient was defined. */
            if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc == 0)
            {
               if (search_ptr == NULL)
               {
                  search_ptr = ptr + 1;
               }

               /* Generate warning message. */
               (void)rec(sys_log_fd, WARN_SIGN,
                         "No recipient specified for %s at line %d. (%s %d)\n",
                         dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].dest_group_name,
                         count_new_lines(database, search_ptr),
                         __FILE__, __LINE__);

               /* To read the next destination entry, put back the */
               /* char that was torn out to mark end of this       */
               /* destination entry.                               */
               if (other_dest_flag == YES)
               {
                  *end_dest_ptr = tmp_dest_char;
               }

               /* Try to read the next destination. */
               continue;
            }
   
            /*++++++++++++++++++++++ Read options ++++++++++++++++++++++++*/
            /* Position ptr after OPTION_IDENTIFIER. */
            if ((search_ptr = posi(ptr, OPTION_IDENTIFIER)) != NULL)
            {
#ifdef _LINE_COUNTER_TEST
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Found OPTION_IDENTIFIER at line %d\n",
                         count_new_lines(database, search_ptr));
#endif
               if (*(search_ptr - 1) != '\n')
               {
                  /* Ignore any data directly behind the identifier. */
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  search_ptr++;
               }
               ptr = search_ptr;

               /* Read each option line by line, until */
               /* we encounter an empty line.          */
               dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc = 0;
               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  /* Store option. */
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

         /* Check if a destination was defined. */
         if (dir->file[dir->fgc].dgc == 0)
         {
            /* Generate error message. */
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Directory %s does not have a destination entry for file group no. %d. (%s %d)\n",
                      dir->location, dir->fgc, __FILE__, __LINE__);

            /* Reduce file counter, since this one is faulty. */
            dir->fgc--;
         }

         /* To read the next file entry, put back the char    */
         /* that was torn out to mark end of this file entry. */
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

      /* Check special case when no file identifier is found. */
      if ((dir->fgc == 0) && (dir->file != NULL))
      {
         /* We assume that all files in this dir should be send. */
         dir->file[dir->fgc].files[0] = '*';
         dir->file[dir->fgc].files[1] = '\0';
         dir->fgc++;
      }

      /* To read the next directory entry, put back the char    */
      /* that was torn out to mark end of this directory entry. */
      if (other_dir_flag == YES)
      {
         *end_dir_ptr = tmp_dir_char;
      }

      /* Check if a destination was defined for the last directory. */
      if ((dir->file == NULL) || (dir->file[0].dest == NULL) ||
          (dir->file[0].dest[0].rc == 0))
      {
         char *end_ptr;

         if (search_ptr == NULL)
         {
            end_ptr = ptr;
         }
         else
         {
            end_ptr = search_ptr;
         }
         (void)rec(sys_log_fd, WARN_SIGN,
                   "In line %d, no destination defined. (%s %d)\n",
                   count_new_lines(database, end_ptr), __FILE__, __LINE__);
      }
      else
      {
         int duplicate = NO;

         /* Check if this directory was not already specified. */
         for (j = 0; j < *dc; j++)
         {
            if (CHECK_STRCMP(dir->location, dd[j].dir_name) == 0)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Ignoring duplicate directory entry %s. (%s %d)\n",
                         dir->location, __FILE__, __LINE__);
               duplicate = YES;
               break;
            }
         }

         if (duplicate == NO)
         {
            if ((*dc % 10) == 0)
            {
               size_t new_size = (((*dc / 10) + 1) * 10) *
                                 sizeof(struct dir_data);

               if (*dc == 0)
               {
                  if ((dd = (struct dir_data *)malloc(new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "malloc() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
               else
               {
                  if ((dd = (struct dir_data *)realloc((char *)dd,
                                                       new_size)) == NULL)
                  {
                     (void)rec(sys_log_fd, FATAL_SIGN,
                               "realloc() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
               }
            }

            dd[*dc].dir_pos = lookup_dir_no(dir->location, &did_number);
            if (dir->alias[0] == '\0')
            {
               (void)sprintf(dir->alias, "DIR-%d", dnb[dd[*dc].dir_pos].dir_id);
            }
            else
            {
               /* Check if the directory alias was not already specified. */
               for (j = 0; j < *dc; j++)
               {
                  if (CHECK_STRCMP(dir->alias, dd[j].dir_alias) == 0)
                  {
                     (void)sprintf(dir->alias, "DIR-%d",
                                   dnb[dd[*dc].dir_pos].dir_id);
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Duplicate directory alias %s, giving it another alias: %s (%s %d)\n",
                               dd[j].dir_alias, dir->alias,
                               __FILE__, __LINE__);
                     break;
                  }
               }
            }

            (void)strcpy(dd[*dc].dir_alias, dir->alias);
            if (dir->type == LOCALE_DIR)
            {
               dd[*dc].fsa_pos = -1;
               dd[*dc].host_alias[0] = '\0';
               (void)strncpy(dd[*dc].url, dir->location, MAX_RECIPIENT_LENGTH);
               if (strlen(dir->location) >= MAX_RECIPIENT_LENGTH)
               {
                  dd[*dc].url[MAX_RECIPIENT_LENGTH - 1] = '\0';
               }
            }
            else
            {
               (void)strcpy(dd[*dc].url, dir->url);
               dd[*dc].fsa_pos = check_hostname_list(dir->url, RETRIEVE_FLAG);
               (void)strcpy(dd[*dc].host_alias, hl[dd[*dc].fsa_pos].host_alias);
               store_file_mask(dd[*dc].dir_alias, dir);
            }
            (void)strcpy(dd[*dc].dir_name, dir->location);
            dd[*dc].protocol = dir->protocol;

            /* Evaluate the directory options.  */
            eval_dir_options(*dc, dir->dir_options, dir->option);

            /* Increase directory counter */
            (*dc)++;

#ifdef _DEBUG
            (void)fprintf(p_debug_file,
                          "\n\n=================> Contents of directory struct %4d<=================\n",
                          *dc);
            (void)fprintf(p_debug_file,
                          "                   =================================\n");

            /* Print directory name and alias. */
            (void)fprintf(p_debug_file, "%3d: >%s< [%s]\n",
                          i + 1, dir->location, dir->alias);

            /* Print contents of each file group. */
            for (j = 0; j < dir->fgc; j++)
            {
               /* Print file group name. */
               (void)fprintf(p_debug_file, "    >%s<\n",
                             dir->file[j].file_group_name);

               /* Print the name of each file. */
               for (k = 0; k < dir->file[j].fc; k++)
               {
                  (void)fprintf(p_debug_file, "\t%3d: %s\n", k + 1,
                                dir->file[j].files[k]);
               }

               /* Print all destinations. */
               for (k = 0; k < dir->file[j].dgc; k++)
               {
                  /* First print destination group name. */
                  (void)fprintf(p_debug_file, "\t\t%3d: >%s<\n", k + 1,
                                dir->file[j].dest[k].dest_group_name);

                  /* Show all recipient's. */
                  (void)fprintf(p_debug_file, "\t\t\tRecipients:\n");
                  for (m = 0; m < dir->file[j].dest[k].rc; m++)
                  {
                     (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                   dir->file[j].dest[k].recipient[m]);
                  }

                  /* Show all options. */
                  (void)fprintf(p_debug_file, "\t\t\tOptions:\n");
                  for (m = 0; m < dir->file[j].dest[k].oc; m++)
                  {
                     (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                   dir->file[j].dest[k].options[m]);
                  }
               }
            }
#endif

            /* Insert directory into temporary memory. */
            insert_dir(dir);

            /* Insert hostnames into temporary memory. */
            insert_hostname(dir);
         } /* if (duplicate == NO) */
         for (j = 0; j < dir->fgc; j++)
         {
            int m;

            for (m = 0; m < dir->file[j].dgc; m++)
            {
               FREE_RT_ARRAY(dir->file[j].dest[m].recipient);
               dir->file[j].dest[m].recipient = NULL;
            }
            free(dir->file[j].dest);
         }
         free(dir->file);
         dir->file = NULL;
      }
   } /* while ((search_ptr = posi(ptr, DIR_IDENTIFIER)) != NULL) */

   /* See if there are any valid directory entries. */
   if (*dc == 0)
   {
      /* We assume there is no entry in database   */
      /* or we have finished reading the database. */
      ret = NO_VALID_ENTRIES;
   }
   else
   {
      /* Now copy data and pointers in their relevant */
      /* shared memory areas.                         */
      copy_to_shm();

      /* Don't forget to create the FSA (Filetransfer */
      /* Status Area).                                */
      create_sa(*dc);

      /* Tell user what we have found in DIR_CONFIG */
      if (*dc > 1)
      {
         (void)rec(sys_log_fd, INFO_SIGN,
                   "Found %d directory entries with %d recipients in %d destinations.\n",
                   *dc, t_rc, t_dgc);
      }
      else if ((*dc == 1) && (t_rc == 1))
           {
              (void)rec(sys_log_fd, INFO_SIGN,
                       "Found one directory entry with %d recipient in %d destination.\n",
                        t_rc, t_dgc);
           }
      else if ((*dc == 1) && (t_rc > 1) && (t_dgc == 1))
           {
              (void)rec(sys_log_fd, INFO_SIGN,
                        "Found one directory entry with %d recipients in %d destination.\n",
                        t_rc, t_dgc);
           }
           else
           {
              (void)rec(sys_log_fd, INFO_SIGN,
                        "Found %d directory entry with %d recipients in %d destinations.\n",
                        *dc, t_rc, t_dgc);
           }
      ret = SUCCESS;
   }

   /* Store the new directory ID number. */
   if (lseek(did_fd, 0, SEEK_SET) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not seek() to beginning of %s : %s (%s %d)\n",
                DIR_ID_FILE, strerror(errno), __FILE__, __LINE__);       
   }
   if (write(did_fd, &did_number, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Failed to write() new DID number to %s : %s (%s %d)\n",
                DIR_ID_FILE, strerror(errno), __FILE__, __LINE__);          
   }

   /* Release directory name buffer for FD. */
   unlock_region(dnb_fd, 1);

   /* Free all memory we allocated. */
   if (dd != NULL)
   {
      free((void *)dd);
   }
   free(database);
   free(p_t);
   if (pp != NULL)
   {
      free(pp);
   }
   free((void *)dir);

   return(ret);
}


/*+++++++++++++++++++++++++ insert_hostname() ++++++++++++++++++++++++++*/
static void
insert_hostname(struct dir_group *dir)
{
   register int i, j, k;

   for (i = 0; i < dir->fgc; i++)
   {
      for (j = 0; j < dir->file[i].dgc; j++)
      {
         for (k = 0; k < dir->file[i].dest[j].rc; k++)
         {
            (void)check_hostname_list(dir->file[i].dest[j].recipient[k],
                                      SEND_FLAG);
         }
      }
   }

   return;
}


/*------------------------- check_hostname_list() -----------------------*/
static int
check_hostname_list(char *recipient, int flag)
{
   int          i;
   unsigned int protocol;
   char         host_alias[MAX_HOSTNAME_LENGTH + 1],
                real_hostname[MAX_HOSTNAME_LENGTH + 1],
                new;

   /* Extract only hostname. */
   if (get_hostname(recipient, real_hostname) == INCORRECT)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to extract hostname in <%s>. (%s %d)\n",
                recipient, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (memcmp(recipient, FTP_SHEME, FTP_SHEME_LENGTH) == 0)
   {
      protocol = FTP_FLAG;
      if (flag & SEND_FLAG)
      {
         protocol |= SEND_FTP_FLAG;
      }
      else
      {
         protocol |= GET_FTP_FLAG;
      }
   }
   else if (memcmp(recipient, LOC_SHEME, LOC_SHEME_LENGTH) == 0)
        {
           protocol = LOC_FLAG;
           if (flag & SEND_FLAG)
           {
              protocol |= SEND_LOC_FLAG;
           }
        }
   else if (memcmp(recipient, SMTP_SHEME, SMTP_SHEME_LENGTH) == 0)
        {
           protocol = SMTP_FLAG;
           if (flag & SEND_FLAG)
           {
              protocol |= SEND_SMTP_FLAG;
           }
        }
#ifdef _WITH_SCP1_SUPPORT
   else if (memcmp(recipient, SCP1_SHEME, SCP1_SHEME_LENGTH) == 0)
        {
           protocol = SCP1_FLAG;
           if (flag & SEND_FLAG)
           {
              protocol |= SEND_SCP1_FLAG;
           }
        }
#endif /* _WITH_SCP1_SUPPORT */
#ifdef _WITH_WMO_SUPPORT
   else if (memcmp(recipient, WMO_SHEME, WMO_SHEME_LENGTH) == 0)
        {
           protocol = WMO_FLAG;
           if (flag & SEND_FLAG)
           {
              protocol |= SEND_WMO_FLAG;
           }
        }
#endif /* _WITH_WMO_SUPPORT */
#ifdef _WITH_MAP_SUPPORT
   else if (memcmp(recipient, MAP_SHEME, MAP_SHEME_LENGTH) == 0)
        {
           protocol = MAP_FLAG;
           if (flag & SEND_FLAG)
           {
              protocol |= SEND_MAP_FLAG;
           }
        }
#endif /* _WITH_MAP_SUPPORT */
        else
        {
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "Hmmm, there is something really wrong here, unable to determine the scheme for %s. (%s %d)\n",
                     __FILE__, __LINE__);
           (void)rec(sys_log_fd, ERROR_SIGN,
                     "For now lets just assume this is FTP and lets try to continue.\n");
           protocol = FTP_FLAG;
        }
   if (flag & SEND_FLAG)
   {
      protocol |= SEND_FLAG;
   }
   else if (flag & RETRIEVE_FLAG)
        {
           protocol |= RETRIEVE_FLAG;
        }

   t_hostname(real_hostname, host_alias);

   /* Check if host already exists. */
   new = YES;
   for (i = 0; i < no_of_hosts; i++)
   {
      if (CHECK_STRCMP(hl[i].host_alias, host_alias) == 0)
      {
         new = NO;

         if (hl[i].fullname[0] == '\0')
         {
            (void)strcpy(hl[i].fullname, real_hostname);
         }
         hl[i].in_dir_config = YES;
         hl[i].protocol |= protocol;
         break;
      }
   }

   /* Add host to host list if it is new. */
   if (new == YES)
   {
      /* Check if buffer for host list structure is large enough. */
      if ((no_of_hosts % HOST_BUF_SIZE) == 0)
      {
         int new_size,
             offset;

         /* Calculate new size for host list. */
         new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                    HOST_BUF_SIZE * sizeof(struct host_list);

         /* Now increase the space. */
         if ((hl = realloc(hl, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not reallocate memory for host list : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /* Initialise the new memory area. */
         new_size = HOST_BUF_SIZE * sizeof(struct host_list);
         offset = (no_of_hosts / HOST_BUF_SIZE) * new_size;
         (void)memset((char *)hl + offset, 0, new_size);
      }

      /* Store host data. */
      (void)strcpy(hl[no_of_hosts].host_alias, host_alias);
      (void)strcpy(hl[no_of_hosts].fullname, real_hostname);
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
      i = no_of_hosts;
      no_of_hosts++;
   }
   return(i);
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
/*              p_t created in the top level function eval_dir_config().*/
/*              Each recipient in the DIR_CONFIG is counted as one job. */
/*              Data is stored are as followed: job type, priority,     */
/*              directory, no. of files/filters, file/filters, number   */
/*              of local options, local options, number of standard     */
/*              options, standard options and recipient. Additionally a */
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
   int            i, j, k,
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
                     TIME_NO_COLLECT_ID,
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
                     ASSEMBLE_ID
                  };
   struct p_array *p_ptr = NULL;

   /* Check if the buffer for pointers is large enough. */
   if ((job_no % PTR_BUF_SIZE) == 0)
   {
      /* Calculate new size of pointer buffer */
      new_size = ((job_no / PTR_BUF_SIZE) + 1) *
                 PTR_BUF_SIZE * sizeof(struct p_array);

      /* Increase the space for pointers by PTR_BUF_SIZE */
      if ((pp = realloc(pp, new_size)) == NULL)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not allocate memory for pointer buffer : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Set pointer array. */
   p_ptr = pp;

   /* Position the data pointer. */
   ptr = p_t + data_length;
   p_offset = p_t;

   /* Insert priority. */
   priority = DEFAULT_PRIORITY;
   for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
   {
      if (strncmp(dir->file[file_no].dest[dest_no].options[i],
                  PRIORITY_ID, PRIORITY_ID_LENGTH) == 0)
      {
         char *tmp_ptr = &dir->file[file_no].dest[dest_no].options[i][PRIORITY_ID_LENGTH];

         while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t'))
         {
            tmp_ptr++;
         }
         if (isdigit(*tmp_ptr))
         {
            priority = *tmp_ptr;
         }

         /* Remove priority, since it is no longer needed. */
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

   /* Insert directory. */
   if ((file_no == 0) && (dest_no == 0))
   {
      offset = sprintf(ptr, "%s", dir->location);
      p_ptr[job_no].ptr[1] = (char *)(ptr - p_offset);
      ptr += offset + 1;

      offset = sprintf(ptr, "%s", dir->alias);
      p_ptr[job_no].ptr[2] = (char *)(ptr - p_offset);
      ptr += offset + 1;
   }
   else
   {
      p_ptr[job_no].ptr[1] = p_ptr[job_no - 1].ptr[1]; /* Directory */
      p_ptr[job_no].ptr[2] = p_ptr[job_no - 1].ptr[2]; /* Alias */
   }

   /* Insert file masks. */
   p_ptr[job_no].ptr[3] = (char *)(ptr - p_offset);
   offset = sprintf(ptr, "%d", dir->file[file_no].fc);
   ptr += offset + 1;
   p_ptr[job_no].ptr[4] = (char *)(ptr - p_offset);
   if (dest_no == 0)
   {
      char *p_file = dir->file[file_no].files;

      for (i = 0; i < dir->file[file_no].fc; i++)
      {
         NEXT(p_file);
      }
      offset = p_file - dir->file[file_no].files;
      (void)memcpy(ptr, dir->file[file_no].files, offset);
      ptr += offset + 1;
   }
   else
   {
      p_ptr[job_no].ptr[4] = p_ptr[job_no - 1].ptr[4];
   }

   /* Insert local options. These are the options that AMG */
   /* has to handle. They are:                             */
   /*        - priority       (special option)             */
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
   p_ptr[job_no].ptr[5] = (char *)(ptr - p_offset);
   if (dir->file[file_no].dest[dest_no].oc > 0)
   {
      p_start = ptr;
      options = 0;
      for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
      {
         for (k = 0; k < LOCAL_OPTION_POOL_SIZE; k++)
         {
            length = strlen(p_loption[k]);
            if (strncmp(dir->file[file_no].dest[dest_no].options[i],
                        p_loption[k], length) == 0)
            {
               /* Save the local option in shared memory region. */
               offset = sprintf(ptr, "%s", dir->file[file_no].
                                dest[dest_no].options[i]) + 1;
               ptr += offset;
               options++;

               /* Remove the option. */
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

         /* Now move local options 'offset' Bytes forward so we can */
         /* store the no. of local options before the actual data.  */
         memmove((p_start + offset), p_start, (ptr - p_start));
         ptr++;

         /* Store position of data for local options. */
         p_ptr[job_no].ptr[6] = (char *)(p_start + offset - p_offset);

         /* Store number of local options. */
         (void)strcpy(p_start, buffer);
      }
      else
      {
         /* No local options */
         *ptr = '0';
         ptr++;
         *ptr = '\0';
         ptr++;
         p_ptr[job_no].ptr[6] = NULL;
      }

      /* Insert standard options. */
      p_ptr[job_no].ptr[7] = (char *)(ptr - p_offset);
      offset = sprintf(ptr, "%d", dir->file[file_no].dest[dest_no].oc);
      ptr += offset + 1;
      p_ptr[job_no].ptr[8] = (char *)(ptr - p_offset);

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
      else /* No standard options. */
      {
         p_ptr[job_no].ptr[8] = NULL;
      }
   }
   else /* No local and standard options. */
   {
      /* No local options. */
      *ptr = '0';
      ptr++;
      *ptr = '\0';
      ptr++;
      p_ptr[job_no].ptr[6] = NULL;

      /* No standard options. */
      p_ptr[job_no].ptr[7] = (char *)(ptr - p_offset);
      *ptr = '0';
      ptr++;
      *ptr = '\0';
      ptr++;
      p_ptr[job_no].ptr[8] = NULL;
   }

   /* Insert recipient. */
   p_ptr[job_no].ptr[9] = (char *)(ptr - p_offset);
   offset = sprintf(ptr, "%s", dir->file[file_no].dest[dest_no].recipient[0]);
   ptr += offset + 1;
   ptr++;

   /* Increase job number. */
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
      /* Check if the buffer for pointers is large enough. */
      if ((job_no % PTR_BUF_SIZE) == 0)
      {
         /* Calculate new size of pointer buffer. */
         new_size = ((job_no / PTR_BUF_SIZE) + 1) *
                    PTR_BUF_SIZE * sizeof(struct p_array);

         /* Increase the space for pointers by PTR_BUF_SIZE. */
         if ((pp = realloc(pp, new_size)) == NULL)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not allocate memory for pointer buffer : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

         /* Set pointer array. */
         p_ptr = pp;
      }

      (void)memcpy(&p_ptr[job_no], &p_ptr[job_no - i], sizeof(struct p_array));
      p_ptr[job_no].ptr[9] = (char *)(ptr - p_offset);
      offset = sprintf(ptr, "%s",
                       dir->file[file_no].dest[dest_no].recipient[i]);
      ptr += offset + 1;

      /* Increase job number. */
      job_no++;
   } /* for (i = 1; i < dir->file[file_no].dest[dest_no].rc; i++) */

   /* Save data length. */
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
   /* Do not forget to remove the old regions. */
   if (shm_id > -1)  /* Was area created?  */
   {
      if (shmctl(shm_id, IPC_RMID, 0) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Could not remove shared memory region %d : %s (%s %d)\n",
                   shm_id, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* First test if there is any data at all for this job type. */
   if (data_length > 0)
   {
      int            loop_counter = 0,
                     size,
                     size_ptr_array;
      char           *p_shm_reg,
                     *ptr,
                     *p_data;
      struct p_array *p_ptr;

      /* First calculate the size for each region. */
      size_ptr_array = job_no * sizeof(struct p_array);
      size = sizeof(int) + data_length + size_ptr_array + sizeof(char);

      /* Create shared memory regions. */
      while (((shm_id = shmget(IPC_PRIVATE, size,
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

      /* Attach to shared memory regions. */
      if ((void *)(p_shm_reg = shmat(shm_id, 0, 0)) == (void *) -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not attach to shared memory region : %s (%s %d)\n",
                   strerror(errno),  __FILE__, __LINE__);
         exit(INCORRECT);
      }

      ptr = p_shm_reg;
      p_ptr = pp;
      p_data = p_t;

      /* Copy number of pointers into shared memory. */
      *(int*)ptr = job_no;
      ptr += sizeof(int);

      /* Copy pointer array into shared memory. */
      memcpy(ptr, p_ptr, size_ptr_array);
      ptr += size_ptr_array;

      /* Copy data into shared memory region. */
      memcpy(ptr, p_data, data_length);
      ptr += data_length;

      /* Detach shared memory regions. */
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
static int
count_new_lines(char *p_start, char *p_end)
{
   int line_counter = 0;

   while (p_start != p_end)
   {
      if (*p_start == '\n')
      {
         line_counter++;
      }
      p_start++;
   }
   if (*p_start == '\n')
   {
      line_counter++;
   }

   return(line_counter);
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
