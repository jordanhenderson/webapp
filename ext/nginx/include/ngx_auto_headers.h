
#ifdef _WIN32
#ifndef NGX_WIN32
#define NGX_WIN32  1
#endif
#endif

#ifdef __GNUC__

#ifndef NGX_HAVE_UNISTD_H
#define NGX_HAVE_UNISTD_H  1
#endif


#ifndef NGX_HAVE_INTTYPES_H
#define NGX_HAVE_INTTYPES_H  1
#endif


#ifndef NGX_HAVE_LIMITS_H
#define NGX_HAVE_LIMITS_H  1
#endif


#ifndef NGX_HAVE_SYS_PARAM_H
#define NGX_HAVE_SYS_PARAM_H  1
#endif


#ifndef NGX_HAVE_SYS_MOUNT_H
#define NGX_HAVE_SYS_MOUNT_H  1
#endif


#ifndef NGX_HAVE_SYS_STATVFS_H
#define NGX_HAVE_SYS_STATVFS_H  1
#endif


#ifndef NGX_HAVE_CRYPT_H
#define NGX_HAVE_CRYPT_H  1
#endif


#ifdef __linux__
#ifndef NGX_LINUX
#define NGX_LINUX  1
#endif
#ifndef NGX_HAVE_SYS_PRCTL_H
#define NGX_HAVE_SYS_PRCTL_H  1
#endif
#endif

#ifndef NGX_HAVE_SYS_VFS_H
#define NGX_HAVE_SYS_VFS_H  1
#endif

#if defined(__sun) || defined(__SVR4)
#ifndef NGX_HAVE_SYS_FILIO_H
#define NGX_HAVE_SYS_FILIO_H  1
#endif
#define NGX_SOLARIS 1
#endif

#endif


