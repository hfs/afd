/*
 *  set_pw.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   set_pw - sets password for the given user/hostname or job ID
 **
 ** SYNOPSIS
 **   set_pw [-w <AFD work dir>] [--version] [-s] -i <job id>|-c <user@hostname>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** SEE ALSO
 **   common/get_hostname.c, common/create_message.c, fd/get_job_data.c,
 **   fd/eval_recipient.c, tools/get_dc_data.c, fd/init_msg_buffer.c,
 **   amg/store_passwd.c
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.08.2005 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>                  /* open()                            */
#include <termios.h>
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#ifdef HAVE_MMAP
#include <sys/mman.h>               /* mmap()                            */
#endif
#include <errno.h>
#include "permission.h"
#include "version.h"

/* Global variables. */
int                  sys_log_fd = STDERR_FILENO;
char                 *p_work_dir = NULL;
const char           *sys_log_name = SYSTEM_LOG_FIFO;
struct termios       buf,
                     set;

/* Local function prototypes. */
static void          sig_handler(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                 fd,
                       i,
                       no_of_job_ids,
                       *no_of_passwd,
                       read_from_stdin,
                       user_offset;
   unsigned int        job_id;
   size_t              size;
   char                current_user[MAX_FULL_USER_ID_LENGTH],
                       fake_user[MAX_FULL_USER_ID_LENGTH],
                       file[MAX_PATH_LENGTH],
                       hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                       *perm_buffer,
                       *ptr,
                       uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 3],
                       user[MAX_USER_NAME_LENGTH + 1],
                       work_dir[MAX_PATH_LENGTH];
   unsigned char       passwd[MAX_USER_NAME_LENGTH];
   struct passwd_buf  *pwb;
   struct job_id_data *jd;
   struct stat        stat_buf;

   CHECK_FOR_VERSION(argc, argv);
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (get_arg(&argc, argv, "-s", NULL, 0) == SUCCESS)
   {
      read_from_stdin = YES;
   }
   else
   {
      read_from_stdin = NO;
   }
   if (get_arg(&argc, argv, "-p", user, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      user_offset = 0;
   }
   else
   {
      user_offset = strlen(user);
   }

   if (get_arg(&argc, argv, "-c", uh_name,
               MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 2) == SUCCESS)
   {
      ptr = uh_name;
      i = 0;
      while ((*(ptr + i) != '@') && (*(ptr + i) != '\0') &&
             (i < MAX_USER_NAME_LENGTH))
      {
         user[i] = *(ptr + i);
         i++;
      }
      if (*(ptr + i) != '@')
      {
         (void)fprintf(stderr,
                       "Invalid user hostname combination %s, it should be <user>@<hostname>.\n",
                       uh_name);
         exit(INCORRECT);
      }
      user[i] = '\0';
      ptr += i + 1;
      i = 0;
      while ((*(ptr + i) != '\0') && (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         hostname[i] = *(ptr + i);
         i++;
      }
      if (*(ptr + i) != '\0')
      {
         (void)fprintf(stderr,
                       "Invalid user hostname combination %s, it should be <user>@<hostname>.\n",
                       uh_name);
         exit(INCORRECT);
      }
      hostname[i] = '\0';
   }
   else if (get_arg(&argc, argv, "-i", uh_name,
                    MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 2) == SUCCESS)
        {
           unsigned long ulong_val;

           if ((ulong_val = strtoul(uh_name, NULL, 16)) == ULONG_MAX)
           {
              (void)fprintf(stderr,
                            "Unable to convert %s, must be a hex number not longer then 32 bits.\n",
                            uh_name);
              exit(INCORRECT);
           }
           job_id = (unsigned int)ulong_val;
           hostname[0] = '\0';
        }
        else
        {
           (void)fprintf(stderr,
                         "Usage: %s [-w <AFD work dir>] [--version] [-s] -i <job id>|-c <user@hostname>\n",
                         argv[0]);
           exit(INCORRECT);
        }

   /*
    * Ensure that the user may use this program.
    */
   get_user(current_user, fake_user, user_offset);
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         {
            int permission = NO;

            if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
                (perm_buffer[2] == 'l') &&
                ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
                 (perm_buffer[3] == '\t')))
            {
               permission = YES;
            }
            else
            {
               if (posi(perm_buffer, SET_PASSWD_PERM) != NULL)
               {
                  permission = YES;
               }
            }
            free(perm_buffer);
            if (permission != YES)
            {
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
               exit(INCORRECT);
            }
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default       :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /*
    * Attach to job ID database to check if the given user/hostname
    * or job ID data is valid. We do not want to insert some uneeded
    * data to the password database.
    */
   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if ((fd = open(file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr, "Failed to mmap() `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_job_ids = *(int *)ptr;
   ptr += AFD_WORD_OFFSET;
   jd = (struct job_id_data *)ptr;
   size = stat_buf.st_size;

   if (hostname[0] == '\0')
   {
      int gotcha = NO;

      for (i = 0; i < no_of_job_ids; i++)
      {
         if (job_id == jd[i].job_id)
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         (void)fprintf(stderr, "Failed to locate %u in local database.\n",
                       job_id);
         exit(INCORRECT);
      }
      else
      {
         ptr = jd[i].recipient;
         while ((*ptr != ':') && (*ptr != '\0'))
         {
            ptr++;
         }
         if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
         {
            ptr += 3; /* Away with '://' */
         }
         if ((*ptr != MAIL_GROUP_IDENTIFIER) && (*ptr != '\0'))
         {
            int j = 0;

#ifdef WITH_SSH_FINGERPRINT
            while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
#else
            while ((*ptr != ':') && (*ptr != '@') &&
#endif
                   (*ptr != '\0') && (j < MAX_USER_NAME_LENGTH))
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
               uh_name[j] = *ptr;
               ptr++; j++;
            }
            if ((*ptr == '\0') || (j == MAX_USER_NAME_LENGTH))
            {
               (void)fprintf(stderr, "Failed to store user.\n");
               exit(INCORRECT);
            }
            else
            {
               uh_name[j] = '\0';
               (void)strcpy(&uh_name[j], jd[i].host_alias);
            }
         }
      }
   }
   else
   {
      int gotcha = NO;

      for (i = 0; i < no_of_job_ids; i++)
      {
         if (CHECK_STRCMP(jd[i].host_alias, hostname) == 0)
         {
            ptr = jd[i].recipient;
            while ((*ptr != ':') && (*ptr != '\0'))
            {
               ptr++;
            }
            if ((*ptr == ':') && (*(ptr + 1) == '/') && (*(ptr + 2) == '/'))
            {
               ptr += 3; /* Away with '://' */
            }
            if ((*ptr != MAIL_GROUP_IDENTIFIER) && (*ptr != '\0'))
            {
               int j = 0;

#ifdef WITH_SSH_FINGERPRINT
               while ((*ptr != ':') && (*ptr != ';') && (*ptr != '@') &&
#else
               while ((*ptr != ':') && (*ptr != '@') &&
#endif
                      (*ptr != '\0') && (j < MAX_USER_NAME_LENGTH))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  uh_name[j] = *ptr;
                  ptr++; j++;
               }
               if ((*ptr == '\0') || (j == MAX_USER_NAME_LENGTH))
               {
                  (void)fprintf(stderr, "Failed to store user.\n");
                  exit(INCORRECT);
               }
               else
               {
                  uh_name[j] = '\0';
                  if (CHECK_STRCMP(uh_name, user) == 0)
                  {
                     (void)strcpy(&uh_name[j], hostname);
                     gotcha = YES;
                     break;
                  }
               }
            }
         }
      }
      if (gotcha == NO)
      {
         (void)fprintf(stderr, "Failed to locate %s@%s in local database.\n",
                       user, hostname);
         exit(INCORRECT);
      }
   }

   ptr = (char *)jd - AFD_WORD_OFFSET;
   if (munmap((char *)jd - AFD_WORD_OFFSET, size) == -1)
   {
      (void)fprintf(stderr, "Failed to munmap() `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   /*
    * Read password from stdin or keyboard.
    */
   (void)fprintf(stdout, "Enter password: ");
   (void)fflush(stdout);
   i = 0;
   if (read_from_stdin == YES)
   {
      unsigned char tmp_passwd[MAX_USER_NAME_LENGTH + 1],
                    *uptr;

      scanf("%s", tmp_passwd);
      uptr = tmp_passwd;
      while ((*uptr != '\0') && (i < MAX_USER_NAME_LENGTH))
      {
         if ((i % 2) == 0)
         {
            passwd[i] = *uptr - 24 + i;
         }
         else
         {
            passwd[i] = *uptr - 11 + i;
         }
         uptr++; i++;
      }
   }
   else
   {
      int            echo_disabled,
                     pas_char;
      FILE           *input;
      void           (*sig)(int);

      if ((input = fopen("/dev/tty", "w+")) == NULL)
      {
         input = stdin;
      }
      if ((sig = signal(SIGINT, sig_handler)) == SIG_ERR)
      {
         (void)fprintf(stderr, "ERROR   : signal() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if (tcgetattr(fileno(stdin), &buf) < 0)
      {
         (void)fprintf(stderr, "ERROR   : tcgetattr() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      set = buf;

      if (set.c_lflag & ECHO)
      {
         set.c_lflag &= ~ECHO;

         if (tcsetattr(fileno(stdin), TCSAFLUSH, &set) < 0)
         {
            (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         echo_disabled = YES;
      }
      else
      {
         echo_disabled = NO;
      }
      while (((pas_char = fgetc(input)) != '\n') && (pas_char != EOF))
      {
         if (i < MAX_USER_NAME_LENGTH)
         {
            if ((i % 2) == 0)
            {
               passwd[i] = (unsigned char)(pas_char - 24 + i);
            }
            else
            {
               passwd[i] = (unsigned char)(pas_char - 11 + i);
            }
            i++;
         }
      }
      if (echo_disabled == YES)
      {
         if (tcsetattr(fileno(stdin), TCSANOW, &buf) < 0)
         {
            (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if (signal(SIGINT, sig) == SIG_ERR)
      {
         (void)fprintf(stderr, "ERROR   : signal() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   (void)fprintf(stdout, "\n");
   passwd[i] = '\0';

   /*
    * Attach to password database and add the password.
    */
   size = (PWB_STEP_SIZE * sizeof(struct passwd_buf)) + AFD_WORD_OFFSET;
   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, PWB_DATA_FILE);
   if ((ptr = attach_buf(file, &fd, size, "set_pw",
#ifdef GROUP_CAN_WRITE
                         (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP),
                         NO)) == (caddr_t) -1)
#else
                         (S_IRUSR | S_IWUSR), NO)) == (caddr_t) -1)
#endif
   {
      (void)fprintf(stderr, "Failed to mmap() to `%s' : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_of_passwd = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   pwb = (struct passwd_buf *)ptr;

   for (i = 0; i < *no_of_passwd; i++)
   {
      if (CHECK_STRCMP(pwb[i].uh_name, uh_name) == 0)
      {
         (void)strcpy((char *)pwb[i].passwd, (char *)passwd);
         exit(SUCCESS);
      }
   }

   if ((*no_of_passwd != 0) &&
       ((*no_of_passwd % PWB_STEP_SIZE) == 0))
   {
      size_t new_size = (((*no_of_passwd / PWB_STEP_SIZE) + 1) *
                        PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)pwb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(fd, ptr, new_size)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "mmap_resize() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_passwd = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      pwb = (struct passwd_buf *)ptr;
   }
   (void)strcpy(pwb[*no_of_passwd].uh_name, uh_name);
   (void)strcpy((char *)pwb[*no_of_passwd].passwd, (char *)passwd);
   pwb[*no_of_passwd].dup_check = YES;
   (*no_of_passwd)++;

   exit(SUCCESS);
}


/*---------------------------- sig_handler() ----------------------------*/
static void
sig_handler(int signo)
{
   if (tcsetattr(fileno(stdin), TCSANOW, &buf) < 0)
   {
      (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   (void)fprintf(stdout, "\n");

   exit(INCORRECT);
}
