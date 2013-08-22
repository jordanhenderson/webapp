/* Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */
#if defined(_MSC_VER) 
#ifndef MY_CONFIG_H
#define MY_CONFIG_H
#define DOT_FRM_VERSION 6
/* Headers we may want to use. */
/* #undef _GNU_SOURCE */
/* #undef HAVE_ALLOCA_H */
/* #undef HAVE_ARPA_INET_H */
/* #undef HAVE_ASM_MSR_H */
/* #undef HAVE_ASM_TERMBITS_H */
/* #undef HAVE_CRYPT_H */
/* #undef HAVE_CURSES_H */
/* #undef HAVE_CXXABI_H */
/* #undef HAVE_NCURSES_H */
/* #undef HAVE_NDIR_H */
/* #undef HAVE_DIRENT_H */
/* #undef HAVE_DLFCN_H */
/* #undef HAVE_EXECINFO_H */
#define HAVE_FCNTL_H 1
/* #undef HAVE_FENV_H */
/* #undef HAVE_FNMATCH_H */
/* #undef HAVE_FPU_CONTROL_H */
/* #undef HAVE_GRP_H */
/* #undef HAVE_IA64INTRIN_H */
/* #undef HAVE_IEEEFP_H */
/* #undef HAVE_INTTYPES_H */
#define HAVE_MALLOC_H 1
/* #undef HAVE_NETINET_IN_H */
/* #undef HAVE_PATHS_H */
/* #undef HAVE_POLL_H */
/* #undef HAVE_PWD_H */
/* #undef HAVE_SCHED_H */
/* #undef HAVE_SELECT_H */
/* #undef HAVE_SOLARIS_LARGE_PAGES */
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
/* #undef HAVE_STRINGS_H */
/* #undef HAVE_STDINT_H */
/* #undef HAVE_SYNCH_H */
/* #undef HAVE_SYSENT_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_CDEFS_H */
/* #undef HAVE_SYS_FPU_H */
/* #undef HAVE_SYS_IOCTL_H */
/* #undef HAVE_SYS_IPC_H */
/* #undef HAVE_SYS_MALLOC_H */
/* #undef HAVE_SYS_MMAN_H */
/* #undef HAVE_SYS_NDIR_H */
/* #undef HAVE_SYS_PRCTL_H */
/* #undef HAVE_SYS_RESOURCE_H */
/* #undef HAVE_SYS_SELECT_H */
/* #undef HAVE_SYS_SHM_H */
/* #undef HAVE_SYS_SOCKET_H */
#define HAVE_SYS_STAT_H 1
/* #undef HAVE_SYS_TIMES_H */
/* #undef HAVE_SYS_TIME_H */
#define HAVE_SYS_TYPES_H 1
/* #undef HAVE_SYS_UN_H */
/* #undef HAVE_SYS_VADVISE_H */
/* #undef HAVE_TERM_H */
/* #undef HAVE_TERMBITS_H */
/* #undef HAVE_TERMIOS_H */
/* #undef HAVE_TERMIO_H */
/* #undef HAVE_TERMCAP_H */
/* #undef HAVE_UNISTD_H */
/* #undef HAVE_UTIME_H */
/* #undef HAVE_VIS_H */
#define HAVE_SYS_UTIME_H 1
/* #undef HAVE_SYS_WAIT_H */
/* #undef HAVE_SYS_PARAM_H */

/* Libraries */
/* #undef HAVE_LIBPTHREAD */
/* #undef HAVE_LIBM */
/* #undef HAVE_LIBDL */
/* #undef HAVE_LIBRT */
/* #undef HAVE_LIBSOCKET */
/* #undef HAVE_LIBNSL */
/* #undef HAVE_LIBCRYPT */
/* #undef HAVE_LIBWRAP */

/* Readline */
/* #undef HAVE_HIST_ENTRY */
/* #undef USE_LIBEDIT_INTERFACE */

/* #undef FIONREAD_IN_SYS_IOCTL */
/* #undef GWINSZ_IN_SYS_IOCTL */
/* #undef FIONREAD_IN_SYS_FILIO */

