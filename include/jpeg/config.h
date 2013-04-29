#define VERSION "1.2.90"
#define BUILD ""
#define PACKAGE_NAME "libjpeg-turbo C"

#ifndef INLINE
#if defined(__GNUC__)
#define INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
#define INLINE __forceinline
#else
#define INLINE
#endif
#endif
