/*
 *  afdcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2005 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   afdcmd - send commands to the AFD
 **
 ** SYNOPSIS
 **   afdcmd [-w <working directory>] [-u[ <user>]] option hostname|position
 **                   -q      start queue
 **                   -Q      stop queue
 **                   -t      start transfer
 **                   -T      stop transfer
 **                   -b      enable directory
 **                   -B      disable directory
 **                   -e      enable host
 **                   -E      disable host
 **                   -s      switch host
 **                   -r      retry
 **                   -R      rescan directory
 **                   -d      enable/disable debug
 **                   -c      enable/disable trace
 **                   -C      enable/disable full trace
 **                   -f      start FD
 **                   -F      stop FD
 **                   -a      start AMG
 **                   -A      stop AMG
 **                   -W      toggle enable/disable directory
 **                   -X      toggle enable/disable host
 **                   -Y      start/stop AMG
 **                   -Z      start/stop FD
 **                   -v      Just print the version number.
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.09.1998 H.Kiehl Created
 **   27.02.2003 H.Kiehl When queue has been stopped automatically and we
 **                      wish to start it again error counter is set to zero.
 **   25.01.2005 H.Kiehl Don't start AUTO_PAUSE_QUEUE_STAT when starting
 **                      queue.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                       /* open()                       */
#include <time.h>                        /* time()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "permission.h"
#include "version.h"

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           no_of_dirs = 0,
                           no_of_host_names,
                           no_of_hosts = 0;
unsigned int               options = 0;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       **hosts = NULL,
                           *p_work_dir;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct afd_status          *p_afd_status;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes */
static void                eval_input(int, char **),
                           usage(char *);

