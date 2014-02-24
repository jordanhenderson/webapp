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

bool endsWith(const std::string &a, const std::string &b);
bool is_number(const std::string &a);
std::string replaceAll(std::string const& original, std::string const& before,
	std::string const& after);
std::string url_decode(const std::string& src);

/**
 * Checks if a needle (n) exists within haystack (h).
 * @param n the needle to search for.
 * @param h the haystack to search in.
*/
template<class T, class X>
bool contains(T& n, X& h) {
	return std::find(n.begin(), n.end(), h) != n.end();
}

/**
 * Tokenize a string into the provided container using the specified delimiter.
 * @param str the string to tokenize.
 * @param tokens the output STL container to store tokens.
 * @param delimiters with which to separate tokens.
 * @param trimEmpty store empty tokens in the output.
*/
template < class T >
void tokenize(const std::string& str, T& tokens,
	const std::string& delimiters = " ", bool trimEmpty = false) {
	//Provide typedefs for GCC
	typedef T Base; typedef typename Base::value_type VType;
	typedef typename VType::size_type SType;

	std::string::size_type pos, lastPos = 0;
	while (true) {
		pos = str.find_first_of(delimiters, lastPos);
		if (pos == std::string::npos) {
			pos = str.length();
			if (pos != lastPos || !trimEmpty)
				tokens.push_back(VType(str.data() + lastPos,
					(SType)pos - lastPos));
			break;
		} else {
			if (pos != lastPos || !trimEmpty)
				tokens.push_back(VType(str.data() + lastPos,
				(SType)pos - lastPos));
		}
		lastPos = pos + 1;
	}
}

#endif //PLATFORM_H

