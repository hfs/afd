/*
 *  fsa_edit.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   fsa_edit - changes certain values int the FSA
 **
 ** SYNOPSIS
 **   fsa_edit [working directory] hostname|position
 **
 ** DESCRIPTION
 **   So far this program can change the following values:
 **   total_file_counter (fc), total_file_size (fs) and error_counter
 **   (ec).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.1996 H.Kiehl Created
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

/* Local functions */
static unsigned char       get_key(void);
static jmp_buf             env_alarm;
static void                menu(int),
                           usage(char *),
                           sig_handler(int),
                           sig_alarm(int);

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_fd = -1,
                           fsa_id,
                           no_of_hosts = 0;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct termios             buf,
                           set;
struct filetransfer_status *fsa;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fsa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          position = -1,
                leave_flag = NO;
   unsigned int value;
   char         file_name[MAX_FILENAME_LENGTH + 1],
                hostname[MAX_HOSTNAME_LENGTH + 1],
                work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc == 2)
   {
      if (isdigit(argv[1][0]) != 0)
      {
         position = atoi(argv[1]);
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if (fsa_attach() < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, "ERROR   : tcgetattr() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(0);
   }

   if (position < 0)
   {
      if ((position = get_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not find host %s in FSA. (%s %d)\n",
                       hostname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   for (;;)
   {
      menu(position);

      switch(get_key())
      {
         case 0   : break;
         case '1' : (void)fprintf(stderr, "\n\n     Enter value [1] : ");
                    (void)scanf("%d", &value);
                    fsa[position].total_file_counter = (int)value;
                    break;
         case '2' : (void)fprintf(stderr, "\n\n     Enter value [2] : ");
                    (void)scanf("%d", &value);
                    fsa[position].total_file_size = value;
                    break;
         case '3' : (void)fprintf(stderr, "\n\n     Enter value [3] : ");
                    (void)scanf("%d", &value);
                    fsa[position].error_counter = value;
                    break;
         case '4' : (void)fprintf(stderr, "\n\n     Enter value [4] (0 - %d): ", fsa[position].allowed_transfers);
                    (void)scanf("%d", &value);
                    if (value <= fsa[position].allowed_transfers)
                    {
                       fsa[position].connections = value;
                    }
                    else
                    {
                       (void)printf("Wrong choice!\n");
                       break;
                    }
                    break;
         case '5' : (void)fprintf(stdout, "\033[2J\033[3;1H");
                    (void)fprintf(stdout, "\n\n\n");
                    (void)fprintf(stdout, "     Start/Stop queue..............(1)\n");
                    (void)fprintf(stdout, "     Start/Stop transfer...........(2)\n");
                    (void)fprintf(stdout, "     Start/Stop auto queue.........(3)\n");
                    (void)fprintf(stdout, "     Start/Stop danger queue.......(4)\n");
                    (void)fprintf(stdout, "     Start/Stop auto queue lock....(5)\n");
                    (void)fprintf(stderr, "     None..........................(6) ");
                    switch(get_key())
                    {
                       case '1' : fsa[position].host_status ^= PAUSE_QUEUE_STAT;
                                  break;
                       case '2' : fsa[position].host_status ^= STOP_TRANSFER_STAT;
                                  break;
                       case '3' : fsa[position].host_status ^= AUTO_PAUSE_QUEUE_STAT;
                                  break;
                       case '4' : fsa[position].host_status ^= DANGER_PAUSE_QUEUE_STAT;
                                  break;
                       case '5' : fsa[position].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
                                  break;
                       case '6' : break;
                       default  : (void)printf("Wrong choice!\n");
                                  (void)sleep(1);
                                  break;
                    }
                    break;
         case '6' : (void)fprintf(stderr, "\n\n     Enter value [6] : ");
                    (void)scanf("%d", &value);
                    fsa[position].max_errors = value;
                    break;
         case '7' : (void)fprintf(stderr, "\n\n     Enter value [7] : ");
                    (void)scanf("%d", &value);
                    fsa[position].block_size = value;
                    break;
         case '8' : (void)fprintf(stderr, "\n\n     Enter value [8] (1 - %d): ", MAX_NO_PARALLEL_JOBS);
                    (void)scanf("%d", &value);
                    if ((value > 0) && (value <= MAX_NO_PARALLEL_JOBS))
                    {
                       fsa[position].allowed_transfers = value;
                    }
                    else
                    {
                       (void)printf("Wrong choice!\n");
                       (void)sleep(1);
                    }
                    break;
         case '9' : (void)fprintf(stderr, "\n\n     Enter value [9] : ");
                    (void)scanf("%d", &value);
                    fsa[position].transfer_timeout = value;
                    break;
         case 'a' : (void)fprintf(stderr, "\n\n     Enter hostname  : ");
                    (void)scanf("%s", fsa[position].real_hostname[0]);
                    break;
         case 'b' : (void)fprintf(stderr, "\n\nEnter hostdisplayname: ");
                    (void)scanf("%s", fsa[position].host_dsp_name);
                    break;
         case 'c' : (void)fprintf(stderr, "\n\n     Enter value [c] : ");
                    (void)scanf("%d", &value);
                    if (value > MAX_NO_PARALLEL_JOBS)
                    {
                       (void)printf("The value must be between 0 and %d!\n", NO_BURST_COUNT_MASK);
                       (void)sleep(1);
                    }
                    else
                    {
                       fsa[position].special_flag = (fsa[position].special_flag & (~NO_BURST_COUNT_MASK)) | value;
                    }
                    break;
         case 'd' : (void)fprintf(stderr, "\n\n     Enter value [d] : ");
                    (void)scanf("%d", &value);
                    if (value > MAX_NO_PARALLEL_JOBS)
                    {
                       (void)printf("The value must be between 0 and %d!\n", MAX_NO_PARALLEL_JOBS);
                       (void)sleep(1);
                    }
                    else
                    {
                       fsa[position].active_transfers = value;
                    }
                    break;
         case 'e' : (void)fprintf(stderr, "\n\n     Enter value [d] : ");
                    file_name[0] = '\0';
                    (void)scanf("%s", file_name);
                    (void)strcpy(fsa[position].job_status[0].file_name_in_use, file_name);
                    break;
         case 'x' :
         case 'Q' :
         case 'q' : leave_flag = YES;
                    break;
         default  : (void)printf("Wrong choice!\n");
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
   (void)fprintf(stdout, "\033[2J\033[3;1H"); /* clear the screen (CLRSCR) */
   (void)fprintf(stdout, "\n\n                     FSA Editor (%s)\n\n", fsa[position].host_dsp_name);
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        | Key | Description      | current value  |\n");
   (void)fprintf(stdout, "        +-----+------------------+----------------+\n");
   (void)fprintf(stdout, "        |  1  |total_file_counter| %14u |\n", fsa[position].total_file_counter);
   (void)fprintf(stdout, "        |  2  |total_file_size   | %14lu |\n", fsa[position].total_file_size);
   (void)fprintf(stdout, "        |  3  |error counter     | %14d |\n", fsa[position].error_counter);
   (void)fprintf(stdout, "        |  4  |No. of connections| %14d |\n", fsa[position].connections);
   (void)fprintf(stdout, "        |  5  |host status       | %14d |\n", fsa[position].host_status);
   (void)fprintf(stdout, "        |  6  |Max. errors       | %14d |\n", fsa[position].max_errors);
   (void)fprintf(stdout, "        |  7  |Block size        | %14d |\n", fsa[position].block_size);
   (void)fprintf(stdout, "        |  8  |Allowed transfers | %14d |\n", fsa[position].allowed_transfers);
   (void)fprintf(stdout, "        |  9  |Transfer timeout  | %14ld |\n", fsa[position].transfer_timeout);
   (void)fprintf(stdout, "        |  a  |Real hostname     | %14s |\n", fsa[position].real_hostname[0]);
   (void)fprintf(stdout, "        |  b  |Host display name | %14s |\n", fsa[position].host_dsp_name);
   (void)fprintf(stdout, "        |  c  |No. of no bursts  | %14d |\n", (fsa[position].special_flag & NO_BURST_COUNT_MASK));
   (void)fprintf(stdout, "        |  d  |Active transfers  | %14d |\n", fsa[position].active_transfers);
   (void)fprintf(stdout, "        |  e  |File name         | %14s |\n", fsa[position].job_status[0].file_name_in_use);
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
      (void)fprintf(stderr, "ERROR   : signal() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (tcgetattr(STDIN_FILENO, &buf) < 0)
   {
      (void)fprintf(stderr, "ERROR   : tcgetattr() error : %s (%s %d)\n",
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
      (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (setjmp(env_alarm) != 0)
   {
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
      {
         (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      return(0);
   }
   alarm(5);
   if (read(STDIN_FILENO, &byte, 1) < 0)
   {
      (void)fprintf(stderr, "ERROR   : read() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   alarm(0);

   if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
   {
      (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
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
      (void)fprintf(stderr, "ERROR   : tcsetattr() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }
   exit(0);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] hostname|position\n",
                 progname);
   return;
}