/* Functions we may want to use. */
#define HAVE_ALIGNED_MALLOC 1
/* #undef HAVE_ALARM */
/* #undef HAVE_INDEX */
/* #undef HAVE_CLOCK_GETTIME */
/* #undef HAVE_CRYPT */
/* #undef HAVE_CUSERID */
/* #undef HAVE_DIRECTIO */
/* #undef HAVE_DLERROR */
/* #undef HAVE_DLOPEN */
/* #undef HAVE_FCHMOD */
/* #undef HAVE_FCNTL */
/* #undef HAVE_FDATASYNC */
/* #undef HAVE_FESETROUND */
#define HAVE_FINITE 1
/* #undef HAVE_FP_EXCEPT */
/* #undef HAVE_FSEEKO */
/* #undef HAVE_FSYNC */
#define HAVE_GETADDRINFO 1
#define HAVE_GETCWD 1
/* #undef HAVE_GETHOSTBYADDR_R */
/* #undef HAVE_GETHRTIME */
/* #undef HAVE_GETNAMEINFO */
/* #undef HAVE_GETPAGESIZE */
/* #undef HAVE_GETPASS */
/* #undef HAVE_GETPASSPHRASE */
/* #undef HAVE_GETPWNAM */
/* #undef HAVE_GETPWUID */
/* #undef HAVE_GETRLIMIT */
/* #undef HAVE_GETRUSAGE */
/* #undef HAVE_GETTIMEOFDAY */
/* #undef HAVE_GETWD */
/* #undef HAVE_GMTIME_R */
/* #undef gmtime_r */
/* #undef HAVE_INITGROUPS */
/* #undef HAVE_ISSETUGID */
/* #undef HAVE_GETUID */
/* #undef HAVE_GETEUID */
/* #undef HAVE_GETGID */
/* #undef HAVE_GETEGID */
/* #undef HAVE_ISINF */
/* #undef HAVE_LARGE_PAGE_OPTION */
/* #undef HAVE_LRAND48 */
/* #undef HAVE_LOCALTIME_R */
/* #undef HAVE_LOG2 */
/* #undef HAVE_LSTAT */
/* #undef HAVE_MEMALIGN */
/* #undef HAVE_MLOCK */
/* #undef HAVE_NL_LANGINFO */
/* #undef HAVE_MADVISE */
/* #undef HAVE_DECL_MADVISE */
/* #undef HAVE_DECL_TGOTO */
/* #undef HAVE_DECL_MHA_MAPSIZE_VA */
/* #undef HAVE_MALLOC_INFO */
/* #undef HAVE_MKSTEMP */
/* #undef HAVE_MLOCKALL */
/* #undef HAVE_MMAP */
/* #undef HAVE_MMAP64 */
/* #undef HAVE_POLL */
/* #undef HAVE_POSIX_FALLOCATE */
/* #undef HAVE_POSIX_MEMALIGN */
/* #undef HAVE_PREAD */
/* #undef HAVE_PAUSE_INSTRUCTION */
/* #undef HAVE_FAKE_PAUSE_INSTRUCTION */
/* #undef HAVE_RDTSCLL */
/* #undef HAVE_PTHREAD_ATTR_GETSTACKSIZE */
/* #undef HAVE_PTHREAD_ATTR_SETSTACKSIZE */
/* #undef HAVE_PTHREAD_CONDATTR_SETCLOCK */
/* #undef HAVE_PTHREAD_KEY_DELETE */
/* #undef HAVE_PTHREAD_RWLOCK_RDLOCK */
/* #undef HAVE_PTHREAD_SETSCHEDPARAM */
/* #undef HAVE_PTHREAD_SIGMASK */
/* #undef HAVE_PTHREAD_YIELD_NP */
/* #undef HAVE_PTHREAD_YIELD_ZERO_ARG */
/* #undef PTHREAD_ONCE_INITIALIZER */
/* #undef HAVE_READDIR_R */
/* #undef HAVE_READLINK */
/* #undef HAVE_REALPATH */
/* #undef HAVE_RINT */
/* #undef HAVE_RWLOCK_INIT */
/* #undef HAVE_SCHED_YIELD */
#define HAVE_SELECT 1
/* #undef HAVE_SETFD */
/* #undef HAVE_SETENV */
/* #undef HAVE_SIGSET */
/* #undef HAVE_SIGACTION */
/* #undef HAVE_SLEEP */
/* #undef HAVE_STPCPY */
/* #undef HAVE_STRSIGNAL */
/* #undef HAVE_STRLCPY */
/* #undef HAVE_STRLCAT */
/* #undef HAVE_FGETLN */
#define HAVE_STRNLEN 1
/* #undef HAVE_STRSEP */
#define HAVE_STRTOK_R 1
#define HAVE_STRTOLL 1
#define HAVE_TELL 1
#define HAVE_TEMPNAM 1
/* #undef HAVE_THR_SETCONCURRENCY */
/* #undef HAVE_THR_YIELD */
/* #undef HAVE_TIMES */
/* #undef HAVE_VASPRINTF */
/* #undef HAVE_FTRUNCATE */
#define HAVE_TZNAME 1
/* Symbols we may use */
/* used by stacktrace functions */
/* #undef HAVE_BSS_START */
/* #undef HAVE_BACKTRACE */
/* #undef HAVE_BACKTRACE_SYMBOLS */
/* #undef HAVE_BACKTRACE_SYMBOLS_FD */
/* #undef HAVE_PRINTSTACK */
/* #undef HAVE_STRUCT_SOCKADDR_IN6 */
/* #undef HAVE_STRUCT_IN6_ADDR */
/* #undef HAVE_NETINET_IN6_H */
#define HAVE_IPV6 1
/* #undef ss_family */
/* #undef HAVE_SOCKADDR_IN_SIN_LEN */
/* #undef HAVE_SOCKADDR_IN6_SIN6_LEN */

