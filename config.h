/*
 *  config.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __config_h
#define __config_h

/*************************************************************************/
/*        Definitions that enable/disable features of the AFD            */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * With the following set of options you can enable/disable certain
 * features of the AFD. But note, the more features you enable will
 * increase memory usage and in some cases may harm performance.
 *
 * _INPUT_LOG             - If you want to log all files names on input you
 *                          have to enable these features. But note that
 *                          enabling this will pose a performance penalty,
 *                          especially if the log files are very large.
 * _OUTPUT_LOG            - If you want to log all files names on output you
 *                          have to enable these features. But note that
 *                          enabling this will pose a performance penalty,
 *                          especially if the log files are very large.
 * _DELETE_LOG            - If set, all files names that are deleted by the
 *                          AFD get logged. They can then be viewed with
 *                          show_dlog. But note that enabling this will pose
 *                          a performance penalty, especially if lots of
 *                          files get deleted.
 * _PRIO_CHECK            - When set the FD will check for priority. Most
 *                          connections are fast enough so you do not need
 *                          to set this option. However if you do have a very
 *                          slow connection with heavy traffic it is best to
 *                          enable this feature. This will ensure that files
 *                          with a higher priority get send first.
 * _AGE_LIMIT             - Sometimes it is useful to remove files that have
 *                          reached a certain age. The age of each individual
 *                          file gets checked, and if it has reached the
 *                          limit it is removed.
 * _VERIFY_FSA            - Every time something is written to the FSA, it is
 *                          checked whether it is correct. If not enabled you
 *                          might see some very large numbers in the 'afd_ctrl'
 *                          dialog. Though this is very seldom. This can be
 *                          corrected with 'fsa_edit'. The checking is only
 *                          done where the regions are being locked.
 * _BURST_MODE            - When files are currently being send via FTP and
 *                          all connections are busy, the AMG/FD will transfer
 *                          the files to the transferring process.
 * _WITH_UID_CHECK        - Checks the user by checking his user ID. If this
 *                          is not set this check is done by checking the
 *                          environment name LOGNAME.
 * _LOCK_REMOVE_INFO      - If set, every time an archive is removed a log
 *                          entry is made in the system log. This can be very
 *                          annoying if you have lots of files in the archive.
 *                          So it's best to have this disabled.
 * _WITH_SHUTDOWN         - When FTP/SMTP closes the data connection it is
 *                          common practice to shutdown() the connection. This
 *                          can however have the unpleasant side effect that
 *                          data can be lost it is still in the local buffer.
 *                          It's best not to enable this.
 * _WITH_SEND             - When writing to sockets we use the send() command
 *                          when this option is set. Otherwise we simply use
 *                          write().
 * _WITH_PTHREAD          - If your system supports POSIX threads enable this.
 *                          Still VERY experimental.
 * _WITH_WMO_SUPPORT      - Support for the special protocol used by the WMO
 *                          to transport data via TCP.
 * _WITH_MAP_SUPPORT      - Support for the special protocol used by the MAP
 *                          system of the German Weather Service (DWD).
 * _WITH_READY_FILES      - Enables support for FSS ready files. Only used
 *                          within the German Weather Service (DWD).
 * _WITH_EUMETSAT_HEADERS - Support to write EUMETSAT headers into each
 *                          file send via FTP.
 * _WITH_UNIQUE_NUMBERS   - When extracting files to the form one file
 *                          per bulletin it can happen that some bulletins
 *                          are over written. To eliminate this set this
 *                          option.
 *-----------------------------------------------------------------------*/
#define _INPUT_LOG
#define _OUTPUT_LOG
#define _DELETE_LOG
#define _PRIO_CHECK
#define _AGE_LIMIT
#define _VERIFY_FSA
#define _BURST_MODE
#define _WITH_UID_CHECK
/* #define _LOCK_REMOVE_INFO */
#define _WITH_SHUTDOWN
/* #define _WITH_SEND */
/* #define _WITH_PTHREAD */
#define _WITH_WMO_SUPPORT
#define _WITH_AFW2WMO
/* #define _RADAR_CHECK */
/* #define _WITH_MAP_SUPPORT */
/* #define _WITH_READY_FILES */
/* #define _WITH_EUMETSAT_HEADERS */
#define _WITH_UNIQUE_NUMBERS

