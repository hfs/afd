/*
 *  mondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2006 Deutscher Wetterdienst (DWD),
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

#ifndef __mondefs_h
#define __mondefs_h

#define MON_WD_ENV_NAME          "MON_WORK_DIR"  /* Environment variable */
                                                 /* for working          */
                                                 /* directory.           */

#define MAX_AFDNAME_LENGTH       12
#define MAX_REMOTE_CMD_LENGTH    10
#define MAX_REAL_HOSTNAME_LENGTH 40
#define MAX_RET_MSG_LENGTH       4096  /* How much data is buffered from */
                                       /* the remote TCP port.           */
#define MAX_VERSION_LENGTH       40
#define MAX_CONVERT_USERNAME     5

#define MON_CONFIG_FILE          "/MON_CONFIG"
#define AFD_MON_CONFIG_FILE      "/AFD_MON_CONFIG"

#define MON_ACTIVE_FILE          "/AFD_MON_ACTIVE"
#define MON_STATUS_FILE          "/mon_status"
#define MON_STATUS_FILE_ALL      "/mon_status.*"
#define AFD_MON_STATUS_FILE      "/afd_mon.status"
#define MSA_ID_FILE              "/msa.id"
#define AHL_FILE_NAME            "/afd_host_list."
#define AHL_FILE_NAME_ALL        "/afd_host_list.*"
#define MON_CMD_FIFO             "/afd_mon_cmd.fifo"
#define MON_RESP_FIFO            "/afd_mon_resp.fifo"
#define MON_PROBE_ONLY_FIFO      "/afd_mon_probe_only.fifo"
#define RETRY_MON_FIFO           "/retry_mon.fifo."
#define RETRY_MON_FIFO_ALL       "/retry_mon.fifo.*"
#define MON_SYS_LOG_FIFO         "/mon_sys_log.fifo"

#define STORAGE_TIME             7    /* Time in days to store top       */
                                      /* values for transfer rate and    */
                                      /* file rate.                      */
#define DEFAULT_POLL_INTERVAL    5
#define DEFAULT_OPTION_ENTRY     0
#define DEFAULT_REMOTE_CMD       "rsh"
#define DEFAULT_CONNECT_TIME     0
#define DEFAULT_DISCONNECT_TIME  0
#define RETRY_INTERVAL           60   /* Interval at which the mon       */
                                      /* process will try to reconnect   */
                                      /* after an error occured.         */

/* Error return values for mon process. */
#define SYNTAX_ERROR             1
#define SELECT_ERROR             2

#define AFDD_SHUTTING_DOWN       24

/* Flags for the options field in struct mon_status_area. */
#define COMPRESS_FLAG            1
#define MINUS_Y_FLAG             2
#define DONT_USE_FULL_PATH_FLAG  4

/* Different toggling status for switching AFD's. */
#define NO_SWITCHING             0
#define AUTO_SWITCHING           1
#define USER_SWITCHING           2

/* Structure to hold all host alias names and there real names. */
struct afd_host_list
       {
          char          host_alias[MAX_HOSTNAME_LENGTH + 1];
          char          real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
          unsigned char error_history[ERROR_HISTORY_LENGTH];
       };

