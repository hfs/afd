/*
 *  afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Deutscher Wetterdienst (DWD),
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
 **   afd - controls startup and shutdown of AFD
 **
 ** SYNOPSIS
 **   afd [options]
 **          -a            only start AFD
 **          -b            Blocks auto restart of AFD
 **          -c            only check if AFD is active
 **          -C            check if AFD is active, if not start it
 **          -d            only start afd_ctrl dialog
 **          -h            only check for heartbeat
 **          -H            check if heartbeat is active, if not start AFD
 **          -i            initialize AFD, by deleting fifodir
 **          -I            initialize AFD, by deleting everything except for
 **                        the etc directory
 **          -p <role>     use the given user role
 **          -r            Removes blocking file
 **          -s            shutdown AFD
 **          -S            silent AFD shutdown
 **          -u[ <user>]   different user
 **          -w <work dir> AFD working directory
 **          -v            Just print the version number.
 **          --version     Current version
 **          -z            Set shared shutdown bit.
 **
 ** DESCRIPTION
 **   This program controls the startup or shutdown procedure of
 **   the AFD. When starting the following process are being
 **   initiated in this order:
 **
 **          init_afd        - Monitors all process of the AFD.
 **          system_log      - Logs all system activities.
 **          transfer_log    - Logs all transfer activities.
 **          trans_db_log    - Logs all debug transfer activities.
 **          receive_log     - Logs all receive activities.
 **          archive_watch   - Searches archive for old files and
 **                            removes them.
 **          input_log       - Logs all activities on input.
 **          output_log      - Logs activities on output (can be turned
 **                            on/off on a per job basis).
 **          delete_log      - Logs all files that are being removed
 **                            by the AFD.
 **          rename_log      - Logs all files being renamed.
 **          afd_stat        - Collects statistic information.
 **          amg             - Searches user directories and generates
 **                            messages for the FD.
 **          fd              - Reads messages from the AMG and distributes
 **                            the corresponding files.
 **          
 **
 ** RETURN VALUES
 **   None when successful. Otherwise if no response is received
 **   from the AFD, it will tell the user.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.04.1996 H.Kiehl Created
 **   30.05.1997 H.Kiehl Added permission check.
 **   13.08.1997 H.Kiehl When doing a shutdown, waiting for init_afd to
 **                      terminate.
 **   10.05.1998 H.Kiehl Added -c and -C options.
 **   16.06.2002 H.Kiehl Added -h and -H options.
 **   01.05.2004 H.Kiehl Added options to initialize AFD.
 **   23.04.2005 H.Kiehl Added -z option.
 **   07.04.2006 H.Kiehl When doing initialization only delete AFD files,
 **                      not AFD_MON files.
 **   10.01.2007 H.Kiehl Do not return -1 when doing a shutdown and
 **                      AFD is not active.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), remove(), NULL               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* mmap(), munmap()                        */
#endif
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval, FD_SET...               */
#include <signal.h>           /* kill()                                  */
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#include <unistd.h>           /* execlp(), chdir(), R_OK, X_OK           */
#include <errno.h>
#include "version.h"
#include "permission.h"
#include "logdefs.h"
#include "amgdefs.h"
#include "statdefs.h"

/* Some local definitions */
#define AFD_ONLY                  1
#define AFD_CHECK_ONLY            2
#define AFD_CHECK                 3
#define AFD_CTRL_ONLY             4
#define SHUTDOWN_ONLY             5
#define SILENT_SHUTDOWN_ONLY      6
#define START_BOTH                7
#define MAKE_BLOCK_FILE           8
#define REMOVE_BLOCK_FILE         9
#define AFD_HEARTBEAT_CHECK_ONLY 10
#define AFD_HEARTBEAT_CHECK      11
#define AFD_INITIALIZE           12
#define AFD_FULL_INITIALIZE      13
#define SET_SHUTDOWN_BIT         14

/* External global variables. */
int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir,
            afd_active_file[MAX_PATH_LENGTH],
            afd_cmd_fifo[MAX_PATH_LENGTH];
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions. */
static void delete_fifodir_files(char *, int),
            delete_log_files(char *, int),
            usage(char *);
