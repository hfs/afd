/*
 *  mondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
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
#define MAX_REAL_HOSTNAME_LENGTH 40
#define MAX_RET_MSG_LENGTH       4096  /* How much data is buffered from */
                                       /* the remote TCP port.           */
#define MAX_VERSION_LENGTH       40

#define MON_CONFIG_FILE          "/MON_CONFIG"
#define AFD_MON_CONFIG_FILE      "/AFD_MON_CONFIG"
#define MON_ACTIVE_FILE          "/AFD_MON_ACTIVE"
#define MON_STATUS_FILE          "/mon_status"
#define MSA_ID_FILE              "/msa.id"
#define MON_CMD_FIFO             "/afd_mon_cmd.fifo"
#define MON_RESP_FIFO            "/afd_mon_resp.fifo"
#define MON_PROBE_ONLY_FIFO      "/afd_mon_probe_only.fifo"
#define AHL_FILE_NAME            "/afd_host_list."

#define STORAGE_TIME             7    /* Time in days to store top       */
                                      /* values for transfer rate and    */
                                      /* file rate.                      */
#define DEFAULT_POLL_INTERVAL    5
#define RETRY_INTERVAL           60   /* Interval at which the mon       */
                                      /* process will try to reconnect   */
                                      /* after an error occured.         */

/* Error return values for mon process. */
#define SYNTAX_ERROR             1
#define SELECT_ERROR             2

#define AFDD_SHUTTING_DOWN       24

/* Structure to hold all host alias names and there real names. */
struct afd_host_list
       {
          char host_alias[MAX_HOSTNAME_LENGTH + 1];
          char real_hostname[2][MAX_REAL_HOSTNAME_LENGTH];
       };

/* Structure to hold data from AFD_MON_CONFIG file. */
struct mon_list
       {
          char         convert_username[2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call     */
                                                 /* remote programms.   */
          char         afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the     */
                                                 /* remote AFD shown in */
                                                 /* the dialog.         */
          char         hostname[MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host     */
                                                 /* name.               */
          int          port;                     /* Port number of      */
                                                 /* remote AFDD.        */
          int          poll_interval;            /* The interval in     */
                                                 /* seconds, getting    */
                                                 /* data from AFDD.     */
       };

/* Structure holding all relevant data of one remote AFD. */
struct mon_status_area
       {
          char         r_work_dir[MAX_PATH_LENGTH];
          char         convert_username[2][MAX_USER_NAME_LENGTH];
                                                 /* Command to call     */
                                                 /* remote programms.   */
          char         afd_alias[MAX_AFDNAME_LENGTH + 1];
                                                 /* The name of the     */
                                                 /* remote AFD shown in */
                                                 /* the dialog.         */
          char         hostname[MAX_REAL_HOSTNAME_LENGTH];
                                                 /* The remote host     */
                                                 /* name.               */
          char         afd_version[MAX_VERSION_LENGTH];
                                                 /* Version of remote   */
                                                 /* AFD.                */
          int          port;                     /* Port number of      */
                                                 /* remote AFDD.        */
          int          poll_interval;            /* The interval in     */
                                                 /* seconds, getting    */
                                                 /* data from AFDD.     */
          char         amg;                      /* Status of AMG.      */
          char         fd;                       /* Status of FD.       */
          char         archive_watch;            /* Status of AW.       */
          int          jobs_in_queue;            /* The number of jobs  */
                                                 /* still to be done    */
                                                 /* by the FD.          */
          int          no_of_transfers;          /* The number of       */
                                                 /* active transfers.   */
          int          top_no_of_transfers[STORAGE_TIME];
                                                 /* Maximum number of   */
                                                 /* sf_xxx/gf_xxx       */
                                                 /* process on a per    */
                                                 /* day basis.          */
          time_t       top_not_time;             /* Time when the       */
                                                 /* this maximum was    */
                                                 /* reached.            */
          int          max_connections;          /* Maximum number of   */
                                                 /* sf_xxx/gf_xxx       */
                                                 /* process.            */
          unsigned int sys_log_ec;               /* System log entry    */
                                                 /* counter.            */
          char         sys_log_fifo[LOG_FIFO_SIZE + 1];
                                                 /* Activity in the     */
                                                 /* system log.         */
          char         log_history[NO_OF_LOG_HISTORY][MAX_LOG_HISTORY];
                                                 /* All log activities  */
                                                 /* the last 24 hours.  */
          int          host_error_counter;       /* Number of host      */
                                                 /* names that are red. */
          int          no_of_hosts;              /* The number of hosts */
                                                 /* that this AFD has.  */
          unsigned int fc;                       /* Files still to be   */
                                                 /* send.               */
          unsigned int fs;                       /* File size still to  */
                                                 /* be send.            */
          unsigned int tr;                       /* Transfer rate.      */
          unsigned int top_tr[STORAGE_TIME];     /* Maximum transfer    */
                                                 /* rate on a per day   */
                                                 /* basis.              */
          time_t       top_tr_time;              /* Time when this      */
                                                 /* maximum was reached.*/
          unsigned int fr;                       /* File rate.          */
          unsigned int top_fr[STORAGE_TIME];     /* Maximum file rate   */
                                                 /* on a per day basis. */
          time_t       top_fr_time;              /* Time when this      */
                                                 /* maximum was reached.*/
          unsigned int ec;                       /* Error counter.      */
          time_t       last_data_time;           /* Last time data was  */
                                                 /* received from       */
                                                 /* remote AFD.         */
          char         connect_status;           /* Connect status to   */
                                                 /* remote AFD.         */
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
int   check_mon(long),
      init_fifos_mon(void),
      read_msg(void),
      tcp_cmd(char *),
      tcp_connect(char *, int),
      tcp_quit(void);
pid_t start_process(char *, int);
void  create_msa(void),
      eval_afd_mon_db(struct mon_list **),
      shutdown_mon(int),
      start_all(void),
      stop_process(int, int);

#endif /* __mondefs_h */