#define START_QUEUE_OPTION       1
#define STOP_QUEUE_OPTION        2
#define START_TRANSFER_OPTION    4
#define STOP_TRANSFER_OPTION     8
#define ENABLE_DIRECTORY_OPTION  16
#define DISABLE_DIRECTORY_OPTION 32
#define ENABLE_HOST_OPTION       64
#define DISABLE_HOST_OPTION      128
#define SWITCH_OPTION            256
#define RETRY_OPTION             512
#define RESCAN_OPTION            1024
#define DEBUG_OPTION             2048
#define TRACE_OPTION             4096
#define FULL_TRACE_OPTION        8192
#define START_FD_OPTION          16384
#define STOP_FD_OPTION           32768
#define START_AMG_OPTION         65536
#define STOP_AMG_OPTION          131072
#define START_STOP_AMG_OPTION    262144
#define START_STOP_FD_OPTION     524288
#define TOGGLE_DIRECTORY_OPTION  1048576
#define TOGGLE_HOST_OPTION       2097152


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdcmd() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int              change_host_config = NO,
                    ehc = YES, /* Error flag for eval_host_config() */
                    errors = 0,
                    hosts_found,
                    i,
                    position;
   char             fake_user[MAX_FULL_USER_ID_LENGTH],
                    host_config_file[MAX_PATH_LENGTH],
                    *perm_buffer,
                    *ptr,
                    user[MAX_FULL_USER_ID_LENGTH],
                    sys_log_fifo[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct host_list *hl = NULL;

   CHECK_FOR_VERSION(argc, argv);

   if ((argc > 1) &&
       (argv[1][0] == '-') && (argv[1][1] == 'v') && (argv[1][2] == '\0'))
   {
      (void)fprintf(stdout, "%s\n", PACKAGE_VERSION);
      exit(SUCCESS);
   }

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   if (argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   eval_input(argc, argv);
   get_user(user, fake_user);

   /*
    * Ensure that the user may use this program.
    */
   switch(get_permissions(&perm_buffer, fake_user))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
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
               if (posi(perm_buffer, AFD_CMD_PERM) != NULL)
               {
                  permission = YES;

                  /*
                   * Check if the user may do everything
                   * he has requested. If not remove it
                   * from the option list.
                   */
                  if ((options & START_QUEUE_OPTION) ||
                      (options & STOP_QUEUE_OPTION))
                  {
                     if (posi(perm_buffer, CTRL_QUEUE_PERM) == NULL)
                     {
                        if (options & START_QUEUE_OPTION)
                        {
                           options ^= START_QUEUE_OPTION;
                        }
                        if (options & STOP_QUEUE_OPTION)
                        {
                           options ^= STOP_QUEUE_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the queue.\n",
                                      user);
                     }
                  }
                  if ((options & START_TRANSFER_OPTION) ||
                      (options & STOP_TRANSFER_OPTION))
                  {
                     if (posi(perm_buffer, CTRL_TRANSFER_PERM) == NULL)
                     {
                        if (options & START_TRANSFER_OPTION)
                        {
                           options ^= START_TRANSFER_OPTION;
                        }
                        if (options & STOP_TRANSFER_OPTION)
                        {
                           options ^= STOP_TRANSFER_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the transfer.\n",
                                      user);
                     }
                  }
                  if ((options & ENABLE_DIRECTORY_OPTION) ||
                      (options & DISABLE_DIRECTORY_OPTION) ||
                      (options & TOGGLE_DIRECTORY_OPTION))
                  {
                     if (posi(perm_buffer, DISABLE_DIR_PERM) == NULL)
                     {
                        if (options & ENABLE_DIRECTORY_OPTION)
                        {
                           options ^= ENABLE_DIRECTORY_OPTION;
                        }
                        if (options & DISABLE_DIRECTORY_OPTION)
                        {
                           options ^= DISABLE_DIRECTORY_OPTION;
                        }
                        if (options & TOGGLE_DIRECTORY_OPTION)
                        {
                           options ^= TOGGLE_DIRECTORY_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable a directory.\n",
                                      user);
                     }
                  }
                  if ((options & ENABLE_HOST_OPTION) ||
                      (options & DISABLE_HOST_OPTION) ||
                      (options & TOGGLE_HOST_OPTION))
                  {
                     if (posi(perm_buffer, DISABLE_HOST_PERM) == NULL)
                     {
                        if (options & ENABLE_HOST_OPTION)
                        {
                           options ^= ENABLE_HOST_OPTION;
                        }
                        if (options & DISABLE_HOST_OPTION)
                        {
                           options ^= DISABLE_HOST_OPTION;
                        }
                        if (options & TOGGLE_HOST_OPTION)
                        {
                           options ^= TOGGLE_HOST_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable a host.\n",
                                      user);
                     }
                  }
                  if (options & SWITCH_OPTION)
                  {
                     if (posi(perm_buffer, SWITCH_HOST_PERM) == NULL)
                     {
                        options ^= SWITCH_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to switch hosts.\n",
                                      user);
                     }
                  }
                  if (options & RETRY_OPTION)
                  {
                     if (posi(perm_buffer, RETRY_PERM) == NULL)
                     {
                        options ^= RETRY_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to retry.\n",
                                      user);
                     }
                  }
                  if (options & RESCAN_OPTION)
                  {
                     if (posi(perm_buffer, RESCAN_PERM) == NULL)
                     {
                        options ^= RESCAN_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to rerscan a directory.\n",
                                      user);
                     }
                  }
                  if (options & DEBUG_OPTION)
                  {
                     if (posi(perm_buffer, DEBUG_PERM) == NULL)
                     {
                        options ^= DEBUG_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable debugging for a host.\n",
                                      user);
                     }
                  }
                  if (options & TRACE_OPTION)
                  {
                     if (posi(perm_buffer, TRACE_PERM) == NULL)
                     {
                        options ^= TRACE_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable tracing for a host.\n",
                                      user);
                     }
                  }
                  if (options & FULL_TRACE_OPTION)
                  {
                     if (posi(perm_buffer, FULL_TRACE_PERM) == NULL)
                     {
                        options ^= FULL_TRACE_OPTION;
                        (void)fprintf(stderr,
                                      "User %s not permitted to enable/disable full tracing for a host.\n",
                                      user);
                     }
                  }
                  if ((options & START_FD_OPTION) ||
                      (options & STOP_FD_OPTION))
                  {
                     if (posi(perm_buffer, FD_CTRL_PERM) == NULL)
                     {
                        if (options & START_FD_OPTION)
                        {
                           options ^= START_FD_OPTION;
                        }
                        if (options & STOP_FD_OPTION)
                        {
                           options ^= STOP_FD_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the FD.\n",
                                      user);
                     }
                  }
                  if ((options & START_AMG_OPTION) ||
                      (options & STOP_AMG_OPTION))
                  {
                     if (posi(perm_buffer, AMG_CTRL_PERM) == NULL)
                     {
                        if (options & START_AMG_OPTION)
                        {
                           options ^= START_AMG_OPTION;
                        }
                        if (options & STOP_AMG_OPTION)
                        {
                           options ^= STOP_AMG_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the AMG.\n",
                                      user);
                     }
                  }
                  if (options & START_STOP_AMG_OPTION)
                  {
                     if (posi(perm_buffer, AMG_CTRL_PERM) == NULL)
                     {
                        if (options & START_STOP_AMG_OPTION)
                        {
                           options ^= START_STOP_AMG_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the AMG.\n",
                                      user);
                     }
                  }
                  if (options & START_STOP_FD_OPTION)
                  {
                     if (posi(perm_buffer, FD_CTRL_PERM) == NULL)
                     {
                        if (options & START_STOP_FD_OPTION)
                        {
                           options ^= START_STOP_FD_OPTION;
                        }
                        (void)fprintf(stderr,
                                      "User %s not permitted to start/stop the FD.\n",
                                      user);
                     }
                  }
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

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* If the process AFD has not yet created the */
   /* system log fifo create it now.             */
   (void)sprintf(sys_log_fifo, "%s%s%s",
                 p_work_dir, FIFO_DIR, SYSTEM_LOG_FIFO);
   if ((stat(sys_log_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Could not create fifo %s. (%s %d)\n",
                       sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Open system log fifo */
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((options & RESCAN_OPTION) ||
       (options & ENABLE_DIRECTORY_OPTION) ||
       (options & DISABLE_DIRECTORY_OPTION) ||
       (options & TOGGLE_DIRECTORY_OPTION))
   {
      int    send_msg = NO;
      time_t current_time;

      if (fra_attach() < 0)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to attach to FRA. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }
      current_time = time(NULL);

      for (i = 0; i < no_of_host_names; i++)
      {
         position = -1;
         ptr = hosts[i];
         while ((*ptr != '\0') && (isdigit((int)(*ptr))))
         {
            ptr++;
         }
         if ((*ptr == '\0') && (ptr != hosts[i]))
         {
            position = atoi(hosts[i]);
            if ((position < 0) || (position > (no_of_dirs - 1)))
            {
               (void)fprintf(stderr,
                             "WARNING : Position %d out of range. Ignoring. (%s %d)\n",
                             position, __FILE__, __LINE__);
               errors++;
               continue;
            }
         }
         if (position < 0)
         {
            if ((position = get_dir_position(fra, hosts[i], no_of_dirs)) < 0)
            {
               (void)fprintf(stderr,
                             "WARNING : Could not find directory %s in FRA. (%s %d)\n",
                             hosts[i], __FILE__, __LINE__);
               errors++;
               continue;
            }
         }

         /*
          * RESCAN DIRECTORY
          */
         if (options & RESCAN_OPTION)
         {
            if ((fra[position].time_option != NO) &&
                (fra[position].next_check_time > current_time))
            {
               if (fra[position].host_alias[0] != '\0')
               {
                  send_msg = YES;
               }
               fra[position].next_check_time = current_time;
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: FORCED rescan (%s) [afdcmd].\n",
                         MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias, user);
            }
         }

         /*
          * ENABLE DIRECTORY
          */
         if (options & ENABLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: ENABLED directory (%s) [afdcmd].\n",
                         MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                         user);
               fra[position].dir_status = NORMAL_STATUS;
               fra[position].dir_flag ^= DIR_DISABLED;
            }
            else
            {
               (void)fprintf(stderr,
                             "INFO    : Directory %s is already enabled.\n",
                             fra[position].dir_alias);
            }
         }

         /*
          * DISABLE HOST
          */
         if (options & DISABLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               (void)fprintf(stderr,
                             "INFO    : Directory %s is already disabled.\n",
                             fra[position].dir_alias);
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: DISABLED directory (%s) [afdcmd].\n",
                         MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                         user);
               fra[position].dir_status = DISABLED;
               fra[position].dir_flag ^= DIR_DISABLED;
            }
         }

         /*
          * ENABLE or DISABLE a directory.
          */
         if (options & TOGGLE_DIRECTORY_OPTION)
         {
            if (fra[position].dir_flag & DIR_DISABLED)
            {
               /*
                * ENABLE DIRECTORY
                */
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: ENABLED directory (%s) [afdcmd].\n",
                         MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                         user);
               fra[position].dir_flag ^= DIR_DISABLED;
               fra[position].dir_status = NORMAL_STATUS;
            }
            else /* DISABLE DIRECTORY */
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: DISABLED directory (%s) [afdcmd].\n",
                         MAX_DIR_ALIAS_LENGTH, fra[position].dir_alias,
                         user);
               fra[position].dir_flag ^= DIR_DISABLED;
               fra[position].dir_status = DISABLED;
            }
         }
      }

      if (send_msg == YES)
      {
         int  fd_cmd_fd;
         char fd_cmd_fifo[MAX_PATH_LENGTH];

         (void)sprintf(fd_cmd_fifo, "%s%s%s",
                       p_work_dir, FIFO_DIR, FD_CMD_FIFO);
         if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
         {
            (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                          fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            if (send_cmd(FORCE_REMOTE_DIR_CHECK, fd_cmd_fd) != SUCCESS)
            {
               (void)fprintf(stderr, "write() error : %s\n", strerror(errno));
            }
            if (close(fd_cmd_fd) == -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "close() error : %s (%s %d)\n",
                         strerror(errno), __FILE__, __LINE__);
            }
         }
      }

      (void)fra_detach();
   }
   else
   {
      if (fsa_attach() < 0)
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
      }

      if ((options & START_QUEUE_OPTION) || (options & STOP_QUEUE_OPTION) ||
          (options & START_TRANSFER_OPTION) ||
          (options & STOP_TRANSFER_OPTION) ||
          (options & DISABLE_HOST_OPTION) || (options & ENABLE_HOST_OPTION) ||
          (options & TOGGLE_HOST_OPTION) || (options & SWITCH_OPTION))
      {
         (void)sprintf(host_config_file, "%s%s%s",
                       p_work_dir, ETC_DIR, DEFAULT_HOST_CONFIG_FILE);
         ehc = eval_host_config(&hosts_found, host_config_file, &hl, NO);
         if ((ehc == NO) && (no_of_hosts != hosts_found))
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Hosts found in HOST_CONFIG (%d) and those currently storred (%d) are not the same. Unable to do any changes. (%s %d)\n",
                      no_of_hosts, hosts_found, __FILE__, __LINE__);
            ehc = YES;
         }
         else if (ehc == YES)
              {
                 (void)rec(sys_log_fd, WARN_SIGN,
                           "Unable to retrieve data from HOST_CONFIG, therefore no values changed in it! (%s %d)\n",
                           __FILE__, __LINE__);
              }
      }

      for (i = 0; i < no_of_host_names; i++)
      {
         position = -1;
         ptr = hosts[i];
         while ((*ptr != '\0') && (isdigit((int)(*ptr))))
         {
            ptr++;
         }
         if ((*ptr == '\0') && (ptr != hosts[i]))
         {
            position = atoi(hosts[i]);
            if ((position < 0) || (position > (no_of_hosts - 1)))
            {
               (void)fprintf(stderr,
                             "WARNING : Position %d out of range. Ignoring. (%s %d)\n",
                             position, __FILE__, __LINE__);
               errors++;
               continue;
            }
         }
         if (position < 0)
         {
            if ((position = get_host_position(fsa, hosts[i], no_of_hosts)) < 0)
            {
               (void)fprintf(stderr,
                             "WARNING : Could not find host %s in FSA. (%s %d)\n",
                             hosts[i], __FILE__, __LINE__);
               errors++;
               continue;
            }
         }

         /*
          * START OUEUE
          */
         if (ehc == NO)
         {
            if (options & START_QUEUE_OPTION)
            {
               if (fsa[position].host_status & PAUSE_QUEUE_STAT)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: STARTED queue (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].host_status ^= PAUSE_QUEUE_STAT;
                  hl[position].host_status &= ~PAUSE_QUEUE_STAT;
                  change_host_config = YES;
               }
               else
               {
                  (void)fprintf(stderr,
                                "INFO    : Queue for host %s is already started.\n",
                                fsa[position].host_dsp_name);
               }
            }

            /*
             * STOP OUEUE
             */
            if (options & STOP_QUEUE_OPTION)
            {
               if (fsa[position].host_status & PAUSE_QUEUE_STAT)
               {
                  (void)fprintf(stderr,
                                "INFO    : Queue for host %s is already stopped.\n",
                                fsa[position].host_dsp_name);
               }
               else
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: STOPPED queue (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].host_status ^= PAUSE_QUEUE_STAT;
                  hl[position].host_status |= PAUSE_QUEUE_STAT;
                  change_host_config = YES;
               }
            }

            /*
             * START TRANSFER
             */
            if (options & START_TRANSFER_OPTION)
            {
               if (fsa[position].host_status & STOP_TRANSFER_STAT)
               {
                  int  fd;
                  char wake_up_fifo[MAX_PATH_LENGTH];

                  (void)sprintf(wake_up_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
                  if ((fd = open(wake_up_fifo, O_RDWR)) == -1)
                  {
                     (void)fprintf(stderr,
                                   "WARNING : Failed to open() %s : %s (%s %d)\n",
                                   FD_WAKE_UP_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     if (write(fd, "", 1) != 1)
                     {
                        (void)fprintf(stderr,
                                      "WARNING : Failed to write() to %s : %s (%s %d)\n",
                                      FD_WAKE_UP_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                               "Failed to close() FIFO %s : %s (%s %d)\n",
                               FD_WAKE_UP_FIFO, strerror(errno),
                               __FILE__, __LINE__);
                     }
                  }
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: STARTED transfer (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].host_status ^= STOP_TRANSFER_STAT;
                  hl[position].host_status &= ~STOP_TRANSFER_STAT;
                  change_host_config = YES;
               }
               else
               {
                  (void)fprintf(stderr,
                                "INFO    : Transfer for host %s is already started.\n",
                                fsa[position].host_dsp_name);
               }
            }

            /*
             * STOP TRANSFER
             */
            if (options & STOP_TRANSFER_OPTION)
            {
               if (fsa[position].host_status & STOP_TRANSFER_STAT)
               {
                  (void)fprintf(stderr,
                                "INFO    : Transfer for host %s is already stopped.\n",
                                fsa[position].host_dsp_name);
               }
               else
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: STOPPED transfer (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].host_status ^= STOP_TRANSFER_STAT;
                  hl[position].host_status |= STOP_TRANSFER_STAT;
                  change_host_config = YES;
               }
            }

            /*
             * ENABLE HOST
             */
            if (options & ENABLE_HOST_OPTION)
            {
               if (fsa[position].special_flag & HOST_DISABLED)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: ENABLED (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].special_flag ^= HOST_DISABLED;
                  hl[position].host_status &= ~HOST_CONFIG_HOST_DISABLED;
                  change_host_config = YES;
               }
               else
               {
                  (void)fprintf(stderr,
                                "INFO    : Host %s is already enabled.\n",
                                fsa[position].host_dsp_name);
               }
            }

            /*
             * DISABLE HOST
             */
            if (options & DISABLE_HOST_OPTION)
            {
               if (fsa[position].special_flag & HOST_DISABLED)
               {
                  (void)fprintf(stderr,
                                "INFO    : Host %s is already disabled.\n",
                                fsa[position].host_dsp_name);
               }
               else
               {
                  int    fd;
                  size_t length;
                  char   delete_jobs_host_fifo[MAX_PATH_LENGTH];

                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: DISABLED (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].special_flag ^= HOST_DISABLED;
                  hl[position].host_status |= HOST_CONFIG_HOST_DISABLED;
                  change_host_config = YES;
                  length = strlen(fsa[position].host_alias) + 1;

                  (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
                  if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                  {
                     (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     char wbuf[MAX_HOSTNAME_LENGTH + 2];

                     wbuf[0] = DELETE_ALL_JOBS_FROM_HOST;
                     (void)strcpy(&wbuf[1], fsa[position].host_alias);
                     if (write(fd, wbuf, (length + 1)) != (length + 1))
                     {
                        (void)fprintf(stderr,
                                      "Failed to write() to %s : %s (%s %d)\n",
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  FD_DELETE_FIFO,
                                  strerror(errno), __FILE__, __LINE__);
                     }
                  }
                  (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, DEL_TIME_JOB_FIFO);
                  if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                  {
                     (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                   DEL_TIME_JOB_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     if (write(fd, fsa[position].host_alias, length) != length)
                     {
                        (void)fprintf(stderr,
                                      "Failed to write() to %s : %s (%s %d)\n",
                                      DEL_TIME_JOB_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  DEL_TIME_JOB_FIFO, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
            }

            /*
             * ENABLE or DISABLE a host.
             */
            if (options & TOGGLE_HOST_OPTION)
            {
               if (fsa[position].special_flag & HOST_DISABLED)
               {
                  /*
                   * ENABLE HOST
                   */
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: ENABLED (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].special_flag ^= HOST_DISABLED;
                  hl[position].host_status &= ~HOST_CONFIG_HOST_DISABLED;
               }
               else /* DISABLE HOST */
               {
                  int    fd;
                  size_t length;
                  char   delete_jobs_host_fifo[MAX_PATH_LENGTH];

                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "%-*s: DISABLED (%s) [afdcmd].\n",
                            MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name,
                            user);
                  fsa[position].special_flag ^= HOST_DISABLED;
                  hl[position].host_status |= HOST_CONFIG_HOST_DISABLED;
                  length = strlen(fsa[position].host_alias) + 1;

                  (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
                  if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                  {
                     (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                   FD_DELETE_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     char wbuf[MAX_HOSTNAME_LENGTH + 2];

                     wbuf[0] = DELETE_ALL_JOBS_FROM_HOST;
                     (void)strcpy(&wbuf[1], fsa[position].host_alias);
                     if (write(fd, wbuf, (length + 1)) != (length + 1))
                     {
                        (void)fprintf(stderr,
                                      "Failed to write() to %s : %s (%s %d)\n",
                                      FD_DELETE_FIFO,
                                      strerror(errno), __FILE__, __LINE__);
                         errors++;
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  FD_DELETE_FIFO,
                                  strerror(errno), __FILE__, __LINE__);
                     }
                  }
                  (void)sprintf(delete_jobs_host_fifo, "%s%s%s",
                                p_work_dir, FIFO_DIR, DEL_TIME_JOB_FIFO);
                  if ((fd = open(delete_jobs_host_fifo, O_RDWR)) == -1)
                  {
                     (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                   DEL_TIME_JOB_FIFO, strerror(errno),
                                   __FILE__, __LINE__);
                     errors++;
                  }
                  else
                  {
                     if (write(fd, fsa[position].host_alias, length) != length)
                     {
                        (void)fprintf(stderr,
                                      "Failed to write() to %s : %s (%s %d)\n",
                                      DEL_TIME_JOB_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        errors++;
                     }
                     if (close(fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "Failed to close() FIFO %s : %s (%s %d)\n",
                                  DEL_TIME_JOB_FIFO, strerror(errno),
                                  __FILE__, __LINE__);
                     }
                  }
               }
               change_host_config = YES;
            }
         }

         if (options & SWITCH_OPTION)
         {
            if ((fsa[position].toggle_pos > 0) &&
                (fsa[position].host_toggle_str[0] != '\0'))
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Host Switch initiated for host %s (%s) [afdcmd]\n",
                         fsa[position].host_dsp_name, user);
               if (fsa[position].host_toggle == HOST_ONE)
               {
                  fsa[position].host_toggle = HOST_TWO;
                  hl[position].host_status |= HOST_TWO_FLAG;
               }
               else
               {
                  fsa[position].host_toggle = HOST_ONE;
                  hl[position].host_status &= ~HOST_TWO_FLAG;
               }
               change_host_config = YES;
               fsa[position].host_dsp_name[(int)fsa[position].toggle_pos] = fsa[position].host_toggle_str[(int)fsa[position].host_toggle];
            }
            else
            {
               (void)fprintf(stderr,
                             "WARNING : Host %s cannot be switched!",
                             fsa[position].host_dsp_name);
            }
         }

         if (options & RETRY_OPTION)
         {
            int  fd;
            char retry_fifo[MAX_PATH_LENGTH];

            (void)sprintf(retry_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, RETRY_FD_FIFO);
            if ((fd = open(retry_fifo, O_RDWR)) == -1)
            {
               (void)fprintf(stderr,
                             "WARNING : Failed to open() %s : %s (%s %d)\n",
                             RETRY_FD_FIFO, strerror(errno),
                             __FILE__, __LINE__);
               errors++;
            }
            else
            {
               if (write(fd, &position, sizeof(int)) != sizeof(int))
               {
                  (void)fprintf(stderr,
                                "WARNING : Failed to write() to %s : %s (%s %d)\n",
                                RETRY_FD_FIFO, strerror(errno),
                                __FILE__, __LINE__);
                  errors++;
               }
               if (close(fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Failed to close() FIFO %s : %s (%s %d)\n"
                            RETRY_FD_FIFO, strerror(errno),
                            __FILE__, __LINE__);
               }
            }
         }

         if (options & DEBUG_OPTION)
         {
            if (fsa[position].debug == NORMAL_MODE)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Enabled DEBUG mode by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = DEBUG_MODE;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Disabled DEBUG mode by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = NORMAL_MODE;
            }
         }

         if (options & TRACE_OPTION)
         {
            if (fsa[position].debug == NORMAL_MODE)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Enabled TRACE mode by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = TRACE_MODE;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Disabled TRACE mode by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = NORMAL_MODE;
            }
         }

         if (options & FULL_TRACE_OPTION)
         {
            if (fsa[position].debug == NORMAL_MODE)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Enabled FULL TRACE MODE by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = FULL_TRACE_MODE;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "%-*s: Disabled FULL TRACE mode by user %s [afdcmd].\n",
                         MAX_HOSTNAME_LENGTH, fsa[position].host_dsp_name, user);
               fsa[position].debug = NORMAL_MODE;
            }
         }
      } /* for (i = 0; i < no_of_host_names; i++) */

      (void)fsa_detach(YES);

      if (((options & START_QUEUE_OPTION) || (options & STOP_QUEUE_OPTION) ||
           (options & START_TRANSFER_OPTION) ||
           (options & STOP_TRANSFER_OPTION) ||
           (options & DISABLE_HOST_OPTION) || (options & ENABLE_HOST_OPTION) ||
           (options & TOGGLE_HOST_OPTION) || (options & SWITCH_OPTION)) &&
           (ehc == NO) && (change_host_config == YES))
      {
         (void)write_host_config(no_of_hosts, host_config_file, hl);
         if (hl != NULL)
         {
            free(hl);
         }
      }
   }

   if ((options & START_FD_OPTION) || (options & STOP_FD_OPTION) ||
       (options & START_AMG_OPTION) || (options & STOP_AMG_OPTION) ||
       (options & START_STOP_AMG_OPTION) || (options & START_STOP_FD_OPTION))
   {
      int  afd_cmd_fd;
      char afd_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(afd_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, AFD_CMD_FIFO);
      if ((afd_cmd_fd = open(afd_cmd_fifo, O_RDWR)) < 0)
      {
         (void)fprintf(stderr, "Could not open fifo %s : %s (%s %d)\n",
                       afd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (attach_afd_status(NULL) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to attach to AFD status area. (%s %d)\n",
                          __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (options & START_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               (void)fprintf(stderr, "%s is running. (%s %d)\n",
                             FD, __FILE__, __LINE__);
            }
            else
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending START to %s by %s [afdcmd]\n", FD, user);
               if (send_cmd(START_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to start %s : %s (%s %d)\n",
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & STOP_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending STOP to %s by %s [afdcmd]\n", FD, user);
               if (send_cmd(STOP_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to stop %s : %s (%s %d)\n",
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)fprintf(stderr, "%s is already stopped. (%s %d)\n",
                             FD, __FILE__, __LINE__);
            }
         }

         if (options & START_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               (void)fprintf(stderr, "%s is already running. (%s %d)\n",
                             AMG, __FILE__, __LINE__);
            }
            else
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending START to %s by %s [afdcmd]\n", AMG, user);
               if (send_cmd(START_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to start %s : %s (%s %d)\n",
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & STOP_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending STOP to %s by %s [afdcmd]\n", AMG, user);
               if (send_cmd(STOP_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to stop %s : %s (%s %d)\n",
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)fprintf(stderr, "%s is already stopped. (%s %d)\n",
                             AMG, __FILE__, __LINE__);
            }
         }

         if (options & START_STOP_AMG_OPTION)
         {
            if (p_afd_status->amg == ON)
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending STOP to %s by %s [afdcmd]\n", AMG, user);
               if (send_cmd(STOP_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to stop %s : %s (%s %d)\n",
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending START to %s by %s [afdcmd]\n", AMG, user);
               if (send_cmd(START_AMG, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to start %s : %s (%s %d)\n",
                                AMG, strerror(errno), __FILE__, __LINE__);
               }
            }
         }

         if (options & START_STOP_FD_OPTION)
         {
            if (p_afd_status->fd == ON)
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending STOP to %s by %s [afdcmd]\n", FD, user);
               if (send_cmd(STOP_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to stop %s : %s (%s %d)\n",
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)rec(sys_log_fd, CONFIG_SIGN,
                         "Sending START to %s by %s [afdcmd]\n", FD, user);
               if (send_cmd(START_FD, afd_cmd_fd) != SUCCESS)
               {
                  (void)fprintf(stderr,
                                "Was not able to start %s : %s (%s %d)\n",
                                FD, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         if (close(afd_cmd_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   exit(errors);
}


/*++++++++++++++++++++++++++++ eval_input() +++++++++++++++++++++++++++++*/
static void
eval_input(int argc, char *argv[])
{
   int  correct = YES,       /* Was input/syntax correct?      */
        need_hostname = NO;
   char progname[128];

   (void)strcpy(progname, argv[0]);

   /**********************************************************/
   /* The following while loop checks for the parameters:    */
   /*                                                        */
   /*         -q : start queue                               */
   /*         -Q : stop queue                                */
   /*         -t : start transfer                            */
   /*         -T : stop transfer                             */
   /*         -b : enable directory                          */
   /*         -B : disable directory                         */
   /*         -e : enable host                               */
   /*         -E : disable host                              */
   /*         -s : switch host                               */
   /*         -r : retry                                     */
   /*         -R : rescan directory                          */
   /*         -d : enable/disable debug                      */
   /*         -c : enable/disable trace                      */
   /*         -C : enable/disable fulle trace                */
   /*         -f : start FD                                  */
   /*         -F : stop FD                                   */
   /*         -a : start AMG                                 */
   /*         -A : stop AMG                                  */
   /*         -W : toggle enable/disable directory           */
   /*         -X : toggle enable/disable host                */
   /*         -Y : start/stop AMG                            */
   /*         -Z : start/stop FD                             */
   /*                                                        */
   /**********************************************************/
   while ((--argc > 0) && ((*++argv)[0] == '-'))
   {
      if (*(argv[0] + 2) == '\0')
      {
         switch(*(argv[0] + 1))
         {
            case 'q': /* start queue */
               options ^= START_QUEUE_OPTION;
               need_hostname = YES;
               break;

            case 'Q': /* stop queue */
               options ^= STOP_QUEUE_OPTION;
               need_hostname = YES;
               break;

            case 't': /* start transfer */
               options ^= START_TRANSFER_OPTION;
               need_hostname = YES;
               break;

            case 'T': /* stop transfer */
               options ^= STOP_TRANSFER_OPTION;
               need_hostname = YES;
               break;

            case 'b': /* enable directory */
               options ^= ENABLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'B': /* disable directory */
               options ^= DISABLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'e': /* enable host */
               options ^= ENABLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 'E': /* disable host */
               options ^= DISABLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 's': /* switch host */
               options ^= SWITCH_OPTION;
               need_hostname = YES;
               break;

            case 'r': /* Retry */
               options ^= RETRY_OPTION;
               need_hostname = YES;
               break;

            case 'R': /* Reread */
               options ^= RESCAN_OPTION;
               need_hostname = YES;
               break;

            case 'd': /* Debug */
               options ^= DEBUG_OPTION;
               need_hostname = YES;
               break;

            case 'c': /* Trace */
               options ^= TRACE_OPTION;
               need_hostname = YES;
               break;

            case 'C': /* Full Trace */
               options ^= FULL_TRACE_OPTION;
               need_hostname = YES;
               break;

            case 'f': /* Start FD */
               options ^= START_FD_OPTION;
               break;

            case 'F': /* Stop FD */
               options ^= STOP_FD_OPTION;
               break;

            case 'a': /* Start AMG */
               options ^= START_AMG_OPTION;
               break;

            case 'A': /* Stop AMG */
               options ^= STOP_AMG_OPTION;
               break;

            case 'W': /* Toggle enable/disable directory */
               options ^= TOGGLE_DIRECTORY_OPTION;
               need_hostname = YES;
               break;

            case 'X': /* Toggle enable/disable host */
               options ^= TOGGLE_HOST_OPTION;
               need_hostname = YES;
               break;

            case 'Y': /* Start/Stop AMG */
               options ^= START_STOP_AMG_OPTION;
               break;

            case 'Z': /* Start/Stop FD */
               options ^= START_STOP_FD_OPTION;
               break;

            default : /* Unknown parameter */

               (void)fprintf(stderr, "ERROR  : Unknown parameter %c. (%s %d)\n",
                            *(argv[0] + 1), __FILE__, __LINE__);
               correct = NO;
               break;

         } /* switch(*(argv[0] + 1)) */
      }
      else
      {
         (void)fprintf(stderr, "ERROR  : Unknown option %s. (%s %d)\n",
                      argv[0], __FILE__, __LINE__);
         correct = NO;
      }
   }

   if (correct != NO)
   {
      no_of_host_names = argc;
      if (no_of_host_names > 0)
      {
         int i = 0;

         RT_ARRAY(hosts, no_of_host_names, (MAX_HOSTNAME_LENGTH + 1), char);
         while (argc > 0)
         {
            (void)strcpy(hosts[i], argv[0]);
            argc--; argv++;
            i++;
         }
      }
      else
      {
         if (need_hostname == YES)
         {
            (void)fprintf(stderr, "ERROR   : No host names specified!\n");
            correct = NO;
         }
      }
   }

   /* If input is not correct show syntax */
   if (correct == NO)
   {
      usage(progname);
      exit(1);
   }

   return;
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{                    
   (void)fprintf(stderr,
                 "SYNTAX  : %s [-w working directory] [-u[ <user>]] options hostname|directory|position\n",
                 progname);
   (void)fprintf(stderr,
                 "    FSA options:\n");
   (void)fprintf(stderr,
                 "               -q      start queue\n");
   (void)fprintf(stderr,
                 "               -Q      stop queue\n");
   (void)fprintf(stderr,
                 "               -t      start transfer\n");
   (void)fprintf(stderr,
                 "               -T      stop transfer\n");
   (void)fprintf(stderr,
                 "               -e      enable host\n");
   (void)fprintf(stderr,
                 "               -E      disable host\n");
   (void)fprintf(stderr,
                 "               -s      switch host\n");
   (void)fprintf(stderr,
                 "               -r      retry\n");
   (void)fprintf(stderr,
                 "               -d      enable/disable debug\n");
   (void)fprintf(stderr,
                 "               -c      enable/disable trace\n");
   (void)fprintf(stderr,
                 "               -C      enable/disable full trace\n");
   (void)fprintf(stderr,
                 "               -X      toggle enable/disable host\n");
   (void)fprintf(stderr,
                 "    FRA options:\n");
   (void)fprintf(stderr,
                 "               -b      enable directory\n");
   (void)fprintf(stderr,
                 "               -B      disable directory\n");
   (void)fprintf(stderr,
                 "               -R      rescan directory\n");
   (void)fprintf(stderr,
                 "               -W      toggle enable/disable directory\n");
   (void)fprintf(stderr,
                 "General options:\n");
   (void)fprintf(stderr,
                 "               -f      start FD\n");
   (void)fprintf(stderr,
                 "               -F      stop FD\n");
   (void)fprintf(stderr,
                 "               -a      start AMG\n");
   (void)fprintf(stderr,
                 "               -A      stop AMG\n");
   (void)fprintf(stderr,
                 "               -Y      start/stop AMG\n");
   (void)fprintf(stderr,
                 "               -Z      start/stop FD\n");
   (void)fprintf(stderr,
                 "               -v      just print Version\n");
   return;
}