/* #undef DNS_USE_CPU_CLOCK_FOR_ID */
/* #undef HAVE_EPOLL */
/* #undef HAVE_EVENT_PORTS */
/* #undef HAVE_INET_NTOP */
/* #undef HAVE_WORKING_KQUEUE */
/* #undef HAVE_TIMERADD */
/* #undef HAVE_TIMERCLEAR */
/* #undef HAVE_TIMERCMP */
/* #undef HAVE_TIMERISSET */

/* #undef HAVE_DEVPOLL */
#define HAVE_SIGNAL_H 1
/* #undef HAVE_SYS_DEVPOLL_H */
/* #undef HAVE_SYS_EPOLL_H */
/* #undef HAVE_TAILQFOREACH */

/* #undef HAVE_VALGRIND */
#ifndef _WIN64
#define SIZEOF_LONG   4
#define SIZEOF_VOIDP  4
#define SIZEOF_CHARP  4
#else
#define SIZEOF_LONG   4
#define SIZEOF_VOIDP  8
#define SIZEOF_CHARP  8
#endif


#define SIZEOF_CHAR 1
#define HAVE_CHAR 1
#define HAVE_LONG 1
#define HAVE_CHARP 1
#define SIZEOF_SHORT 2
#define HAVE_SHORT 1
#define SIZEOF_INT 4
#define HAVE_INT 1
#define SIZEOF_LONG_LONG 8
#define HAVE_LONG_LONG 1
#define SIZEOF_OFF_T 4
#define HAVE_OFF_T 1
/* #undef SIZEOF_SIGSET_T */
/* #undef SIZEOF_UINT */
/* #undef HAVE_UINT */
/* #undef SIZEOF_ULONG */
/* #undef HAVE_ULONG */
/* #undef SIZEOF_U_INT32_T */
/* #undef HAVE_U_INT32_T */

#define SOCKET_SIZE_TYPE int

/* #undef HAVE_MBSTATE_T */

#define MAX_INDEXES 64U

#define QSORT_TYPE_IS_VOID 1
#define RETQSORTTYPE void

#define SIGNAL_RETURN_TYPE_IS_VOID 1
#define RETSIGTYPE void
#define VOID_SIGHANDLER 1
#define STRUCT_RLIMIT struct rlimit

/* #undef WORDS_BIGENDIAN */

/*
  Define to `__inline__' or `__inline' if that's what the C compiler calls it.
*/
/* #undef C_HAS_inline */
#if !(C_HAS_inline)
#ifndef __cplusplus
# define inline __inline
#endif
#endif


/* #undef TARGET_OS_LINUX */

/* #undef HAVE_LANGINFO_H */
/* #undef HAVE_WCSDUP */
/* #undef HAVE_WCHAR_T */
/* #undef HAVE_WINT_T */


#define HAVE_STRDUP 1
/* #undef HAVE_LANGINFO_CODESET */

/* #undef HAVE_WEAK_SYMBOL */
/* #undef HAVE_ABI_CXA_DEMANGLE */


/* #undef HAVE_POSIX_SIGNALS */
/* #undef HAVE_BSD_SIGNALS */
/* #undef HAVE_SVR3_SIGNALS */
#define HAVE_V7_SIGNALS 1


/* #undef HAVE_SOLARIS_STYLE_GETHOST */

/* #undef MY_ATOMIC_MODE_DUMMY */
/* #undef MY_ATOMIC_MODE_RWLOCKS */
/* #undef HAVE_GCC_ATOMIC_BUILTINS */
/* #undef HAVE_SOLARIS_ATOMIC */
/* #undef HAVE_DECL_SHM_HUGETLB */
/* #undef HAVE_LARGE_PAGES */
/* #undef HUGETLB_USE_PROC_MEMINFO */
#define NO_FCNTL_NONBLOCK 1

/* #undef _LARGE_FILES */
#define _LARGEFILE_SOURCE 1
/* #undef _LARGEFILE64_SOURCE */
/* #undef _FILE_OFFSET_BITS */

/* #undef TIME_WITH_SYS_TIME */

#define STACK_DIRECTION -1

#define SYSTEM_TYPE "Win32"
#define MACHINE_TYPE "AMD64"
/* #undef HAVE_DTRACE */

