#define HAVE_BYTESWAP_H 1
#define HAVE_DLFCN_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_BYTESWAP_H 1
#define HAVE_SYS_ENDIAN_H 1
#define HAVE_SYS_RESOURCE_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

#ifdef _MSC_VER
#define HAVE_WINDOWS_H 1
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#define HAVE_SYS_MMAN_H 1
#define HAVE_BUILTIN_CTZ 1
#define HAVE_BUILTIN_EXPECT 1
#endif

#define STDC_HEADERS 1

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel and VAX). */
#if defined __BIG_ENDIAN__
# define WORDS_BIGENDIAN 1
#elif ! defined __LITTLE_ENDIAN__
# undef WORDS_BIGENDIAN
#endif

