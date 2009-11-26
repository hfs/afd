/*
 *  fra_edit.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fra_edit - changes certain values int the FRA
 **
 ** SYNOPSIS
 **   fra_edit [working directory] diralias|position
 **
 ** DESCRIPTION
 **   So far this program can change the following values:
 **   total_file_counter (fd), total_file_size (bd) and error_counter
 **   (ec).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.09.2007 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <termios.h>
#include <unistd.h>                      /* sleep(), STDERR_FILENO       */
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include "version.h"

/* Local functions. */
static unsigned char       get_key(void);
static jmp_buf             env_alarm;
static void                menu(int),
                           usage(char *),
                           sig_handler(int),
                           sig_alarm(int);

/* Global variables. */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_fd = -1,
                           fra_id,
                           no_of_dirs = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
char                       *p_work_dir;
struct termios             buf,
                           set;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fra_edit() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          position = -1,
                leave_flag = NO,
                ret;
   unsigned int value;
   char         dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
                work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         (void)strcpy(dir_alias, argv[1]);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if ((ret = fra_attach()) < 0)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FRA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr,
                       _("ERROR   : Failed to attach to FRA. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcgetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(0);
   }

   if (position < 0)
   {
      if ((position = get_dir_position(fra, dir_alias, no_of_dirs)) < 0)
      {
         (void)fprintf(stderr,
                       _("ERROR   : Could not find directory %s in FRA. (%s %d)\n"),
                       dir_alias, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   for (;;)
   {
      menu(position);

      switch (get_key())
      {
         case 0   : break;
         case '1' : (void)fprintf(stderr, _("\n\n     Enter value [1] : "));
                    (void)scanf("%u", &value);
                    fra[position].files_in_dir = (int)value;
                    break;
         case '2' : (void)fprintf(stderr, _("\n\n     Enter value [2] : "));
                    (void)scanf("%u", &value);
                    fra[position].bytes_in_dir = value;
                    break;
         case '3' : (void)fprintf(stderr, _("\n\n     Enter value [3] : "));
                    (void)scanf("%u", &value);
                    fra[position].files_queued = value;
                    break;
         case '4' : (void)fprintf(stderr, _("\n\n     Enter value [4] : "));
                    (void)scanf("%u", &value);
                    fra[position].bytes_in_queue = value;
                    break;
         case '5' : (void)fprintf(stderr, _("\n\n     Enter value [5] : "));
                    (void)scanf("%u", &value);
                    fra[position].error_counter = value;
                    break;
         case 'x' :
         case 'Q' :
         case 'q' : leave_flag = YES;
                    break;
         default  : (void)printf(_("Wrong choice!\n"));
                    (void)sleep(1);
                    break;
      }

      if (leave_flag == YES)
      {
         (void)fprintf(stdout, "\n\n");
         break;
      }
      else
      {
         (void)my_usleep(100000L);
      }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ menu() +++++++++++++++++++++++++++++++*/
static void
menu(int position)
{
   (void)fprintf(stdout, "\033[2J\033[3;1H"); /* Clear the screen (CLRSCR). */
   (void)fprintf(stdout, "\n\n                     FRA Editor (%s)\n\n", fra[position].dir_alias);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        | Key | Description      | current value  |\n");
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        |  1  |files_in_dir      | %14u |\n", fra[position].files_in_dir);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  2  |bytes_in_dir      | %14ld |\n", (pri_off_t)fra[position].bytes_in_dir);
#else
   (void)fprintf(stdout, "        |  2  |bytes_in_dir      | %14lld |\n", (pri_off_t)fra[position].bytes_in_dir);
#endif
   (void)fprintf(stdout, "        |  3  |files_queued      | %14u |\n", fra[position].files_queued);
#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "        |  4  |bytes_in_queue    | %14ld |\n", (pri_off_t)fra[position].bytes_in_queue);
#else
   (void)fprintf(stdout, "        |  4  |bytes_in_queue    | %14lld |\n", (pri_off_t)fra[position].bytes_in_queue);
#endif
   (void)fprintf(stdout, "        |  5  |error counter     | %14d |\n", fra[position].error_counter);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");

   return;
}


/*+++++++++++++++++++++++++++++++ get_key() +++++++++++++++++++++++++++++*/
static unsigned char
get_key(void)
{
   static unsigned char byte;

   if ((signal(SIGQUIT, sig_handler) == SIG_ERR) ||
       (signal(SIGINT, sig_handler) == SIG_ERR) ||
       (signal(SIGTSTP, sig_handler) == SIG_ERR) ||
       (signal(SIGALRM, sig_alarm) == SIG_ERR))
   {
      (void)fprintf(stderr, _("ERROR   : signal() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcgetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   set = buf;

   set.c_lflag &= ~ICANON;
   set.c_lflag &= ~ECHO;
   set.c_cc[VMIN] = 1;
   set.c_cc[VTIME] = 0;

   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &set) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (setjmp(env_alarm) != 0)
   {
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
      {
         (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      return(0);
   }
   alarm(5);
   if (read(STDIN_FILENO, &byte, 1) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : read() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   alarm(0);

   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   return(byte);
}


/*------------------------------ sig_alarm() ----------------------------*/
static void
sig_alarm(int signo)
{
   longjmp(env_alarm, 1);
}


/*---------------------------- sig_handler() ----------------------------*/
static void
sig_handler(int signo)
{
   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
   {
      (void)fprintf(stderr, _("ERROR   : tcsetattr() error : %s (%s %d)\n"),
                    strerror(errno), __FILE__, __LINE__);
   }
   exit(0);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] dir_alias|position\n"),
                 progname);
   return;
}