/* Windows stuff, mostly functions, that have Posix analogs but named differently */
#define S_IROTH _S_IREAD
#define S_IFIFO _S_IFIFO
/* #undef IPPROTO_IPV6 */
/* #undef IPV6_V6ONLY */
#define sigset_t int
#define mode_t int
#define SIGQUIT SIGTERM
#define SIGPIPE SIGINT
#define isnan _isnan
#define finite _finite
#define popen _popen
#define pclose _pclose
#define ssize_t SSIZE_T
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define snprintf _snprintf
#define strtok_r strtok_s
#define strtoll _strtoi64
#define strtoull _strtoui64
/* #undef vsnprintf */
#if (_MSC_VER > 1310)
# define HAVE_SETENV
#define setenv(a,b,c) _putenv_s(a,b)
#endif
/* We don't want the min/max macros */
#ifdef _WIN32
#define NOMINMAX
#endif

/*
   Memcached config options
*/
/* #undef WITH_INNODB_MEMCACHED */
/* #undef ENABLE_MEMCACHED_SASL */
/* #undef ENABLE_MEMCACHED_SASL_PWDB */
/* #undef HAVE_SASL_SASL_H */
/* #undef HAVE_HTONLL */

/*
  MySQL features
*/
#define ENABLED_LOCAL_INFILE 1
#define ENABLED_PROFILING 1
/* #undef EXTRA_DEBUG */
/* #undef BACKUP_TEST */
/* #undef CYBOZU */
/* #undef OPTIMIZER_TRACE */

/*
   InnoDB config options
*/
/* #undef INNODB_COMPILER_HINTS */

/* Character sets and collations */
#define MYSQL_DEFAULT_CHARSET_NAME "latin1"
#define MYSQL_DEFAULT_COLLATION_NAME "latin1_swedish_ci"

/* #undef USE_STRCOLL */

/* This should mean case insensitive file system */
#define FN_NO_CASE_SENSE 1

#define HAVE_CHARSET_armscii8 1
#define HAVE_CHARSET_ascii 1
#define HAVE_CHARSET_big5 1
#define HAVE_CHARSET_cp1250 1
#define HAVE_CHARSET_cp1251 1
#define HAVE_CHARSET_cp1256 1
#define HAVE_CHARSET_cp1257 1
#define HAVE_CHARSET_cp850 1
#define HAVE_CHARSET_cp852 1 
#define HAVE_CHARSET_cp866 1
#define HAVE_CHARSET_cp932 1
#define HAVE_CHARSET_dec8 1
#define HAVE_CHARSET_eucjpms 1
#define HAVE_CHARSET_euckr 1
#define HAVE_CHARSET_gb2312 1
#define HAVE_CHARSET_gbk 1
#define HAVE_CHARSET_geostd8 1
#define HAVE_CHARSET_greek 1
#define HAVE_CHARSET_hebrew 1
#define HAVE_CHARSET_hp8 1
#define HAVE_CHARSET_keybcs2 1
#define HAVE_CHARSET_koi8r 1
#define HAVE_CHARSET_koi8u 1
#define HAVE_CHARSET_latin1 1
#define HAVE_CHARSET_latin2 1
#define HAVE_CHARSET_latin5 1
#define HAVE_CHARSET_latin7 1
#define HAVE_CHARSET_macce 1
#define HAVE_CHARSET_macroman 1
#define HAVE_CHARSET_sjis 1
#define HAVE_CHARSET_swe7 1
#define HAVE_CHARSET_tis620 1
#define HAVE_CHARSET_ucs2 1
#define HAVE_CHARSET_ujis 1
#define HAVE_CHARSET_utf8mb4 1
/* #undef HAVE_CHARSET_utf8mb3 */
#define HAVE_CHARSET_utf8 1
#define HAVE_CHARSET_utf16 1
#define HAVE_CHARSET_utf32 1
#define HAVE_UCA_COLLATIONS 1
#define HAVE_COMPRESS 1
/* #undef COMPILE_FLAG_WERROR */

/*
  Important storage engines (those that really need define 
  WITH_<ENGINE>_STORAGE_ENGINE for the whole server)
*/
/* #undef WITH_MYISAM_STORAGE_ENGINE */
/* #undef WITH_HEAP_STORAGE_ENGINE */
#define WITH_PARTITION_STORAGE_ENGINE 1
/* #undef WITH_PERFSCHEMA_STORAGE_ENGINE */
/* #undef WITH_NDBCLUSTER_STORAGE_ENGINE */

#define DEFAULT_MYSQL_HOME "C:/Program Files/MySQL/MySQL Server 6.1"
#define SHAREDIR "share"
#define DEFAULT_BASEDIR "C:/Program Files/MySQL/MySQL Server 6.1"
#define MYSQL_DATADIR "C:/Program Files/MySQL/MySQL Server 6.1/data"
#define DEFAULT_CHARSET_HOME "C:/Program Files/MySQL/MySQL Server 6.1"
#define PLUGINDIR "C:/Program Files/MySQL/MySQL Server 6.1/lib/plugin"
/* #undef DEFAULT_SYSCONFDIR */