static int  check_database(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         start_up,
               readfd,
               afd_ctrl_perm,
               initialize_perm,
               startup_perm,
               shutdown_perm,
               user_offset;
   char        *perm_buffer,
               work_dir[MAX_PATH_LENGTH],
               auto_block_file[MAX_PATH_LENGTH],
               exec_cmd[AFD_CTRL_LENGTH + 1],
               fake_user[MAX_FULL_USER_ID_LENGTH],
               sys_log_fifo[MAX_PATH_LENGTH],
               user[MAX_FULL_USER_ID_LENGTH];
   struct stat stat_buf;

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
   if (get_arg(&argc, argv, "-p", user, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      user_offset = 0;
   }
   else
   {
      user_offset = strlen(user);
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   get_user(user, fake_user, user_offset);

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
         (void)fprintf(stderr, "%s [%s]\n", PERMISSION_DENIED_STR, user);
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
              (perm_buffer[3] == '\t')))
         {
            afd_ctrl_perm = YES;
            shutdown_perm = YES;
            startup_perm  = YES;
            initialize_perm = YES;
         }
         else
         {
            if (posi(perm_buffer, AFD_CTRL_PERM) == NULL)
            {
               afd_ctrl_perm = NO_PERMISSION;
            }
            else
            {
               afd_ctrl_perm = YES;
            }
            if (posi(perm_buffer, SHUTDOWN_PERM) == NULL)
            {
               shutdown_perm = NO_PERMISSION;
            }
            else
            {
               shutdown_perm = YES;
            }
            if (posi(perm_buffer, STARTUP_PERM) == NULL)
            {
               startup_perm = NO_PERMISSION;
            }
            else
            {
               startup_perm = YES;
            }
            if (posi(perm_buffer, INITIALIZE_PERM) == NULL)
            {
               initialize_perm = NO_PERMISSION;
            }
            else
            {
               initialize_perm = YES;
            }
            free(perm_buffer);
         }
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         afd_ctrl_perm   = YES;
         shutdown_perm   = YES;
         startup_perm    = YES;
         initialize_perm = YES;
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (argc <= 2)
   {
      if (argc == 2)
      {
         if (strcmp(argv[1], "-a") == 0) /* Start AFD */
         {
            if (startup_perm != YES)
            {
               (void)fprintf(stderr,
                             "You do not have the permission to start the AFD.\n");
               exit(INCORRECT);
            }
            start_up = AFD_ONLY;
         }
         else if (strcmp(argv[1], "-b") == 0) /* Block auto restart */
              {
                 start_up = MAKE_BLOCK_FILE;
              }
         else if (strcmp(argv[1], "-c") == 0) /* Only check if AFD is active */
              {
                 start_up = AFD_CHECK_ONLY;
              }
         else if (strcmp(argv[1], "-C") == 0) /* Only check if AFD is active */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the AFD.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_CHECK;
              }
         else if (strcmp(argv[1], "-d") == 0) /* Start afd_ctrl */
              {
                 if (afd_ctrl_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the AFD control dialog.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_CTRL_ONLY;
              }
         else if (strcmp(argv[1], "-h") == 0) /* Only check for heartbeat. */
              {
                 start_up = AFD_HEARTBEAT_CHECK_ONLY;
              }
         else if (strcmp(argv[1], "-H") == 0) /* If there is no heartbeat start AFD. */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the AFD.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_HEARTBEAT_CHECK;
              }
         else if (strcmp(argv[1], "-i") == 0) /* Initialize AFD. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to initialize the AFD.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_INITIALIZE;
              }
         else if (strcmp(argv[1], "-I") == 0) /* Full Initialization of AFD. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to do a full initialization of AFD.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_FULL_INITIALIZE;
              }
         else if (strcmp(argv[1], "-s") == 0) /* Shutdown AFD */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD. [%s]\n",
                                  user);
                    exit(INCORRECT);
                 }
                 start_up = SHUTDOWN_ONLY;
              }
         else if (strcmp(argv[1], "-S") == 0) /* Silent AFD shutdown */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD. [%s]\n",
                                  user);
                    exit(INCORRECT);
                 }
                 start_up = SILENT_SHUTDOWN_ONLY;
              }
         else if (strcmp(argv[1], "-r") == 0) /* Removes blocking file */
              {
                 start_up = REMOVE_BLOCK_FILE;
              }
         else if (strcmp(argv[1], "-z") == 0) /* Set shutdown bit. */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to set the shutdown bit. [%s]\n",
                                  user);
                    exit(INCORRECT);
                 }
                 start_up = SET_SHUTDOWN_BIT;
              }
         else if ((strcmp(argv[1], "--help") == 0) ||
                  (strcmp(argv[1], "-?") == 0)) /* Show usage */
              {
                 usage(argv[0]);
                 exit(0);
              }
              else
              {
                 usage(argv[0]);
                 exit(1);
              }
      }
      else /* Start AFD and afd_ctrl */
      {
         if ((startup_perm == YES) && (afd_ctrl_perm == YES))
         {
            start_up = START_BOTH;
         }
         else if (startup_perm == YES)
              {
                 start_up = AFD_ONLY;
              }
         else if (afd_ctrl_perm == YES)
              {
                 start_up = AFD_CTRL_ONLY;
              }
              else
              {
                 (void)fprintf(stderr,
                               "You do not have enough permissions to use this program.\n");
                 exit(INCORRECT);
              }
      }
   }
   else
   {
      usage(argv[0]);
      exit(1);
   }

   (void)umask(0);
   if (chdir(work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to change directory to %s : %s (%s %d)\n",
                    work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables. */
   (void)strcpy(auto_block_file, work_dir);
   (void)strcat(auto_block_file, ETC_DIR);
   (void)strcat(auto_block_file, BLOCK_FILE);
   (void)strcpy(afd_active_file, work_dir);
   (void)strcat(afd_active_file, FIFO_DIR);
   if (check_dir(afd_active_file, R_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(sys_log_fifo, afd_active_file);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcpy(afd_cmd_fifo, afd_active_file);
   (void)strcat(afd_cmd_fifo, AFD_CMD_FIFO);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);

   if ((stat(sys_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   if ((start_up == SHUTDOWN_ONLY) || (start_up == SILENT_SHUTDOWN_ONLY))
   {
      int   loops = 0,
            n;
      pid_t ia_pid;

      /* First get the pid of init_afd before we send the */
      /* shutdown command.                                */
      if ((readfd = open(afd_active_file, O_RDONLY)) == -1)
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stderr, "Failed to open %s : %s (%s %d)\n",
                          afd_active_file, strerror(errno),
                          __FILE__, __LINE__);
         }
         else
         {
            if (start_up == SHUTDOWN_ONLY)
            {
               (void)fprintf(stderr, "There is no AFD active.\n");
            }
         }
         exit(AFD_IS_NOT_ACTIVE);
      }
      if ((n = read(readfd, &ia_pid, sizeof(pid_t))) != sizeof(pid_t))
      {
         if (n == 0)
         {
            (void)fprintf(stderr,
                          "File %s is empty. Unable to determine if AFD is active.\n",
                          afd_active_file);
         }
         else
         {
            (void)fprintf(stderr, "read() error : %s (%s %d)\n",
                          strerror(errno),  __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }
      (void)close(readfd);

      /* Check that we have a valid pid! */
      if (ia_pid < 1)
      {
         (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                       "File %s contains an invalid pid (%d). Please try and terminate it by hand.\n",
#else
                       "File %s contains an invalid pid (%lld). Please try and terminate it by hand.\n",
#endif
                       afd_active_file, (pri_pid_t)ia_pid);
         exit(INCORRECT);
      }

      if (start_up == SHUTDOWN_ONLY)
      {
         (void)fprintf(stdout, "Starting AFD shutdown ");
         fflush(stdout);
      }

      shutdown_afd(user);

      /*
       * Wait for init_afd to terminate. But lets not wait forever.
       */
      for (;;)
      {
         if ((access(afd_active_file, F_OK) == -1) && (errno == ENOENT))
         {
            if (start_up == SHUTDOWN_ONLY)
            {
               (void)fprintf(stdout, "\nDone!\n");
            }
            exit(0);
         }

         if (start_up == SHUTDOWN_ONLY)
         {
            (void)fprintf(stdout, ".");
            fflush(stdout);
         }
         (void)sleep(1);

         if (loops++ >= 120)
         {
            (void)fprintf(stdout, "\nTimeout reached, killing init_afd.\n");
            if (kill(ia_pid, SIGINT) == -1)
            {
               (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                             "Failed to kill init_afd (%d) : %s (%s %d)\n",
#else
                             "Failed to kill init_afd (%lld) : %s (%s %d)\n",
#endif
                             (pri_pid_t)ia_pid, strerror(errno),
                             __FILE__, __LINE__);
            }
            else
            {
               if (start_up == SHUTDOWN_ONLY)
               {
                  (void)fprintf(stdout, "\nDone!\n");
               }
            }
            break;
         }
      }

      /*
       * Before we exit lets check if init_afd is really gone.
       */
      loops = 0;
      for (;;)
      {
         if ((access(afd_active_file, F_OK) == -1) && (errno == ENOENT))
         {
            break;
         }
         (void)sleep(1);

         if (loops++ >= 40)
         {
            (void)fprintf(stdout, "\nSecond timeout reached, killing init_afd the hard way.\n");
            if (kill(ia_pid, SIGKILL) == -1)
            {
               (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                             "Failed to kill init_afd (%d) : %s (%s %d)\n",
#else
                             "Failed to kill init_afd (%lld) : %s (%s %d)\n",
#endif
                             (pri_pid_t)ia_pid, strerror(errno),
                             __FILE__, __LINE__);
            }
            break;
         }
      }

      exit(0);
   }
   else if (start_up == AFD_CTRL_ONLY)
        {
           (void)strcpy(exec_cmd, AFD_CTRL);
           if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                      (char *) 0) == -1)
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to execute %s : %s (%s %d)\n",
                            exec_cmd, strerror(errno), __FILE__, __LINE__);
              exit(1);
           }
           exit(0);
        }
   else if (start_up == AFD_ONLY)
        {
           /* Check if starting of AFD is currently disabled.  */
           if (eaccess(auto_block_file, F_OK) == 0)
           {
              (void)fprintf(stderr,
                            "AFD is currently disabled by system manager.\n");
              exit(INCORRECT);
           }

           if (check_database() == -1)
           {
              (void)fprintf(stderr,
                            "ERROR   : Cannot read <%s%s%s> file : %s\n          Unable to start AFD.\n",
                            p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE,
                            strerror(errno));
              exit(INCORRECT);
           }

           if (check_afd_heartbeat(25L, NO) == 1)
           {
              (void)fprintf(stdout, "AFD is active in %s\n", p_work_dir);
              exit(AFD_IS_ACTIVE);
           }

           if (startup_afd() != YES)
           {
              exit(INCORRECT);
           }

           exit(0);
        }
   else if ((start_up == AFD_CHECK) || (start_up == AFD_CHECK_ONLY) ||
            (start_up == AFD_HEARTBEAT_CHECK) ||
            (start_up == AFD_HEARTBEAT_CHECK_ONLY))
        {
           if ((start_up == AFD_CHECK_ONLY) ||
               (start_up == AFD_HEARTBEAT_CHECK_ONLY))
           {
              if (check_afd_heartbeat(25L, NO) == 1)
              {
                 (void)fprintf(stdout, "AFD is active in %s\n", p_work_dir);
                 exit(AFD_IS_ACTIVE);
              }
           }
           else
           {
              if (check_afd_heartbeat(25L, YES) == 1)
              {
                 (void)fprintf(stdout, "AFD is active in %s\n", p_work_dir);
                 exit(AFD_IS_ACTIVE);
              }
           }
           if ((start_up == AFD_CHECK) ||
               (start_up == AFD_HEARTBEAT_CHECK))
           {
              /* Check if starting of AFD is currently disabled.  */
              if (eaccess(auto_block_file, F_OK) == 0)
              {
                 (void)fprintf(stderr,
                               "AFD is currently disabled by system manager.\n");
                 exit(INCORRECT);
              }

              if (check_database() == -1)
              {
                 (void)fprintf(stderr,
                               "Cannot read <%s%s%s> file : %s\nUnable to start AFD.\n",
                               p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE,
                               strerror(errno));
                 exit(NO_DIR_CONFIG);
              }

              if (startup_afd() != YES)
              {
                 exit(INCORRECT);
              }
           }
           else
           {
              (void)fprintf(stderr, "No AFD active in %s\n", p_work_dir);
           }

           exit(0);
        }
   else if (start_up == MAKE_BLOCK_FILE)
        {
           int fd;

           if ((fd = open(auto_block_file, O_WRONLY | O_CREAT | O_TRUNC,
#ifdef GROUP_CAN_WRITE
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
#else
                          S_IRUSR | S_IWUSR)) == -1)
#endif
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to create block file %s : %s (%s %d)\n",
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           if (close(fd) == -1)
           {
              (void)fprintf(stderr,
                            "WARNING : Failed to close() block file %s : %s (%s %d)\n",
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
           }
           exit(SUCCESS);
        }
   else if (start_up == REMOVE_BLOCK_FILE)
        {
           if (remove(auto_block_file) < 0)
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to remove block file %s : %s (%s %d)\n",
                            auto_block_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           exit(SUCCESS);
        }
   else if ((start_up == AFD_INITIALIZE) || (start_up == AFD_FULL_INITIALIZE))
        {
           if (check_afd_heartbeat(25L, NO) == 1)
           {
              (void)fprintf(stderr,
                            "ERROR   : AFD is still active, unable to initialize.\n");
              exit(INCORRECT);
           }
           else
           {
              int  offset;
              char dirs[MAX_PATH_LENGTH];

              offset = sprintf(dirs, "%s%s", p_work_dir, FIFO_DIR);
              delete_fifodir_files(dirs, offset);
              (void)sprintf(dirs, "%s%s", p_work_dir, AFD_MSG_DIR);
              if (rec_rmdir(dirs) == INCORRECT)
              {
                 (void)fprintf(stderr,
                               "WARNING : Failed to delete everything in %s.\n",
                               dirs);
              }
              if (start_up == AFD_FULL_INITIALIZE)
              {
                 (void)sprintf(dirs, "%s%s", p_work_dir, AFD_FILE_DIR);
                 if (rec_rmdir(dirs) == INCORRECT)
                 {
                    (void)fprintf(stderr,
                                  "WARNING : Failed to delete everything in %s.\n",
                                  dirs);
                 }
                 (void)sprintf(dirs, "%s%s", p_work_dir, AFD_ARCHIVE_DIR);
                 if (rec_rmdir(dirs) == INCORRECT)
                 {
                    (void)fprintf(stderr,
                                  "WARNING : Failed to delete everything in %s.\n",
                                  dirs);
                 }
                 offset = sprintf(dirs, "%s%s", p_work_dir, LOG_DIR);
                 delete_log_files(dirs, offset);
              }
              exit(SUCCESS);
           }
        }
   else if (start_up == SET_SHUTDOWN_BIT)
        {
           int         fd;
           off_t       offset;
           char        *ptr,
                       *shared_shutdown;
           struct stat stat_buf;

           if ((fd = open(afd_active_file, O_RDWR)) == -1)
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to open() %s : %s (%s %d)\n",
                            afd_active_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           if (fstat(fd, &stat_buf) == -1)
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to fstat() %s : %s (%s %d)\n",
                            afd_active_file, strerror(errno),
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           offset = ((NO_OF_PROCESS + 1) * sizeof(pid_t)) + sizeof(unsigned int) + 1 + 1;
           if ((offset + 1) != stat_buf.st_size)
           {
              (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                            "ERROR   : Unable to set shutdown bit due to incorrect size (%ld != %ld) of %s.\n",
#else
                            "ERROR   : Unable to set shutdown bit due to incorrect size (%lld != %lld) of %s.\n",
#endif
                            (pri_off_t)(offset + 1),
                            (pri_off_t)stat_buf.st_size, afd_active_file);
              exit(INCORRECT);
           }
#ifdef HAVE_MMAP
           if ((ptr = mmap(0, offset, (PROT_READ | PROT_WRITE),
                           MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
           if ((ptr = mmap_emu(0, offset, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, afd_active_file, 0)) == (caddr_t) -1)
#endif
           {
              (void)fprintf(stderr, "ERROR   : mmap() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
           offset -= (sizeof(unsigned int) + 1 + 1);
           shared_shutdown = ptr + offset + sizeof(unsigned int);
           *shared_shutdown = SHUTDOWN;
           (void)fprintf(stdout, "Shutdown bit set.\n");
           exit(SUCCESS);
        }

   /* Check if starting of AFD is currently disabled.  */
   if (eaccess(auto_block_file, F_OK) == 0)
   {
      (void)fprintf(stderr, "AFD is currently disabled by system manager.\n");
      exit(INCORRECT);
   }

   /* Is another AFD active in this directory? */
   if (check_afd_heartbeat(25L, YES) == 1)
   {
      /* Another AFD is active. Only start afd_ctrl. */
      (void)strcpy(exec_cmd, AFD_CTRL);
      if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir, (char *) 0) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to execute %s : %s (%s %d)\n",
                       exec_cmd, strerror(errno), __FILE__, __LINE__);
         exit(1);
      }
   }
   else /* Start both */
   {
      if (check_database() == -1)
      {
         (void)fprintf(stderr,
                       "Cannot read <%s%s%s> file : %s\nUnable to start AFD.\n",
                       p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE,
                       strerror(errno));
         exit(INCORRECT);
      }

      if (startup_afd() == YES)
      {
         (void)strcpy(exec_cmd, AFD_CTRL);
         if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                    (char *) 0) == -1)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to execute %s : %s (%s %d)\n",
                          exec_cmd, strerror(errno),
                          __FILE__, __LINE__);
            exit(1);
         }
      }
      else
      {
         exit(INCORRECT);
      }
   }

   exit(0);
}