/*-----------------------------------------------------------------------*
 * These following options are only for the dialogs of the AFD.
 *
 * _AUTO_REPOSITION          - When set, the main window of afd_ctrl will
 *                             be automatically repositioned when the
 *                             right or bottom side of the 'afd_ctrl'
 *                             window touches the right or bottom of the
 *                             screen. This will be done until the top or
 *                             left side of the afd_ctrl window reaches the
 *                             top or left side of the screen.
 * _SQUARE_LED               - Makes the debug LED square. Otherwise the
 *                             LED is round.
 * _SLOW_COUNTER             - If this value is set the line counter of
 *                             the dialog 'show_log' is only updated when
 *                             the log file does not increase. Gives a
 *                             slight performance boost when lots of log
 *                             entries are being made.
 * _WITH_XPM                 - If the system supports XPM and this option
 *                             is set it will indicate which host is the
 *                             active one, when there is a secondary host.
 * _WITH_CHECK_TIME_INTERVAL - This option is only used for the program
 *                             viewing the input and output log files.
 *                             On slow systems this option should be
 *                             enabled, otherwise a search can take too
 *                             long and the user cannot interrupt the
 *                             search.
 *-----------------------------------------------------------------------*/
#define _AUTO_REPOSITION
#define _SQUARE_LED
#define _NO_LED_FRAME
#define _SLOW_COUNTER
#define _WITH_XPM
#define _WITH_CHECK_TIME_INTERVAL



/*************************************************************************/
/*        Debug/test features of the AFD. (Should be disabled)           */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * Can only be used with dir_check. This tells the process dir_check to
 * print the data of structure directory_entry to stdout, for every
 * directory entry.
 *-----------------------------------------------------------------------*/
/* #define _TEST_FILE_TABLE */

/*-----------------------------------------------------------------------*
 * Used to enable general debug options. All debugging information will
 * be written to stdout.
 *-----------------------------------------------------------------------*/
/* #define _DEBUG */

/*-----------------------------------------------------------------------*
 * When enabled all communication done via fifo will be written to
 * stdout.
 *-----------------------------------------------------------------------*/
/* #define _FIFO_DEBUG */

/*-----------------------------------------------------------------------*
 * If you want to set the resources for the X stuff it is a good idea
 * to enable support for editres.
 *-----------------------------------------------------------------------*/
/* #define _EDITRES */




/*************************************************************************/
/*      Global definitions for the AFD (Automatic File Distributor)      */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * Some commonly used maximum values.
 *-----------------------------------------------------------------------*/
#define MAX_FILENAME_LENGTH 256
#define MAX_PATH_LENGTH     1024
#define MAX_LINE_LENGTH     2048

/*-----------------------------------------------------------------------*
 * The interval time (in seconds) at which the process 'init_afd'
 * checks if all its child processes are still running and checks how
 * many directories (jobs) are currently in the AFD file directory.
 * DEFAULT: 3
 *-----------------------------------------------------------------------*/
#define AFD_RESCAN_TIME 3

/*-----------------------------------------------------------------------*
 * When the disk is full, this is the time interval (in seconds) at
 * which the AFD will retry at that point where it was stopped.
 * DEFAULT: 60
 *-----------------------------------------------------------------------*/
#define DISK_FULL_RESCAN_TIME 60

/*-----------------------------------------------------------------------*
 * The maximum and default number of parallel jobs per host.
 *-----------------------------------------------------------------------*/
#define MAX_NO_PARALLEL_JOBS     5
#define DEFAULT_NO_PARALLEL_JOBS 2

/*-----------------------------------------------------------------------*
 * The default file and directory creation modes.
 *-----------------------------------------------------------------------*/
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define DIR_MODE  (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | \
                   S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH)

/*-----------------------------------------------------------------------*
 * The interval at which the archive should be scanned for old
 * archives that can be removed.
 * DEFAULT: 3600 (=> 1 hour)
 *-----------------------------------------------------------------------*/
