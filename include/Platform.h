//This file contains universal macros and definitions helpful to all classes.
#ifndef PLATFORM_H
#define PLATFORM_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unordered_map>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <memory>
#include <ctime>
#include <direct.h>


#ifdef WIN32
#define ENV_NEWLINE "\r\n"
#define snprintf _snprintf
#else
#define ENV_NEWLINE "\n"
#define _wfopen fopen

#endif
//Similar definitions to winerror.h to avoid incompatibility.
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_SOCKET_FAILED 3L
#define ERROR_DB_FAILED 4L
#define ERROR_INVALID_IMAGE 5L
#define ERROR_IMAGE_PROCESSING_FAILED 6L

#define HTML_HEADER "Content-type: text/html\r\n\r\n"
#define CSS_HEADER "Content-type: text/css; charset=UTF-8\r\n\r\n"
#define JSON_HEADER "Content-type: application/json\r\n\r\n"
#define JS_HEADER "Content-type: application/javascript\r\n\r\n"
#define HTML_404 "Status: 404 Not Found\r\n\r\nThe page you requested cannot be found (404)."
#define XSTR(a) STR(a)
#define STR(a) #a
#define PATHSEP '/'

class Internal {
public:
	int GetLastError();
protected: int nError;
};

bool endsWith(const std::string &a, const std::string &b);
bool is_number(const std::string &a);
#endif