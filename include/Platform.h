//This file contains universal macros and definitions helpful to all classes.
#ifndef PLATFORM_H
#define PLATFORM_H

#include <sys/types.h>
#include <sys/stat.h>
#include <tchar.h>
#include <unordered_map>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <string>


#ifdef WIN32
#define ENV_NEWLINE _T("\r\n")
#else
#define ENV_NEWLINE _T("\n")
#define TCHAR char
#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define closesocket(socket) close(socket);
#define _tcsncmp strncmp
#define _tcsncpy strncpy
#define _tcscpy strcpy
#define _tcscat strcat
#define _tcslen strlen
#define _T(str) ##str
#define _tcsstr strstr
#define _fgetts fgets
#define _tfopen fopen
#define _tprintf printf
#define _sntprintf snprintf
#define _fputts fputs
#define _tstat stat
#define _ttoi atoi
#endif

#define tstring std::basic_string<TCHAR>

#define ERROR_FILE_NOT_FOUND 1
#define ERROR_SOCKET_FAILED 2
class Internal {
protected: int nError;
public:
	int GetLastError();
};

bool endsWith(const tstring &a, const tstring &b);
#endif