/* #undef SO_EXT */

#define MYSQL_VERSION_MAJOR 6
#define MYSQL_VERSION_MINOR 1
#define MYSQL_VERSION_PATCH 1
#define MYSQL_VERSION_EXTRA ""

#define PACKAGE "mysql"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "MySQL Server"
#define PACKAGE_STRING "MySQL Server 6.1.1"
#define PACKAGE_TARNAME "mysql"
#define PACKAGE_VERSION "6.1.1"
#define VERSION "6.1.1"
#define PROTOCOL_VERSION 10


/* time_t related defines */

#define SIZEOF_TIME_T 8
/* #undef TIME_T_UNSIGNED */

/* CPU information */

#define CPU_LEVEL1_DCACHE_LINESIZE 64

#endif
#else
#ifndef MY_CONFIG_H
#define MY_CONFIG_H
#define DOT_FRM_VERSION 6
/* Headers we may want to use. */
#define _GNU_SOURCE 1
#define HAVE_ALLOCA_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_ASM_MSR_H 1
#define HAVE_ASM_TERMBITS_H 1
#define HAVE_CRYPT_H 1
/* #undef HAVE_CURSES_H */
/* #undef HAVE_CXXABI_H */
/* #undef HAVE_NCURSES_H */
/* #undef HAVE_NDIR_H */
#define HAVE_DIRENT_H 1
#define HAVE_DLFCN_H 1
#define HAVE_EXECINFO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FENV_H 1
#define HAVE_FNMATCH_H 1
#define HAVE_FPU_CONTROL_H 1
#define HAVE_GRP_H 1
/* #undef HAVE_IA64INTRIN_H */
/* #undef HAVE_IEEEFP_H */
#define HAVE_INTTYPES_H 1
#define HAVE_MALLOC_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_PATHS_H 1
#define HAVE_POLL_H 1
#define HAVE_PWD_H 1
#define HAVE_SCHED_H 1
/* #undef HAVE_SELECT_H */
/* #undef HAVE_SOLARIS_LARGE_PAGES */
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STDINT_H 1
/* #undef HAVE_SYNCH_H */
/* #undef HAVE_SYSENT_H */
#define HAVE_SYS_DIR_H 1
#define HAVE_SYS_CDEFS_H 1
/* #undef HAVE_SYS_FPU_H */
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_IPC_H 1
/* #undef HAVE_SYS_MALLOC_H */
#define HAVE_SYS_MMAN_H 1
/* #undef HAVE_SYS_NDIR_H */
#define HAVE_SYS_PRCTL_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SHM_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIMES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UN_H 1
/* #undef HAVE_SYS_VADVISE_H */
/* #undef HAVE_TERM_H */
/* #undef HAVE_TERMBITS_H */
#define HAVE_TERMIOS_H 1
#define HAVE_TERMIO_H 1
/* #undef HAVE_TERMCAP_H */
#define HAVE_UNISTD_H 1
#define HAVE_UTIME_H 1
/* #undef HAVE_VIS_H */
/* #undef HAVE_SYS_UTIME_H */
#define HAVE_SYS_WAIT_H 1
#define HAVE_SYS_PARAM_H 1

/* Libraries */
/* #undef HAVE_LIBPTHREAD */
#define HAVE_LIBM 1
#define HAVE_LIBDL 1
/* #undef HAVE_LIBRT */
/* #undef HAVE_LIBSOCKET */
/* #undef HAVE_LIBNSL */
#define HAVE_LIBCRYPT 1
/* #undef HAVE_LIBWRAP */

/* Readline */
/* #undef HAVE_HIST_ENTRY */
/* #undef USE_LIBEDIT_INTERFACE */

#define FIONREAD_IN_SYS_IOCTL 1
#define GWINSZ_IN_SYS_IOCTL 1
/* #undef FIONREAD_IN_SYS_FILIO */

