/* config.h.  Generated by configure.  */
/* ac-tools/config.h.in.  Generated from configure.in by autoheader.  */

/* With afd_ctrl dialog checking for main AFD process */
#define AFD_CTRL_PROC_CHECK 

/* With reusing FTP data port */
/* #undef FTP_REUSE_DATA_PORT */

/* Allow group write access */
#define GROUP_CAN_WRITE 

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <arpa/telnet.h> header file. */
#define HAVE_ARPA_TELNET_H 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* With eaccess support */
/* #undef HAVE_EACCESS */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `efence' library (-lefence). */
/* #undef HAVE_LIBEFENCE */

/* With -lXp support */
#define HAVE_LIB_XP 

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have the `mkfifo' function. */
#define HAVE_MKFIFO 1

/* Define to 1 if you have the `mktime' function. */
#define HAVE_MKTIME 1

/* With mmap support */
#define HAVE_MMAP 

/* Define this if you have Motif */
#define HAVE_MOTIF 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/in_systm.h> header file. */
#define HAVE_NETINET_IN_SYSTM_H 1

/* Define to 1 if you have the <netinet/ip.h> header file. */
#define HAVE_NETINET_IP_H 1

/* Define to 1 if you have the `rmdir' function. */
#define HAVE_RMDIR 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `seteuid' function. */
#define HAVE_SETEUID 1

/* Define to 1 if you have the `setreuid' function. */
#define HAVE_SETREUID 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strtod' function. */
#define HAVE_STRTOD 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if `utime(file, NULL)' sets file's timestamp to the present. */
#define HAVE_UTIME_NULL 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* With debug messages of locking */
/* #undef LOCK_DEBUG */

/* Name of package */
#define PACKAGE "afd"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "Holger.Kiehl@dwd.de"

/* Define to the full name of this package. */
#define PACKAGE_NAME "afd"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "afd 1.3.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "afd"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.3.1"

/* Define to 1 if the C compiler supports function prototypes. */
#define PROTOTYPES 1

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if the `setvbuf' function takes the buffering type as its
   second argument and the buffer pointer as the third, as on System V before
   release 3. */
/* #undef SETVBUF_REVERSED */

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 8

/* The size of a `pid_t', as computed by sizeof. */
#define SIZEOF_PID_T 4

/* The size of a `ssize_t', as computed by sizeof. */
#define SIZEOF_SSIZE_T 4

/* The size of a `time_t', as computed by sizeof. */
#define SIZEOF_TIME_T 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "1.3.1"

/* With automatic creation of configuration */
#define WITH_AUTO_CONFIG 

/* Use ctrl key as accelerator */
/* #undef WITH_CTRL_ACCELERATOR */

/* With duplicate check support */
#define WITH_DUP_CHECK 

/* With editres support */
/* #undef WITH_EDITRES */

/* With eumetsat header support */
/* #undef WITH_EUMETSAT_HEADERS */

/* With MS FTP error workaround */
/* #undef WITH_MS_ERROR_WORKAROUND */

/* With password in message */
/* #undef WITH_PASSWD_IN_MSG */

/* With FSS-type ready files */
/* #undef WITH_READY_FILES */

/* Some programs with setuid */
/* #undef WITH_SETUID */

/* Set some programms setuid. */
/* #undef WITH_SETUID_PROGS */

/* with TLS/SSL support */
#define WITH_SSL 

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* With delete log support */
#define _DELETE_LOG 

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* With input log support */
#define _INPUT_LOG 

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* With output log support */
#define _OUTPUT_LOG 

/* With production log support */
#define _PRODUCTION_LOG 

/* With verify FSA support */
#define _VERIFY_FSA 

/* With MAP support */
/* #undef _WITH_MAP_SUPPORT */

/* With server support */
/* #undef _WITH_SERVER_SUPPORT */

/* With WMO socket support */
#define _WITH_WMO_SUPPORT 

/* Define like PROTOTYPES; this can be used by system headers. */
#define __PROTOTYPES 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Set argument of getsockname and other functions */
#define my_socklen_t socklen_t

/* Define to `long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */

/* Set socklen_t to int if not set */
/* #undef socklen_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */
