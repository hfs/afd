/*
 *  write_host_config.c - Part of AFD, an automatic file distribution
 *                        program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
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
 **   write_host_config - writes the HOST_CONFIG file
 **
 ** SYNOPSIS
 **   time_t write_host_config(int              no_of_hosts,
 **                            char             *host_config_file,
 **                            struct host_list *hl)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   The modification time of the HOST_CONFIG file. Will exit() with
 **   INCORRECT when any system call will fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.12.1997 H.Kiehl Created
 **   02.03.1998 H.Kiehl Put host switching information into HOST_CONFIG.
 **   28.07.1998 H.Kiehl Change mode to 660 for HOST_CONFIG so when
 **                      two users use the same binaries each can edit
 **                      their own HOST_CONFIG file.
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **   10.01.2004 H.Kiehl Added option for fast renaming.
 **   10.06.2004 H.Kiehl Added transfer rate limit.
 **   27.06.2004 H.Kiehl Added TTL.
 **   03.11.2004 H.Kiehl Write the new HOST_CONFIG to some temporary file
 **                      and when done overwrite the original file.
 **                      Otherwise when the disk is full HOST_CONFIG
 **                      can just be an empty file!
 **   16.02.2006 H.Kiehl Added socket send and receive buffer.
 **   28.02.2006 H.Kiehl Added keep connected parameter.
 **   08.03.2006 H.Kiehl Added dupcheck on a per host basis.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>            /* exit()                                 */
#include <unistd.h>            /* write(), close()                       */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>

#ifdef WITH_DUP_CHECK
#define HOST_CONFIG_TEXT_PART1 "#\n\
#                Host configuration file for the AFD\n\
#                ===================================\n\
#\n\
# There are 22 parameters that can be configured for each remote\n\
# host. They are:\n\
#\n\
# Keep connected          <----------------------------------------------+\n\
# Duplicate check flag    <-------------------------------------------+  |\n\
# Duplicate check timeout <----------------------------------------+  |  |\n\
# Socket receive buffer   <-------------------------------------+  |  |  |\n\
# Socket send buffer      <---------------------------------+   |  |  |  |\n\
#                                                           |   |  |  |  |\n\
# AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB:HS:SF:TRL:TTL:SSB:SRB:DT:DF:KC\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   |\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   +-> TTL\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   +-----> Transfer rate limit\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  +---------> Protocol options\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  +------------> Host status\n\
# |   |   |   |  |  |  |  |  |  |   |  |  +---------------> Number of no bursts\n\
# |   |   |   |  |  |  |  |  |  |   |  +------------------> Transfer timeout\n\
# |   |   |   |  |  |  |  |  |  |   +---------------------> File size offset\n\
# |   |   |   |  |  |  |  |  |  +-------------------------> Successful retries\n\
# |   |   |   |  |  |  |  |  +----------------------------> Transfer block size\n\
# |   |   |   |  |  |  |  +-------------------------------> Retry interval\n\
# |   |   |   |  |  |  +----------------------------------> Max. errors\n\
# |   |   |   |  |  +-------------------------------------> Allowed transfers\n\
# |   |   |   |  +----------------------------------------> Proxy name\n\
# |   |   |   +-------------------------------------------> Host toggle\n\
# |   |   +-----------------------------------------------> Real hostname 2\n\
# |   +---------------------------------------------------> Real hostname 1\n\
# +-------------------------------------------------------> Alias hostname\n\
#\n\
# Or if you prefer another view of the above:\n\
#\n\
#   <Alias hostname>:<Real hostname 1>:<Real hostname 2>:<Host toggle>:\n\
#   <Proxy name>:<Allowed transfers>:<Max. errors>:<Retry interval>:\n\
#   <Transfer block size>:<Successful retries>:<File size offset>:\n\
#   <Transfer timeout>:<no bursts>:<host status>:<special flag>:\n\
#   <transfer rate limit>:<TTL>:<Socket send buffer>:<Socket receive buffer>:\n\
#   <dupcheck timeout>:<dupcheck flag>:<Keep connected>\n"
#else
#define HOST_CONFIG_TEXT_PART1 "#\n\
#                Host configuration file for the AFD\n\
#                ===================================\n\
#\n\
# There are 20 parameters that can be configured for each remote\n\
# host. They are:\n\
#\n\
# Keep connected          <----------------------------------------+\n\
# Socket receive buffer   <-------------------------------------+  |\n\
# Socket send buffer      <---------------------------------+   |  |\n\
#                                                           |   |  |\n\
# AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB:HS:SF:TRL:TTL:SSB:SRB:KC\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   |\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   +-> TTL\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   +-----> Transfer rate limit\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  |  +---------> Protocol options\n\
# |   |   |   |  |  |  |  |  |  |   |  |  |  +------------> Host status\n\
# |   |   |   |  |  |  |  |  |  |   |  |  +---------------> Number of no bursts\n\
# |   |   |   |  |  |  |  |  |  |   |  +------------------> Transfer timeout\n\
# |   |   |   |  |  |  |  |  |  |   +---------------------> File size offset\n\
# |   |   |   |  |  |  |  |  |  +-------------------------> Successful retries\n\
# |   |   |   |  |  |  |  |  +----------------------------> Transfer block size\n\
# |   |   |   |  |  |  |  +-------------------------------> Retry interval\n\
# |   |   |   |  |  |  +----------------------------------> Max. errors\n\
# |   |   |   |  |  +-------------------------------------> Allowed transfers\n\
# |   |   |   |  +----------------------------------------> Proxy name\n\
# |   |   |   +-------------------------------------------> Host toggle\n\
# |   |   +-----------------------------------------------> Real hostname 2\n\
# |   +---------------------------------------------------> Real hostname 1\n\
# +-------------------------------------------------------> Alias hostname\n\
#\n\
# Or if you prefer another view of the above:\n\
#\n\
#   <Alias hostname>:<Real hostname 1>:<Real hostname 2>:<Host toggle>:\n\
#   <Proxy name>:<Allowed transfers>:<Max. errors>:<Retry interval>:\n\
#   <Transfer block size>:<Successful retries>:<File size offset>:\n\
#   <Transfer timeout>:<no bursts>:<host status>:<special flag>:\n\
#   <transfer rate limit>:<TTL>:<Socket send buffer>:<Socket receive buffer>:\n\
#   <Keep connected>\n"
#endif

