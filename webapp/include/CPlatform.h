/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 
 * Some portions of this file may be licensed under public domain.
 */
 
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
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

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

//webapp string, helper functions.
struct webapp_str_t {
	const char* data = NULL;
	size_t len = 0;
};

webapp_str_t* webapp_strdup(webapp_str_t*);

#ifdef contains
#undef contains
#endif

bool endsWith(const std::string &a, const std::string &b);
bool is_number(const std::string &a);
std::string replaceAll(std::string const& original, std::string const& before, std::string const& after);
std::string url_decode(const std::string& src);

template<class T, class X>
bool contains(T& n, X& h) {
	return std::find(n.begin(), n.end(), h) != n.end();
};

template < class T >
void tokenize(const std::string& str, T& tokens,
	const std::string& delimiters = " ", bool trimEmpty = false)
{
	typedef T Base; typedef typename Base::value_type VType; typedef typename VType::size_type SType;
	std::string::size_type pos, lastPos = 0;
	while (true)
	{
		pos = str.find_first_of(delimiters, lastPos);
		if (pos == std::string::npos)
		{
			pos = str.length();

			if (pos != lastPos || !trimEmpty)
				tokens.push_back(VType(str.data() + lastPos,
				(SType)pos - lastPos));

			break;
		}
		else
		{
			if (pos != lastPos || !trimEmpty)
				tokens.push_back(VType(str.data() + lastPos,
				(SType)pos - lastPos));
		}

		lastPos = pos + 1;
	}
};


#endif
