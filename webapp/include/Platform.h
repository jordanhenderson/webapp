/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#if _MSC_VER
#pragma warning (disable : 4503)
#define snprintf _snprintf
#endif

#define ERROR_SUCCESS 0L

#define XSTR(a) STR(a)
#define STR(a) #a

#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define APIEXPORT __attribute__ ((dllexport))
#else
#define APIEXPORT __declspec(dllexport)
#endif
#else
#if __GNUC__ >= 4
#define APIEXPORT __attribute__ ((visibility ("default")))
#endif
#endif

#if defined(_WIN32)
# if defined(_WIN64)
#  define FORCE_UNDEFINED_SYMBOL(x) __pragma(comment (linker, "/export:" #x))
# else
#  define FORCE_UNDEFINED_SYMBOL(x) __pragma(comment (linker, "/export:_" #x))
# endif
#else
# define FORCE_UNDEFINED_SYMBOL(x) void x(void); void (*__ ## x ## _fp)(void)=&x;
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cctype>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <vector>
#include <deque>
#include <new>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <random>
#include <functional>
#include <atomic>
#include <utility>
#ifndef _WIN32
#include <unistd.h>
#endif

//Support large (64 bit) seek and tell operations.
#ifdef _WIN32
#define ftell64(a) _ftelli64(a)
#define fseek64(a,b,c) _fseeki64(a,b,c)
#else
#define ftell64(a) ftello(a)
#define fseek64(a,b,c) fseeko(a,b,c)
#endif

int32_t strntol(const char* src, size_t n);
extern struct tm epoch_tm;
extern time_t epoch;


#endif //PLATFORM_H