#define HOST_CONFIG_TEXT_PART2 "#\n\
# The meaning of each is outlined in more detail below:\n\
#\n\
# Alias hostname         - This is the host name that is being displayed\n\
#                          in the afd_ctrl window and is used in the log\n\
#                          files. It may only be 8 (MAX_HOSTNAME_LENGTH)\n\
#                          characters long.\n\
#                          DEFAULT: None (Empty)\n\
# Real hostname 1        - The real host name or IP number of the primary\n\
#                          host.\n\
# Real hostname 2        - The real host name or IP number of the secondary\n\
#                          host.\n\
# Host toggle            - Host switching information. This string holds the\n\
#                          toggling character to be displayed for the\n\
#                          primary and secondary host. The two characters\n\
#                          must be put in either curly brackets {} for\n\
#                          automatic host switching or square brackets []\n\
#                          host switching by the user.\n\
# Proxy name             - If the remote host can only be reached via a\n\
#                          proxy, specify the name of the proxy here.\n\
#                          DEFAULT: None (Empty)\n\
# Allowed transfers      - The maximum number of parallel transfers for\n\
#                          this host.\n\
#                          DEFAULT: 2\n\
# Max. errors            - If max. errors is reached the destination\n\
#                          identifier turns 'red'. If error retries\n\
#                          reaches twice max. errors the queue of this\n\
#                          host will be paused.\n\
# Retry interval         - If an error occurs, this is the delay (in\n\
#                          seconds) before another transfer is initiated.\n\
# Transfer block size    - The size of the blocks being used to send\n\
#                          files to the remote host (in Bytes).\n\
#                          DEFAULT: 1024\n\
# Successful retries     - This is only used when there is a secondary\n\
#                          host and automatic switch over is active.\n\
#                          It is the number of successful transfers to\n\
#                          the secondary host, before it tries to switch\n\
#                          back to the main host to see if it is alive\n\
#                          again.\n\
# File size offset       - When transmitting large files and the transfer\n\
#                          gets interrupted, the AFD can append a file\n\
#                          on the remote site. For this it needs to know\n\
#                          the file size on the remote site. And to get\n\
#                          the size it does a dir 'filename' at the remote\n\
#                          site. Due to different replies of the FTP\n\
#                          servers, the position of the file size is\n\
#                          needed. You can easily determine this value\n\
#                          simply doing an FTP to the remote site and\n\
#                          a dir and count the spaces to the file size.\n\
#                          For example:\n\
#\n\
#             -rw-r--r--   1 afd      mts-soft   14971 Jan  3 17:16\n\
#                       ^^^ ^   ^^^^^^        ^^^\n\
#                        |  |     |            |\n\
#                        |  |     |            |\n\
#                        1  2     3            4\n\
#\n\
#                          You may also put a -2 here, then AFD will try\n\
#                          to use the FTP SIZE command to get the size of\n\
#                          the remote file.\n\
#                          DEFAULT: -1 (Disabled)\n\
#\n\
# Transfer timeout       - The time how long the AFD should wait for a\n\
#                          reply from the remote site.\n\
#                          DEFAULT: 120\n\
# Number of no bursts    - This option applies only to FTP transfers.\n\
#                          A burst is when a new job is appended to a\n\
#                          transferring job. It can happen that jobs\n\
#                          get constantly appended while other jobs\n\
#                          with a higher priority have to wait. Therefor\n\
#                          it is possible to state the number of\n\
#                          connections that may NOT burst.\n\
#                          DEFAULT: 0\n\
# Host status            - This indicates the status of the host, currently\n\
#                          only bits number 1, 2, 3, 6 and 7 can be set. The\n\
#                          meaning is as follows (the values in brackets\n\
#                          are the integer values that may be set):\n\
#                          1 (1)   - If set transfer is stopped for this host.\n\
#                          2 (2)   - If set queue is stopped for this host.\n\
#                          3 (4)   - If set host is NOT in DIR_CONFIG.\n\
#                          5 (16)  - Error status offline.\n\
#                          6 (32)  - If set this host is disabled.\n\
#                          7 (64)  - If set and host switching is used\n\
#                                    this tells that host two is active.\n\
#                          DEFAULT: 0\n\
# Protocol options       - To set some protocol specific features for\n\
#                          this host. The following bits can be set (again\n\
#                          the values in bracket are the integer values\n\
#                          that can be set):\n\
#                          1 (1)   - FTP passive mode\n\
#                          2 (2)   - Set FTP idle time to transfer timeout\n\
#                          3 (4)   - Send STAT command to keep control\n\
#                                    connection alive.\n\
#                          4 (8)   - Combine RNFR and RNTO to one command.\n\
#                          5 (16)  - Do not do a cd, always use absolute path.\n\
#                          6 (32)  - Do not send TYPE I command.\n\
#                          7 (64)  - Use extended active or extended passive\n\
#                                    mode.\n\
#                          8 (128) - If set bursting is disabled.\n\
#                          9 (256) - If set FTP passive mode allows to be\n\
#                                    redirected to another address.\n\
#                          DEFAULT: 0\n\
# Transfer rate limit    - The maximum number of kilobytes that may be\n\
#                          transfered per second.\n\
#                          DEFAULT: 0 (Disabled)\n\
# TTL                    - The time-to-live for outgoing multicasts.\n\
# Socket send buffer     - How large the socket send buffer should be in\n\
#                          bytes. If this is zero it will leave it unchanged\n\
#                          ie. it will leave the system default.\n\
#                          DEFAULT: 0\n\
# Socket receive buffer  - How large the socket receive buffer should be in\n\
#                          bytes. If this is zero it will leave it unchanged\n\
#                          ie. it will leave the system default.\n\
#                          DEFAULT: 0\n"

