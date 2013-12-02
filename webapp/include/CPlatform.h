#ifndef HELPERS_H
#define HELPERS_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <algorithm>
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

#ifdef __GNUC__
#define HAVE_VSNPRINTF 1
#define HAVE_SNPRINTF 1
#define HAVE_VASPRINTF 1
#define HAVE_ASPRINTF 1
#endif

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

class Internal {
public:
	Internal() { nError = 0; };
	int GetLastError() {
		return nError;
	};
protected: int nError;
};

//webapp string, helper functions.
struct webapp_str_t {
	const char* data = NULL;
	int len = 0;
};

webapp_str_t* webapp_strdup(webapp_str_t*);

#ifdef contains
#undef contains
#endif

bool endsWith(const std::string &a, const std::string &b);
bool is_number(const std::string &a);
const char* date_format(const char* fmt, const size_t datesize, time_t* t = NULL, int gmt = 0);
void add_days(time_t& t, int days);
std::string replaceAll(std::string const& original, std::string const& before, std::string const& after);
std::string url_decode(const std::string& src);

template< class T, class X>
bool contains(T& needle, X& haystack) {
	return std::find(needle.begin(), needle.end(), haystack) != needle.end();
};

template < class ContainerT >
void tokenize(const std::string& str, ContainerT& tokens,
	const std::string& delimiters = " ", bool trimEmpty = false)
{
	typedef ContainerT Base; typedef typename Base::value_type ValueType; typedef typename ValueType::size_type SizeType;
	std::string::size_type pos, lastPos = 0;
	while (true)
	{
		pos = str.find_first_of(delimiters, lastPos);
		if (pos == std::string::npos)
		{
			pos = str.length();

			if (pos != lastPos || !trimEmpty)
				tokens.push_back(ValueType(str.data() + lastPos,
				(SizeType)pos - lastPos));

			break;
		}
		else
		{
			if (pos != lastPos || !trimEmpty)
				tokens.push_back(ValueType(str.data() + lastPos,
				(SizeType)pos - lastPos));
		}

		lastPos = pos + 1;
	}
};


#endif
