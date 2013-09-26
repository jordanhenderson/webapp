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

#ifdef WIN32
#define ENV_NEWLINE "\r\n"
#define fopen _wfopen
#else
#define ENV_NEWLINE "\n"
#include <unistd.h>
#endif

#define vsnprintf rpl_vsnprintf
#define snprintf rpl_snprintf
#define vasprintf rpl_vasprintf
#define asprintf rpl_asprintf

extern "C" {
#if !HAVE_VSNPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list);
#endif
#if !HAVE_SNPRINTF
int rpl_snprintf(char *, size_t, const char *, ...);
#endif
#if !HAVE_VASPRINTF
int rpl_vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_ASPRINTF
int rpl_asprintf(char **, const char *, ...);
#endif
};

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


const char* date_format(const char* fmt, const size_t datesize, time_t* t=NULL, int gmt=0);
std::string url_decode(const std::string& src);
void add_days(time_t& t, int days);

typedef void* FCGX_Request;
#endif