#ifdef WITH_DUP_CHECK
#define HOST_CONFIG_TEXT_PART3 "# Duplicate check timeout- Check for duplicates if the value is bigger then 0.\n\
#                          The unit is seconds and is the time how long the\n\
#                          CRC is to be stored.\n\
#                          DEFAULT: 0 (Disabled)\n\
# Duplicate check flag   - This flag specifies how to determine the checksum,\n\
#                          which CRC to use and what action should be taken\n\
#                          when we find a duplicate. The bits have the\n\
#                          following meaning:\n\
#                          1 (1)        - Only do CRC checksum for filename.\n\
#                          2 (2)        - Only do CRC checksum for file content.\n\
#                          3 (4)        - Cecksum for filename and content.\n\
#                          4 (8)        - Checksum of filename without last suffix.\n\
#                          16(32768)    - Do a CRC32 checksum.\n\
#                          24(8388608)  - Delete the file.\n\
#                          25(16777216) - Store the duplicate file.\n\
#                          26(33554432) - Warn in SYSTEM_LOG.\n\
#                          DEFAULT: 0\n\
# Keep connected         - Keep connection for the given number of seconds\n\
#                          after all files have been transmitted or some\n\
#                          data was retrieved.\n\
#                          DEFAULT: 0\n\
#\n\
# Example entry:\n\
#  idefix:192.168.1.24:192.168.1.25:[12]::5:10:300:4096:10:-2:20:0:0:0:0:0:0:0:0:0:0\n\n"
#else
#define HOST_CONFIG_TEXT_PART3 "#  Keep connected         - Keep connection for the given number of seconds\n\
#                           after all files have been transmitted.\n\
#                           DEFAULT: 0\n\
#\n\
# Example entry:\n\
#  idefix:192.168.1.24:192.168.1.25:[12]::5:10:300:4096:10:-2:20:0:0:0:0:0:0:0:0\n\n"
#endif