/* Functions we may want to use. */
/* #undef HAVE_ALIGNED_MALLOC */
#define HAVE_ALARM 1
#define HAVE_INDEX 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_CRYPT 1
#define HAVE_CUSERID 1
/* #undef HAVE_DIRECTIO */
#define HAVE_DLERROR 1
#define HAVE_DLOPEN 1
#define HAVE_FCHMOD 1
#define HAVE_FCNTL 1
#define HAVE_FDATASYNC 1
#define HAVE_FESETROUND 1
#define HAVE_FINITE 1
/* #undef HAVE_FP_EXCEPT */
#define HAVE_FSEEKO 1
#define HAVE_FSYNC 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETCWD 1
#define HAVE_GETHOSTBYADDR_R 1
/* #undef HAVE_GETHRTIME */
#define HAVE_GETNAMEINFO 1
#define HAVE_GETPAGESIZE 1
#define HAVE_GETPASS 1
/* #undef HAVE_GETPASSPHRASE */
#define HAVE_GETPWNAM 1
#define HAVE_GETPWUID 1
#define HAVE_GETRLIMIT 1
#define HAVE_GETRUSAGE 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GETWD 1
#define HAVE_GMTIME_R 1
/* #undef gmtime_r */
#define HAVE_INITGROUPS 1
/* #undef HAVE_ISSETUGID */
#define HAVE_GETUID 1
#define HAVE_GETEUID 1
#define HAVE_GETGID 1
#define HAVE_GETEGID 1
#define HAVE_ISINF 1
#define HAVE_LARGE_PAGE_OPTION 1
#define HAVE_LRAND48 1
#define HAVE_LOCALTIME_R 1
#define HAVE_LOG2 1
#define HAVE_LSTAT 1
#define HAVE_MEMALIGN 1
#define HAVE_MLOCK 1
#define HAVE_NL_LANGINFO 1
#define HAVE_MADVISE 1
#define HAVE_DECL_MADVISE 1
/* #undef HAVE_DECL_TGOTO */
/* #undef HAVE_DECL_MHA_MAPSIZE_VA */
#define HAVE_MALLOC_INFO 1
#define HAVE_MKSTEMP 1
#define HAVE_MLOCKALL 1
#define HAVE_MMAP 1
#define HAVE_MMAP64 1
#define HAVE_POLL 1
#define HAVE_POSIX_FALLOCATE 1
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_PREAD 1
#define HAVE_PAUSE_INSTRUCTION 1
/* #undef HAVE_FAKE_PAUSE_INSTRUCTION */
/* #undef HAVE_RDTSCLL */
#define HAVE_PTHREAD_ATTR_GETSTACKSIZE 1
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#define HAVE_PTHREAD_KEY_DELETE 1
#define HAVE_PTHREAD_RWLOCK_RDLOCK 1
/* #undef HAVE_PTHREAD_SETSCHEDPARAM */
#define HAVE_PTHREAD_SIGMASK 1
/* #undef HAVE_PTHREAD_YIELD_NP */
#define HAVE_PTHREAD_YIELD_ZERO_ARG 1
#define PTHREAD_ONCE_INITIALIZER PTHREAD_ONCE_INIT
#define HAVE_READDIR_R 1
#define HAVE_READLINK 1
#define HAVE_REALPATH 1
#define HAVE_RINT 1
/* #undef HAVE_RWLOCK_INIT */
#define HAVE_SCHED_YIELD 1
#define HAVE_SELECT 1
/* #undef HAVE_SETFD */
#define HAVE_SETENV 1
#define HAVE_SIGSET 1
#define HAVE_SIGACTION 1
#define HAVE_SLEEP 1
#define HAVE_STPCPY 1
#define HAVE_STRSIGNAL 1
/* #undef HAVE_STRLCPY */
/* #undef HAVE_STRLCAT */
/* #undef HAVE_FGETLN */
#define HAVE_STRNLEN 1
#define HAVE_STRSEP 1
#define HAVE_STRTOK_R 1
#define HAVE_STRTOLL 1
/* #undef HAVE_TELL */
#define HAVE_TEMPNAM 1
/* #undef HAVE_THR_SETCONCURRENCY */
/* #undef HAVE_THR_YIELD */
#define HAVE_TIMES 1
#define HAVE_VASPRINTF 1
#define HAVE_FTRUNCATE 1
#define HAVE_TZNAME 1
/* Symbols we may use */
/* used by stacktrace functions */
#define HAVE_BSS_START 1
#define HAVE_BACKTRACE 1
#define HAVE_BACKTRACE_SYMBOLS 1
#define HAVE_BACKTRACE_SYMBOLS_FD 1
/* #undef HAVE_PRINTSTACK */
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_STRUCT_IN6_ADDR 1
/* #undef HAVE_NETINET_IN6_H */
#define HAVE_IPV6 1
/* #undef ss_family */
/* #undef HAVE_SOCKADDR_IN_SIN_LEN */
/* #undef HAVE_SOCKADDR_IN6_SIN6_LEN */

#define DNS_USE_CPU_CLOCK_FOR_ID 1
#define HAVE_EPOLL 1
/* #undef HAVE_EVENT_PORTS */
#define HAVE_INET_NTOP 1
/* #undef HAVE_WORKING_KQUEUE */
#define HAVE_TIMERADD 1
#define HAVE_TIMERCLEAR 1
#define HAVE_TIMERCMP 1
#define HAVE_TIMERISSET 1

