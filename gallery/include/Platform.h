//This file contains universal macros and definitions helpful to all classes.
#ifndef PLATFORM_H
#define PLATFORM_H

#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <string.h>
#include <map>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <list>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

//#define DATABASE_SQLITE
#define DATABASE_MYSQL

#ifdef WIN32
#define ENV_NEWLINE "\r\n"
#else
#define ENV_NEWLINE "\n"
#define _wfopen fopen
#include <unistd.h>
#endif

#if _MSC_VER
	#pragma warning (disable : 4503)
#endif

#define DB_FUNC_RANDOM "RANDOM()"
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_SOCKET_FAILED 3L
#define ERROR_DB_FAILED 4L
#define ERROR_INVALID_IMAGE 5L
#define ERROR_IMAGE_PROCESSING_FAILED 6L
#define ERROR_IMAGE_NOT_SUPPORTED 7L

#define HTML_HEADER "Content-type: text/html\r\n"
#define CSS_HEADER "Content-type: text/css; charset=UTF-8\r\n"
#define JSON_HEADER "Content-type: application/json\r\n"
#define JS_HEADER "Content-type: application/javascript\r\n"
#define END_HEADER "\r\n"
#define HTML_404 "Status: 404 Not Found\r\n\r\nThe page you requested cannot be found (404)."
#define XSTR(a) STR(a)
#define STR(a) #a


extern const std::string EMPTY;

class Internal {
public:
	Internal() { nError = 0; };
	int GetLastError();
protected: int nError;
};

#define contains(v, x) (std::find(v.begin(), v.end(), x) != v.end())

bool endsWith(const std::string &a, const std::string &b);
bool is_number(const std::string &a);
std::string replaceAll( std::string const& original, std::string const& before, std::string const& after );

template < class ContainerT >
void tokenize(const std::string& str, ContainerT& tokens,
			  const std::string& delimiters = " ", bool trimEmpty = false) 
{
	typedef ContainerT Base; typedef typename Base::value_type ValueType; typedef typename ValueType::size_type SizeType;
	std::string::size_type pos, lastPos = 0;
	while(true)
	{
		pos = str.find_first_of(delimiters, lastPos);
		if(pos == std::string::npos)
		{
			pos = str.length();

			if(pos != lastPos || !trimEmpty)
				tokens.push_back(ValueType(str.data()+lastPos,
				(SizeType)pos-lastPos ));

			break;
		}
		else
		{
			if(pos != lastPos || !trimEmpty)
				tokens.push_back(ValueType(str.data()+lastPos,
				(SizeType)pos-lastPos ));
		}

		lastPos = pos + 1;
	}
};



std::string string_format(const std::string fmt, ...);
std::string date_format(const std::string& fmt, const size_t datesize, time_t* t=NULL, int gmt = 0);
std::string url_decode(const std::string& src);
void add_days(time_t& t, int days);
#endif