/*++++++++++++++++++++++++ write_host_config() ++++++++++++++++++++++++++*/
time_t
write_host_config(int              no_of_hosts,
                  char             *host_config_file,
                  struct host_list *p_hl)
{
   int           i,
                 fd,
                 lock_fd;
   static time_t mod_time;
   size_t        length;
   char          line_buffer[MAX_HOSTNAME_LENGTH +  /* Alias hostname    */
                             (2 * MAX_REAL_HOSTNAME_LENGTH) +
                                                    /* Real hostname 1+2 */
                             MAX_TOGGLE_STR_LENGTH +/* Toggle string     */
                             MAX_PROXY_NAME_LENGTH +/* Proxy name        */
                             MAX_INT_LENGTH +       /* Allowed transfers */
                             MAX_INT_LENGTH +       /* Max. errors       */
                             MAX_INT_LENGTH +       /* Retry interval    */
                             MAX_INT_LENGTH +       /* Transfer blksize  */
                             MAX_INT_LENGTH +       /* Succ. retries     */
                             MAX_INT_LENGTH +       /* File size offset  */
                             MAX_INT_LENGTH +       /* Transfer timeout  */
                             MAX_INT_LENGTH +       /* No. of no bursts  */
                             MAX_INT_LENGTH +       /* Host status       */
                             MAX_INT_LENGTH +       /* Protocol options  */
                             MAX_INT_LENGTH +       /* Trnsfer rate limit*/
                             MAX_INT_LENGTH +       /* TTL               */
                             MAX_INT_LENGTH +       /* Socket snd buffer */
                             MAX_INT_LENGTH +       /* Socket rcv buffer */
#ifdef WITH_DUP_CHECK
                             MAX_LONG_LENGTH +      /* Dupcheck timeout  */
                             MAX_INT_LENGTH +       /* Dupcheck flag     */
#endif
                             MAX_INT_LENGTH +       /* Keep connected    */
#ifdef WITH_DUP_CHECK
                             21 +                   /* Separator signs   */
#else
                             19 +                   /* Separator signs   */
#endif
                             2],                    /* \n and \0         */
                 *new_name,
                 *ptr;
   struct stat   stat_buf;