/* Structure to hold data from AFD_MON_CONFIG file. */
struct mon_list
       {
          char          convert_username[MAX_CONVERT_USERNAME][2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call      */
                                                 /* remote programms.    */
          char          afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the      */
                                                 /* remote AFD shown in  */
                                                 /* the dialog.          */
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host name.*/
          char          rcmd[MAX_REMOTE_CMD_LENGTH + 1];
                                                 /* The command to       */
                                                 /* execute on remote    */
                                                 /* host, either rsh or  */
                                                 /* ssh.                 */
          int           port[2];                 /* Port number of       */
                                                 /* remote AFDD.         */
          int           poll_interval;           /* The interval in      */
                                                 /* seconds, getting     */
                                                 /* data from AFDD.      */
          unsigned int  connect_time;            /* How long should we   */
                                                 /* stay connected when  */
                                                 /* disconnect.          */
          unsigned int  disconnect_time;         /* How long should we   */
                                                 /* stay disconnected.   */
          unsigned int  options;                 /* Special options with */
                                                 /* following meaning:   */
                                                 /*+------+-------------+*/
                                                 /*|Bit(s)|   Meaning   |*/
                                                 /*+------+-------------+*/
                                                 /*| 4-32 | Not used.   |*/
                                                 /*| 3    | Don't use   |*/
                                                 /*|      | full path.  |*/
                                                 /*| 2    | Use -Y      |*/
                                                 /*|      | instead of  |*/
                                                 /*|      | -X (ssh).   |*/
                                                 /*| 1    | compression |*/
                                                 /*|      | for ssh     |*/
                                                 /*+------+-------------+*/
          unsigned char afd_switching;           /* Flag to specify if   */
                                                 /* automatic AFD switch-*/
                                                 /* ing (AUTO_SWITCHING),*/
                                                 /* user switching       */
                                                 /* (USER_SWITCHING) or  */
                                                 /* no switching         */
                                                 /* (NO_SWITCHING) is    */
                                                 /* being used.          */
       };

/* Structure to hold status of afd_mon and the process it starts. */
struct afd_mon_status
       {
          time_t       start_time;
          signed char  afd_mon;
          signed char  mon_sys_log;
          signed char  mon_log;
          unsigned int mon_sys_log_ec;  /* Mon system log entry counter. */
          char         mon_sys_log_fifo[LOG_FIFO_SIZE + 1];
          unsigned int mon_log_ec;      /* Mon log entry counter.        */
          char         mon_log_fifo[LOG_FIFO_SIZE + 1];
       };

/* Structure holding all relevant data of one remote AFD. */
/*-----------------------------------------------------------------------*
 * For the MSA these bytes are used to store information about the
 * AFD_MON with the following meaning (assuming SIZEOF_INT is 4):
 *     Byte  | Type          | Description
 *    -------+---------------+---------------------------------------
 *     1 - 4 | int           | The number of hosts served by the AFD.
 *           |               | If this FSA in no longer in use there
 *           |               | will be a -1 here.
 *    -------+---------------+---------------------------------------
 *     5 - 7 | unsigned char | Not used.
 *    -------+---------------+---------------------------------------
 *       8   | unsigned char | Version of this structure.
 *    -------+---------------+---------------------------------------
 *     9 - 16|               | Not used.
 *-----------------------------------------------------------------------*/
#define CURRENT_MSA_VERSION 0
struct mon_status_area
       {
          char          r_work_dir[MAX_PATH_LENGTH];
          char          convert_username[MAX_CONVERT_USERNAME][2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call      */
                                                 /* remote programms.    */
          char          afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the      */
                                                 /* remote AFD shown in  */
                                                 /* the dialog.          */
          char          hostname[2][MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host name.*/
          char          rcmd[MAX_REMOTE_CMD_LENGTH];
                                                 /* The command to       */
                                                 /* execute on remote    */
                                                 /* host, either rsh or  */
                                                 /* ssh.                 */
          char          afd_version[MAX_VERSION_LENGTH];
                                                 /* Version of remote    */
                                                 /* AFD.                 */
          int           port[2];                 /* Port number of       */
                                                 /* remote AFDD.         */
          int           poll_interval;           /* The interval in      */
                                                 /* seconds, getting     */
                                                 /* data from AFDD.      */
          unsigned int  connect_time;            /* How long should we   */
                                                 /* stay connected when  */
                                                 /* disconnect.          */
          unsigned int  disconnect_time;         /* How long should we   */
                                                 /* stay disconnected.   */
          char          amg;                     /* Status of AMG.       */
          char          fd;                      /* Status of FD.        */
          char          archive_watch;           /* Status of AW.        */
          int           jobs_in_queue;           /* The number of jobs   */
                                                 /* still to be done     */
                                                 /* by the FD.           */
          int           no_of_transfers;         /* The number of        */
                                                 /* active transfers.    */
          int           top_no_of_transfers[STORAGE_TIME];
                                                 /* Maximum number of    */
                                                 /* sf_xxx/gf_xxx        */
                                                 /* process on a per     */
                                                 /* day basis.           */
          time_t        top_not_time;            /* Time when the        */
                                                 /* this maximum was     */
                                                 /* reached.             */
          int           max_connections;         /* Maximum number of    */
                                                 /* sf_xxx/gf_xxx        */
                                                 /* process.             */
          unsigned int  sys_log_ec;              /* System log entry     */
                                                 /* counter.             */
          char          sys_log_fifo[LOG_FIFO_SIZE + 1];
                                                 /* Activity in the      */
                                                 /* system log.          */
          char          log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];
                                                 /* All log activities   */
                                                 /* the last 48 hours.   */
          int           host_error_counter;      /* Number of host       */
                                                 /* names that are red.  */
          int           no_of_hosts;             /* The number of hosts  */
          unsigned int  no_of_jobs;              /* The number of jobs   */
                                                 /* configured.          */
          unsigned int  options;                 /* Special options with */
                                                 /* following meaning:   */
                                                 /*+------+-------------+*/
                                                 /*|Bit(s)|   Meaning   |*/
                                                 /*+------+-------------+*/
                                                 /*| 4-32 | Not used.   |*/
                                                 /*| 3    | Don't use   |*/
                                                 /*|      | full path.  |*/
                                                 /*| 2    | Use -Y      |*/
                                                 /*|      | instead of  |*/
                                                 /*|      | -X (ssh).   |*/
                                                 /*| 1    | compression |*/
                                                 /*|      | for ssh.    |*/
                                                 /*+------+-------------+*/
                                                 /* that this AFD has.   */
          unsigned int  fc;                      /* Files still to be    */
                                                 /* send.                */
          unsigned int  fs;                      /* File size still to   */
                                                 /* be send.             */
          unsigned int  tr;                      /* Transfer rate.       */
          unsigned int  top_tr[STORAGE_TIME];    /* Maximum transfer     */
                                                 /* rate on a per day    */
                                                 /* basis.               */
          time_t        top_tr_time;             /* Time when this       */
                                                 /* maximum was reached. */
          unsigned int  fr;                      /* File rate.           */
          unsigned int  top_fr[STORAGE_TIME];    /* Maximum file rate    */
                                                 /* on a per day basis.  */
          time_t        top_fr_time;             /* Time when this       */
                                                 /* maximum was reached. */
          unsigned int  ec;                      /* Error counter.       */
          time_t        last_data_time;          /* Last time data was   */
                                                 /* received from        */
                                                 /* remote AFD.          */
          char          connect_status;          /* Connect status to    */
                                                 /* remote AFD.          */
          unsigned char afd_switching;           /* Flag to specify if   */
                                                 /* automatic AFD switch-*/
                                                 /* ing (AUTO_SWITCHING),*/
                                                 /* user switching       */
                                                 /* (USER_SWITCHING) or  */
                                                 /* no switching         */
                                                 /* (NO_SWITCHING) is    */
                                                 /* being used.          */
          char          afd_toggle;              /* If there is more then*/
                                                 /* one AFD, this tells  */
                                                 /* which ist the active */
                                                 /* one (HOST_ONE or     */
                                                 /* HOST_TWO).           */
       };

struct process_list
       {
          char         afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the     */
                                                 /* remote AFD shown in */
                                                 /* the dialog.         */
          pid_t        pid;                      /* Process ID of       */
                                                 /* process monitoring  */
                                                 /* this AFD.           */
          time_t       start_time;               /* Time when process   */
                                                 /* was started.        */
          int          number_of_restarts;       /* Number of times     */
                                                 /* this process has    */
                                                 /* restarted.          */
       };

/* Function prototypes */
int   attach_afd_mon_status(void),
      check_mon(long),
      detach_afd_mon_status(void),
      init_fifos_mon(void),
      read_msg(void),
      tcp_cmd(char *),
      tcp_connect(char *, int),
      tcp_quit(void);
pid_t start_process(char *, int);
void  create_msa(void),
      eval_afd_mon_db(struct mon_list **),
      shutdown_mon(int, char *),
      start_all(void),
      stop_process(int, int);

#endif /* __mondefs_h */