#define ARCHIVE_RESCAN_TIME 3600

/*-----------------------------------------------------------------------*
 * The interval at which a new archive will be created. This value
 * can be used to reduce the number of archives. The higher this value
 * will be, the smaller will be the number of archive directories
 * created. But note that the higher it will be, it will increase the
 * the possibility that files will be over written in that archive
 * directory. The lowest value is 1.
 * DEFAULT: 60 (=> 1 minute)
 *-----------------------------------------------------------------------*/
#define ARCHIVE_STEP_TIME 60

/*-----------------------------------------------------------------------*
 * The maximum length of the host name (alias) that is displayed by
 * 'afd_ctrl'.
 * DEFAULT: 8
 *-----------------------------------------------------------------------*/
#define MAX_HOSTNAME_LENGTH 8

/*-----------------------------------------------------------------------*
 * The maximum directory alias name length that is displayed by
 * 'dir_ctrl'.
 * DEFAULT: 10
 *-----------------------------------------------------------------------*/
#define MAX_DIR_ALIAS_LENGTH 10

/*-----------------------------------------------------------------------*
 * The maximum size of the system and transfer debug log files.
 * DEFAULT: 2097152  (2MB)
 *-----------------------------------------------------------------------*/
#define MAX_LOGFILE_SIZE 2097152

/*-----------------------------------------------------------------------*
 * The following definitions are for logging input, output and debug
 * file names:
 *
 * SWITCH_FILE_TIME      - When to start a new I/O log file. If you for
 *                         example want to create a new log file every
 *                         day then use 86400. Or you want to do it
 *                         once a week then you have to put 604800 here.
 * MAX_INPUT_LOG_FILES   - The number of log files that should be kept
 *                         for input logging. If it is set to 10 and
 *                         SWITCH_FILE_TIME is 86400 (i.e. one day), you
 *                         will store the input log for 10 days.
 * MAX_OUTPUT_LOG_FILES  - The number of log files that should be kept
 *                         for output logging. If it is set to 10 and
 *                         SWITCH_FILE_TIME is 86400 (i.e. one day), you
 *                         will store the output log for 10 days.
 * MAX_DELETE_LOG_FILES  - Same as above only for the delete log.
 *-----------------------------------------------------------------------*/
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG)
#define SWITCH_FILE_TIME       86400
#endif
#ifdef _INPUT_LOG
#define MAX_INPUT_LOG_FILES    7
#endif
#ifdef _OUTPUT_LOG
#define MAX_OUTPUT_LOG_FILES   7
#endif
#ifdef _DELETE_LOG
#define MAX_DELETE_LOG_FILES   7
#endif

/*-----------------------------------------------------------------------*
 * The following definitions are used to control the length and interval
 * of the log history for the receive-, system- and transfer-log.
 * HISTORY_LOG_INTERVAL - At what interval the log history is shuffled
 *                        one bock to the left in the mon_ctrl dialog.
 *                        DEFAULT: 3600
 * MAX_LOG_HISTORY      - The number of history blocks are to be stored.
 *                        DEFAULT: 24
 *-----------------------------------------------------------------------*/
#define HISTORY_LOG_INTERVAL 3600
#define MAX_LOG_HISTORY      24



/*************************************************************************/
/*      Definitions for the AMG (Automatic Message Generator)            */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * The interval in seconds at which the AMG should check if it is time
 * to handle any time jobs in the queue.
 * DEFAULT: 60
 *-----------------------------------------------------------------------*/
#define TIME_CHECK_INTERVAL 60

/*-----------------------------------------------------------------------*
 * REPORT_DIR_TIME_INTERVAL - The interval in seconds at which the AMG
 *                            should report the time it takes for
 *                            scanning one full sets of directories.
 *                            To disable this option comment out this
 *                            variable.
 *                            DEFAULT: 3600
 * MAX_DIFF_TIME            - If you don't want it report every interval
 *                            but only when it reaches a certain value
 *                            define this.
 *                            DEFAULT: 30L
 *-----------------------------------------------------------------------*/
