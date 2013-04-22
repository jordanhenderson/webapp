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


#ifdef WIN32
#define ENV_NEWLINE "\r\n"
#define snprintf _snprintf
#else
#define ENV_NEWLINE "\n"
#define _wfopen fopen

#endif

#define ERROR_FILE_NOT_FOUND 1
#define ERROR_SOCKET_FAILED 2
class Internal {
protected: int nError;
public:
	int GetLastError();
};

bool endsWith(const std::string &a, const std::string &b);
#endif