/* #undef HAVE_DEVPOLL */
#define HAVE_SIGNAL_H 1
/* #undef HAVE_SYS_DEVPOLL_H */
#define HAVE_SYS_EPOLL_H 1
#define HAVE_TAILQFOREACH 1

/* #undef HAVE_VALGRIND */
#ifdef __LP64__
#define SIZEOF_LONG   8
#define SIZEOF_VOIDP  8
#define SIZEOF_CHARP  8
#else
#define SIZEOF_LONG   4
#define SIZEOF_VOIDP  4
#define SIZEOF_CHARP  4
#endif

#define SIZEOF_CHAR 1
#define HAVE_CHAR 1
#define HAVE_LONG 1
#define HAVE_CHARP 1
#define SIZEOF_SHORT 2
#define HAVE_SHORT 1
#define SIZEOF_INT 4
#define HAVE_INT 1
#define SIZEOF_LONG_LONG 8
#define HAVE_LONG_LONG 1
#define SIZEOF_OFF_T 8
#define HAVE_OFF_T 1
#define SIZEOF_SIGSET_T 128
#define SIZEOF_UINT 4
#define HAVE_UINT 1
#define SIZEOF_ULONG 8
#define HAVE_ULONG 1
#define SIZEOF_U_INT32_T 4
#define HAVE_U_INT32_T 1

#define SOCKET_SIZE_TYPE socklen_t

/* #undef HAVE_MBSTATE_T */

#define MAX_INDEXES 64U

#define QSORT_TYPE_IS_VOID 1
#define RETQSORTTYPE void

#define SIGNAL_RETURN_TYPE_IS_VOID 1
#define RETSIGTYPE void
#define VOID_SIGHANDLER 1
#define STRUCT_RLIMIT struct rlimit

/* #undef WORDS_BIGENDIAN */

/*
  Define to `__inline__' or `__inline' if that's what the C compiler calls it.
*/
#define C_HAS_inline 1
#if !(C_HAS_inline)
#ifndef __cplusplus
# define inline 
#endif
#endif


#define TARGET_OS_LINUX 1

#define HAVE_LANGINFO_H 1
/* #undef HAVE_WCSDUP */
/* #undef HAVE_WCHAR_T */
/* #undef HAVE_WINT_T */


#define HAVE_STRDUP 1
/* #undef HAVE_LANGINFO_CODESET */

#define HAVE_WEAK_SYMBOL 1
/* #undef HAVE_ABI_CXA_DEMANGLE */


#define HAVE_POSIX_SIGNALS 1
/* #undef HAVE_BSD_SIGNALS */
/* #undef HAVE_SVR3_SIGNALS */
/* #undef HAVE_V7_SIGNALS */


/* #undef HAVE_SOLARIS_STYLE_GETHOST */

/* #undef MY_ATOMIC_MODE_DUMMY */
/* #undef MY_ATOMIC_MODE_RWLOCKS */
#define HAVE_GCC_ATOMIC_BUILTINS 1
/* #undef HAVE_SOLARIS_ATOMIC */
#define HAVE_DECL_SHM_HUGETLB 1
#define HAVE_LARGE_PAGES 1
#define HUGETLB_USE_PROC_MEMINFO 1
/* #undef NO_FCNTL_NONBLOCK */

/* #undef _LARGE_FILES */
#define _LARGEFILE_SOURCE 1
/* #undef _LARGEFILE64_SOURCE */
#define _FILE_OFFSET_BITS 64

#define TIME_WITH_SYS_TIME 1

#define STACK_DIRECTION -1

#define SYSTEM_TYPE "Linux"
#define MACHINE_TYPE "x86_64"
/* #undef HAVE_DTRACE */

/* Windows stuff, mostly functions, that have Posix analogs but named differently */
/* #undef S_IROTH */
/* #undef S_IFIFO */
/* #undef IPPROTO_IPV6 */
/* #undef IPV6_V6ONLY */
/* #undef sigset_t */
/* #undef mode_t */
/* #undef SIGQUIT */
/* #undef SIGPIPE */
/* #undef isnan */
/* #undef finite */
/* #undef popen */
/* #undef pclose */
/* #undef ssize_t */
/* #undef strcasecmp */
/* #undef strncasecmp */
/* #undef snprintf */
/* #undef strtok_r */
/* #undef strtoll */
/* #undef strtoull */
/* #undef vsnprintf */
#if (_MSC_VER > 1310)
# define HAVE_SETENV
#define setenv(a,b,c) _putenv_s(a,b)
#endif
/* We don't want the min/max macros */
#ifdef _WIN32
#define NOMINMAX
#endif