#define REPORT_DIR_TIME_INTERVAL 3600
#ifdef REPORT_DIR_TIME_INTERVAL
#define MAX_DIFF_TIME            30L
#endif /* REPORT_DIR_TIME_INTERVAL */

/*-----------------------------------------------------------------------*
 * The maximum number of files that may be handled in one time interval.
 * DEFAULT: 800
 *-----------------------------------------------------------------------*/
#define MAX_FILES_FOR_TIME_JOBS 800

/*-----------------------------------------------------------------------*
 * The rescan time of the user directories in the DIR_CONFIG file. Don't
 * pick a small value here if your system shows high system load when
 * working with files or directories (eg. fault tolerant systems).
 * This is also the interval time that the AMG checks if the DIR_CONFIG
 * or HOST_CONFIG file have been changed. The unit is second.
 * DEFAULT: 5
 *-----------------------------------------------------------------------*/
#define DEFAULT_RESCAN_TIME 5

/*-----------------------------------------------------------------------*
 * The following entries are used to set the maximum values for the
 * DIR_CONFIG file. They are as followed:
 *
 * MAX_NO_OPTIONS       - Number of options that may be defined for one
 *                        destination.
 * MAX_RECIPIENT_LENGTH - Maximum length that one recipient may have,
 *                        including the directory entry.
 * MAX_FILE_MASK_BUFFER - Maximum number of bytes that may be used to
 *                        store the file masks for one file group entry.
 *                        Each file mask will be terminated by a new-
 *                        line.
 *-----------------------------------------------------------------------*/
#define MAX_NO_OPTIONS       10
#define MAX_RECIPIENT_LENGTH 256
#define MAX_OPTION_LENGTH    256
#define MAX_FILE_MASK_BUFFER 4096

/*-----------------------------------------------------------------------*
 * ONE_DIR_COPY_TIMEOUT - When AMG has collected MAX_COPIED_FILES, it
 *                        will try to continue to copy from this directory
 *                        until it reaches ONE_DIR_COPY_TIMEOUT timeout.
 *                        If you want to disable this option comment it
 *                        out (NOT by setting it to 0).
 *                        DEFAULT: 10 (seconds)
 * DIR_CHECK_TIMEOUT    - Sometimes it can take very long for dir_check
 *                        to go once through all directories and it will
 *                        take too long until important files get send.
 *                        This is only effective when the administrator
 *                        has marked one or more directories with an
 *                        asterix '*'.
 *                        DEFAULT: 60 (seconds)
 *                        NOTE: This is only a quick hack until a better
 *                              solution is found.
 *-----------------------------------------------------------------------*/
#define ONE_DIR_COPY_TIMEOUT 10
#ifndef _WITH_PTHREAD
#define DIR_CHECK_TIMEOUT    60
#endif

/*-----------------------------------------------------------------------*
 * MAX_NO_OF_DIR_CHECKS - Maximum number of times that the process
 *                        dir_check may fork itself.
 *                        DEFAULT: 20
 * MAX_PROCESS_PER_DIR  - The maximum number of times that the process
 *                        dir_check may fork itself per directory. This
 *                        number must be less then or equal to
 *                        MAX_NO_OF_DIR_CHECKS!
 *                        DEFAULT: 10
 *-----------------------------------------------------------------------*/
#define MAX_NO_OF_DIR_CHECKS 20
#define MAX_PROCESS_PER_DIR  10

/*-----------------------------------------------------------------------*
 * When no archive time is specified in the DIR_CONFIG file this is it's
 * default value. If you expect to distribute lot's of files its best to
 * set this value to zero. Because when you forget to enter a value
 * it can very quickly fill up your disk. The unit depends on
 * ARCHIVE_UNIT.
 * DEFAULT: 0
 *-----------------------------------------------------------------------*/
#define DEFAULT_ARCHIVE_TIME 0




/*************************************************************************/
/*            Definitions for the FD (File Distributor)                  */
/*                                                                       */
/*   NOTE: some of the values are set by the AMG.                        */
/*************************************************************************/

