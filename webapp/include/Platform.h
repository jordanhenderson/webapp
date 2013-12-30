/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */
 
//This file contains universal macros and definitions helpful to all classes.
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef WIN32
#define ENV_NEWLINE "\r\n"
#else
#define ENV_NEWLINE "\n"
#include <unistd.h>
#endif

#ifdef WIN32
#define vsnprintf rpl_vsnprintf
#define snprintf rpl_snprintf
#define vasprintf rpl_vasprintf
#define asprintf rpl_asprintf
#endif

#if _MSC_VER
	#pragma warning (disable : 4503)
#endif

#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_SOCKET_FAILED 3L
#define ERROR_DB_FAILED 4L
#define ERROR_INVALID_IMAGE 5L
#define ERROR_IMAGE_PROCESSING_FAILED 6L
#define ERROR_IMAGE_NOT_SUPPORTED 7L

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

#define GETCHK(s) s.empty() ? 0 : 1
#define WEBAPP_PARAM_BASEPATH 0
#define WEBAPP_PARAM_DBPATH 1
#define WEBAPP_STATIC_STRINGS 3
#define WEBAPP_LEN_SESSIONID 1
#define WEBAPP_NUM_THREADS 8
#define INT_INTERVAL(i) sizeof(int)*i
#define WEBAPP_DEFAULT_QUEUESIZE 1023
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PORT_DEFAULT 5000
#endif