/*++++++++++++++++++++++++++++ check_database() +++++++++++++++++++++++++*/
static int
check_database(void)
{
   char db_file[MAX_PATH_LENGTH];

   (void)sprintf(db_file, "%s%s%s",
                 p_work_dir, ETC_DIR, DEFAULT_DIR_CONFIG_FILE);

#ifdef WITH_AUTO_CONFIG
   if (eaccess(db_file, R_OK) == -1)
   {
      int         ret;
      FILE        *fp;
      struct stat stat_buf;

      (void)sprintf(db_file, "%s%s", p_work_dir, ETC_DIR);
      if (stat(db_file, &stat_buf) == -1)
      {
         if (errno == ENOENT)
         {
            if (mkdir(db_file, DIR_MODE) == -1)
            {
               (void)fprintf(stderr, "Failed to mkdir() `%s' : %s\n",
                             db_file, strerror(errno));
               return(-1);
            }
         }
         else
         {
            return(-1);
         }
      }
      (void)sprintf(db_file, "%s %s 2>&1", AFD_AUTO_CONFIG, p_work_dir);
      if ((fp = popen(db_file, "r")) == NULL)
      {
         (void)fprintf(stderr, "Failed to popen() `%s' : %s\n",
                       db_file, strerror(errno));
         return(-1);
      }
      db_file[0] = '\0';
      while (fgets(db_file, MAX_PATH_LENGTH, fp) != NULL)
      {
         ;
      }
      if (db_file[0] != '\0')
      {
         (void)fprintf(stderr, "%s failed : `%s'\n", AFD_AUTO_CONFIG, db_file);
         ret = -1;
      }
      else
      {
         ret = 0;
      }
      if (ferror(fp))
      {
         (void)fprintf(stderr, "ferror() error : %s\n", strerror(errno));
      }
      if (pclose(fp) == -1)
      {
         (void)fprintf(stderr, "Failed to pclose() : %s\n", strerror(errno));
      }
      return(ret);
   }
   else
   {
      return(0);
   }
#else
   return(eaccess(db_file, R_OK));
#endif
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
         {
            FSA_ID_FILE,
            FRA_ID_FILE,
            STATUS_SHMID_FILE,
            BLOCK_FILE,
            AMG_COUNTER_FILE,
            COUNTER_FILE,
            MESSAGE_BUF_FILE,
            MSG_CACHE_FILE,
            MSG_QUEUE_FILE,
#ifdef WITH_ERROR_QUEUE
            ERROR_QUEUE_FILE,
#endif
            FILE_MASK_FILE,
            DC_LIST_FILE,
            DIR_NAME_FILE,
            JOB_ID_DATA_FILE,
            PWB_DATA_FILE,
            CURRENT_MSG_LIST_FILE,
            AMG_DATA_FILE,
            AMG_DATA_FILE_TMP,
            LOCK_PROC_FILE,
            AFD_ACTIVE_FILE,
            WINDOW_ID_FILE,
            SYSTEM_LOG_FIFO,
            EVENT_LOG_FIFO,
            RECEIVE_LOG_FIFO,
            TRANSFER_LOG_FIFO,
            TRANS_DEBUG_LOG_FIFO,
            AFD_CMD_FIFO,
            AFD_RESP_FIFO,
            AMG_CMD_FIFO,
            DB_UPDATE_FIFO,
            FD_CMD_FIFO,
            AW_CMD_FIFO,
            IP_FIN_FIFO,
            SF_FIN_FIFO,
            RETRY_FD_FIFO,
            FD_DELETE_FIFO,
            FD_WAKE_UP_FIFO,
            PROBE_ONLY_FIFO,
#ifdef _INPUT_LOG
            INPUT_LOG_FIFO,
#endif
#ifdef _OUTPUT_LOG
            OUTPUT_LOG_FIFO,
#endif
#ifdef _DELETE_LOG
            DELETE_LOG_FIFO,
#endif
#ifdef _PRODUCTION_LOG
            PRODUCTION_LOG_FIFO,
#endif
            DEL_TIME_JOB_FIFO,
            FD_READY_FIFO,
            MSG_FIFO,
            DC_CMD_FIFO, /* from amgdefs.h */
            DC_RESP_FIFO,
            AFDD_LOG_FIFO
         },
        *mfilelist[] =
        {
           FSA_STAT_FILE_ALL,
           FRA_STAT_FILE_ALL,
           ALTERNATE_FILE_ALL,
           DB_UPDATE_REPLY_FIFO_ALL
        };

   file_ptr = fifodir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      (void)strcpy(file_ptr, filelist[i]);
      (void)unlink(fifodir);
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      (void)remove_files(fifodir, &mfilelist[i][1]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*++++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
           "/DAEMON_LOG.init_afd",
           NEW_STATISTIC_FILE,
           NEW_ISTATISTIC_FILE
        },
        *mloglist[] =
        {
           SYSTEM_LOG_NAME_ALL,
           EVENT_LOG_NAME_ALL,
           RECEIVE_LOG_NAME_ALL,
           TRANSFER_LOG_NAME_ALL,
           TRANS_DB_LOG_NAME_ALL,
#ifdef _INPUT_LOG
           INPUT_BUFFER_FILE_ALL,
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DELETE_LOG
           DELETE_BUFFER_FILE_ALL,
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_BUFFER_FILE_ALL,
#endif
           ISTATISTIC_FILE_ALL,
           STATISTIC_FILE_ALL
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      (void)unlink(logdir);
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      (void)remove_files(logdir, mloglist[i]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s[ -w <AFD working dir>][ -p <role>][ -u[ <user>]] [option]\n", progname);
   (void)fprintf(stderr, "              -a          only start AFD\n");
   (void)fprintf(stderr, "              -b          Blocks auto restart of AFD\n");
   (void)fprintf(stderr, "              -c          only check if AFD is active\n");
   (void)fprintf(stderr, "              -C          check if AFD is active, if not start it\n");
   (void)fprintf(stderr, "              -d          only start afd_ctrl dialog\n");
   (void)fprintf(stderr, "              -h          only check for heartbeat\n");
   (void)fprintf(stderr, "              -H          check if heartbeat is active, if not start AFD\n");
   (void)fprintf(stderr, "              -i          initialize AFD, by deleting fifodir\n");
   (void)fprintf(stderr, "              -I          initialize AFD, by deleting everything\n");
   (void)fprintf(stderr, "                          except for etc directory\n");
   (void)fprintf(stderr, "              -r          Removes blocking file\n");
   (void)fprintf(stderr, "              -s          shutdown AFD\n");
   (void)fprintf(stderr, "              -S          silent AFD shutdown\n");
   (void)fprintf(stderr, "              -z          Set shutdown bit\n");
   (void)fprintf(stderr, "              --help      Prints out this syntax\n");
   (void)fprintf(stderr, "              -v          Just print version number\n");
   (void)fprintf(stderr, "              --version   Show current version\n");

   return;
}