   if ((lock_fd = lock_file(host_config_file, ON)) == INCORRECT)
   {
      exit(INCORRECT);
   }
   length = strlen(host_config_file);
   if ((new_name = malloc(length + 2)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not malloc() %d bytes : %s",
                 length + 2, strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy(new_name, host_config_file, length + 1);
   ptr = new_name + length;
   while ((*ptr != '/') && (ptr > new_name))
   {
      ptr--;
   }
   if (*ptr == '/')
   {
      char *copy_ptr;

      ptr++;
      *ptr = '.';
      copy_ptr = host_config_file + (ptr - new_name);
      ptr++;
      do
      {
         *ptr = *copy_ptr;
         ptr++; copy_ptr++;
      } while (*copy_ptr != '\0');
      *ptr = '\0';
   }

   if ((fd = open(new_name, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                  (S_IRUSR | S_IWUSR))) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not open() %s : %s", new_name, strerror(errno));
      exit(INCORRECT);
   }

   /* Write introduction comment. */
   length = strlen(HOST_CONFIG_TEXT_PART1);
   if (write(fd, HOST_CONFIG_TEXT_PART1, length) != length)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   length = strlen(HOST_CONFIG_TEXT_PART2);
   if (write(fd, HOST_CONFIG_TEXT_PART2, length) != length)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   length = strlen(HOST_CONFIG_TEXT_PART3);
   if (write(fd, HOST_CONFIG_TEXT_PART3, length) != length)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Commit data line by line. */
   for (i = 0; i < no_of_hosts; i++)
   {
      length = sprintf(line_buffer,
#ifdef WITH_DUP_CHECK
                       "%s:%s:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%u:%d:%d:%u:%u:%ld:%u:%u\n",
#else
                       "%s:%s:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%u:%d:%d:%u:%u:%u\n",
#endif
                       p_hl[i].host_alias,
                       p_hl[i].real_hostname[0],
                       p_hl[i].real_hostname[1],
                       p_hl[i].host_toggle_str,
                       p_hl[i].proxy_name,
                       p_hl[i].allowed_transfers,
                       p_hl[i].max_errors,
                       p_hl[i].retry_interval,
                       p_hl[i].transfer_blksize,
                       p_hl[i].successful_retries,
                       (int)p_hl[i].file_size_offset,
                       (int)p_hl[i].transfer_timeout,
#ifdef WHEN_WE_USE_FOR_THIS
                       (int)p_hl[i].number_of_no_bursts,
#else
                       0,
#endif
                       (int)p_hl[i].host_status,
                       p_hl[i].protocol_options,
                       p_hl[i].transfer_rate_limit,
                       p_hl[i].ttl,
                       p_hl[i].socksnd_bufsize,
                       p_hl[i].sockrcv_bufsize,
#ifdef WITH_DUP_CHECK
                       p_hl[i].dup_check_timeout,
                       p_hl[i].dup_check_flag,
#endif
                       p_hl[i].keep_connected);

      if (write(fd, line_buffer, length) != length)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
         exit(INCORRECT);
      }
   }

#ifdef _CYGWIN
   if (fsync(fd) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to fsync() %s : %s", new_name, strerror(errno));
   }
#endif
   if (close(fd) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }
   if ((unlink(host_config_file) == -1) && (errno != ENOENT))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to unlink() %s : %s",
                 host_config_file, strerror(errno));
   }
   if (rename(new_name, host_config_file) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                "Failed to rename() %s to %s : %s",
                new_name, host_config_file, strerror(errno));
      exit(INCORRECT);
   }
   if (lock_fd != LOCKFILE_NOT_THERE)
   {
      if (close(lock_fd) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to close() %s : %s",
                    host_config_file, strerror(errno));
      }
   }
   free(new_name);

   if (stat(host_config_file, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to stat() %s : %s", host_config_file, strerror(errno));
      exit(INCORRECT);
   }
#ifdef GROUP_CAN_WRITE
   if (stat_buf.st_mode != (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))
   {
      if (chmod(host_config_file,
                (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Can't change mode to %o for file %s : %s",
                    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), host_config_file,
                    strerror(errno));
      }
   }
#endif
   mod_time = stat_buf.st_mtime;

   return(mod_time);
}
