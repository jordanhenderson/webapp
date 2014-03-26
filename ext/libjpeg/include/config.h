#ifndef INLINE
#if defined(__GNUC__)
#define INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define INLINE __forceinline
#else
#define INLINE
#endif
#endif

#define JPEG_LIB_VERSION 80
#define LIBJPEG_TURBO_VERSION 1.3.1
#define VERSION "1.3.1"
#define BUILD "20140326"
#define PACKAGE_NAME "libjpeg-turbo"
#define PACKAGE "libjpeg-turbo"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_STRING "libjpeg-turbo 1.3.1"
#define PACKAGE_TARNAME "libjpeg-turbo"
#define PACKAGE_VERSION "1.3.1"
#define VERSION "1.3.1"

#define C_ARITH_CODING_SUPPORTED 1
#define D_ARITH_CODING_SUPPORTED 1
#define HAVE_DLFCN_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMCPY 1
#define HAVE_MEMORY_H 1
#define HAVE_MEMSET 1
#define HAVE_PROTOTYPES 1

#ifndef HAVE_STDLIB_H
#define HAVE_STDLIB_H 1
#endif

#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_UNSIGNED_CHAR 1
#define HAVE_UNSIGNED_SHORT 1
#define MEM_SRCDST_SUPPORTED 1
#define NEED_SYS_TYPES_H 1
#define STDC_HEADERS 1
#define WITH_SIMD 1
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif
