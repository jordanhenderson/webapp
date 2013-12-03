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

#define FORCE_UNDEFINED_SYMBOL(x) extern void x(void); APIEXPORT void* __ ## x ## _fp =(void*)&x;

#define GETCHK(s) s.empty() ? 0 : 1
#define WEBAPP_PARAM_BASEPATH 0
#define WEBAPP_PARAM_DBPATH 1
#define WEBAPP_STATIC_STRINGS 2
#define WEBAPP_LEN_SESSIONID 1
#define INT_INTERVAL(i) sizeof(int)*i
#define WEBAPP_DEFAULT_QUEUESIZE 1023
#define WEBAPP_PORT 5000

#endif