/*
   Memcached config options
*/
/* #undef WITH_INNODB_MEMCACHED */
/* #undef ENABLE_MEMCACHED_SASL */
/* #undef ENABLE_MEMCACHED_SASL_PWDB */
/* #undef HAVE_SASL_SASL_H */
/* #undef HAVE_HTONLL */

/*
  MySQL features
*/
#define ENABLED_LOCAL_INFILE 1
#define ENABLED_PROFILING 1
/* #undef EXTRA_DEBUG */
/* #undef BACKUP_TEST */
/* #undef CYBOZU */
/* #undef OPTIMIZER_TRACE */

/*
   InnoDB config options
*/
/* #undef INNODB_COMPILER_HINTS */

/* Character sets and collations */
#define MYSQL_DEFAULT_CHARSET_NAME "latin1"
#define MYSQL_DEFAULT_COLLATION_NAME "latin1_swedish_ci"

/* #undef USE_STRCOLL */

/* This should mean case insensitive file system */
/* #undef FN_NO_CASE_SENSE */

#define HAVE_CHARSET_armscii8 1
#define HAVE_CHARSET_ascii 1
#define HAVE_CHARSET_big5 1
#define HAVE_CHARSET_cp1250 1
#define HAVE_CHARSET_cp1251 1
#define HAVE_CHARSET_cp1256 1
#define HAVE_CHARSET_cp1257 1
#define HAVE_CHARSET_cp850 1
#define HAVE_CHARSET_cp852 1 
#define HAVE_CHARSET_cp866 1
#define HAVE_CHARSET_cp932 1
#define HAVE_CHARSET_dec8 1
#define HAVE_CHARSET_eucjpms 1
#define HAVE_CHARSET_euckr 1
#define HAVE_CHARSET_gb2312 1
#define HAVE_CHARSET_gbk 1
#define HAVE_CHARSET_geostd8 1
#define HAVE_CHARSET_greek 1
#define HAVE_CHARSET_hebrew 1
#define HAVE_CHARSET_hp8 1
#define HAVE_CHARSET_keybcs2 1
#define HAVE_CHARSET_koi8r 1
#define HAVE_CHARSET_koi8u 1
#define HAVE_CHARSET_latin1 1
#define HAVE_CHARSET_latin2 1
#define HAVE_CHARSET_latin5 1
#define HAVE_CHARSET_latin7 1
#define HAVE_CHARSET_macce 1
#define HAVE_CHARSET_macroman 1
#define HAVE_CHARSET_sjis 1
#define HAVE_CHARSET_swe7 1
#define HAVE_CHARSET_tis620 1
#define HAVE_CHARSET_ucs2 1
#define HAVE_CHARSET_ujis 1
#define HAVE_CHARSET_utf8mb4 1
/* #undef HAVE_CHARSET_utf8mb3 */
#define HAVE_CHARSET_utf8 1
#define HAVE_CHARSET_utf16 1
#define HAVE_CHARSET_utf32 1
#define HAVE_UCA_COLLATIONS 1
#define HAVE_COMPRESS 1
/* #undef COMPILE_FLAG_WERROR */

/*
  Important storage engines (those that really need define 
  WITH_<ENGINE>_STORAGE_ENGINE for the whole server)
*/
/* #undef WITH_MYISAM_STORAGE_ENGINE */
/* #undef WITH_HEAP_STORAGE_ENGINE */
#define WITH_PARTITION_STORAGE_ENGINE 1
/* #undef WITH_PERFSCHEMA_STORAGE_ENGINE */
/* #undef WITH_NDBCLUSTER_STORAGE_ENGINE */

#define DEFAULT_MYSQL_HOME "/usr/local/mysql"
#define SHAREDIR "/usr/local/mysql/share"
#define DEFAULT_BASEDIR "/usr/local/mysql"
#define MYSQL_DATADIR "/usr/local/mysql/data"
#define DEFAULT_CHARSET_HOME "/usr/local/mysql"
#define PLUGINDIR "/usr/local/mysql/lib/plugin"
#define DEFAULT_SYSCONFDIR "/usr/local/mysql/etc"

/* #undef SO_EXT */

#define MYSQL_VERSION_MAJOR 6
#define MYSQL_VERSION_MINOR 1
#define MYSQL_VERSION_PATCH 1
#define MYSQL_VERSION_EXTRA ""

#define PACKAGE "mysql"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "MySQL Server"
#define PACKAGE_STRING "MySQL Server 6.1.1"
#define PACKAGE_TARNAME "mysql"
#define PACKAGE_VERSION "6.1.1"
#define VERSION "6.1.1"
#define PROTOCOL_VERSION 10


/* time_t related defines */

#define SIZEOF_TIME_T 8
/* #undef TIME_T_UNSIGNED */

/* CPU information */

#define CPU_LEVEL1_DCACHE_LINESIZE 64


#endif
#endif