/*-----------------------------------------------------------------------*
 * The time interval at which the FD looks in the message directory to
 * see if a new message has arrived, or checks if it has to handle any
 * error messages.
 * DEFAULT: 10
 *-----------------------------------------------------------------------*/
#define FD_RESCAN_TIME 10

/*-----------------------------------------------------------------------*
 * The time interval at which the FD scans remote directories for files
 * that must be retrieved.
 * DEFAULT: 60
 *-----------------------------------------------------------------------*/
#define DEFAULT_REMOTE_FILE_CHECK_INTERVAL 60

/*-----------------------------------------------------------------------*
 * If this is defined, AFD will try to use the type-of-service field
 * in the IP header for a TCP socket. For FTP this would minimize delay
 * on the control connection and maximize throughput on the data
 * connection.
 * To disable just make a comment of this.
 *-----------------------------------------------------------------------*/
#define _WITH_TOS

/*-----------------------------------------------------------------------*
 * The number of simultaneous connections/transfers for input and
 * output:
 *
 * MAX_DEFAULT_CONNECTIONS      - default when no value is set in
 *                                AFD_CONFIG file (default 30).
 * MAX_CONFIGURABLE_CONNECTIONS - the maximum configurable value in
 *                                AFD_CONFIG file (default 512). If
 *                                a higher value is specified it will
 *                                default to MAX_DEFAULT_CONNECTIONS.
 *-----------------------------------------------------------------------*/
#define MAX_DEFAULT_CONNECTIONS      30
#define MAX_CONFIGURABLE_CONNECTIONS 512

/*-----------------------------------------------------------------------*
 * If the number of messages queued is very large and most messages
 * belong to a host that is very slow, new messages will only be
 * processed very slowly when the whole queue is scanned each time a
 * new message arrives or a job has terminated. The following parameters
 * can reduce system load and make FD much more responsive in such
 * situations.
 *
 * MAX_QUEUED_BEFORE_CECKED   - the maximum number of messages that
 *                              must be queued before we start
 *                              counting the loops.
 *                              DEFAULT: 4000
 * ELAPSED_LOOPS_BEFORE_CHECK - the number of loops before the whole
 *                              queue is being checked.
 *                              DEFAULT: 20
 *-----------------------------------------------------------------------*/
#define MAX_QUEUED_BEFORE_CECKED   4000
#define ELAPSED_LOOPS_BEFORE_CHECK 20

/*-----------------------------------------------------------------------*
 * The default retry time (in seconds) when a transfer has failed.
 * Don't set this value to low! If the connection is very bad to the
 * remote site, it can happen that you succeed to connect but then
 * fail to transmit the file and in most cases will then also fail to
 * disconnect properly. And if this continues too fast the remote
 * host will have lots of ftp daemons that still think they are connected.
 * DEFAULT: 600 (=> 10 min)
 *-----------------------------------------------------------------------*/
#define DEFAULT_RETRY_INTERVAL 600

/*-----------------------------------------------------------------------*
 * The default block size when transmitting files.
 * DEFAULT: 1024
 *-----------------------------------------------------------------------*/
#define DEFAULT_TRANSFER_BLOCKSIZE 1024

/*-----------------------------------------------------------------------*
 * The maximum block size that may be specified in the edit_hc dialog.
 * Experiment with these values to see if they have any big impact
 * on your system.
 * DEFAULT: 65536
 *-----------------------------------------------------------------------*/
#define MAX_TRANSFER_BLOCKSIZE 65536

/*-----------------------------------------------------------------------*
 * If a transfer of a big file gets interrupted, it is annoying to
 * always retransmit the complete file. So files will be appended when
 * we already have send a part of that file. However it does not make
 * sense to do this for very small files, or when we only have transfered
 * a couple of bytes. With this definition you can define when a file
 * should be appended.
 *-----------------------------------------------------------------------*/
#define MAX_SEND_BEFORE_APPEND 102400


/*************************************************************************/
/* End of user definable things. Please do not edit below here.          */
/*************************************************************************/

#ifdef _WITH_PTHREAD
#define _REENTRANT
#endif
#endif /* __config_h */
