/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 
 * Some portions of this file may be licensed under public domain.
 */
 
#ifndef CPLATFORM_H
#define CPLATFORM_H

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

//Support large (64 bit) seek and tell operations.
#ifdef _WIN32
#define ftell64(a) _ftelli64(a)
#define fseek64(a,b,c) _fseeki64(a,b,c)
#else
#define ftell64(a) ftello(a)
#define fseek64(a,b,c) fseeko(a,b,c)
#endif

struct webapp_str_t {
	char* data = NULL;
	long long len = 0;
	int allocated = 0;
	webapp_str_t() {}
	webapp_str_t(const char* s, long long len) {
		allocated = 1;
		data = new char[len];
		memcpy(data, s, len);
	}
	webapp_str_t(long long _len) {
		allocated = 1;
		data = new char[len];
		_len = len;
	}
	webapp_str_t(webapp_str_t* other) {
		allocated = 1;
		len = other->len;
		data = new char[len];
		memcpy(data, other->data, len);
	}
	webapp_str_t(const char* s) {
		allocated = 1;
		len = strlen(s);
		data = new char[len];
		memcpy(data, s, len);
	}
	webapp_str_t(const webapp_str_t& other) {
		allocated = 1;
		len = other.len;
		data = new char[len];
		memcpy(data, other.data, len);
	}
	~webapp_str_t() {
		if(allocated) delete[] data;
	}
	operator std::string const () {
		return std::string(data, len);
	}
};

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

#ifdef _MSC_VER
wchar_t* strtowide(const char* str) {
	size_t requiredSize = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	wchar_t* tmpPath = new wchar_t[requiredSize+sizeof(wchar_t)];
	MultiByteToWideChar(CP_UTF8, 0, str, -1, tmpPath, requiredSize);
	return tmpPath;
}

char* widetostr(const wchar_t* str) {
	size_t requiredSize = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	char* tmpPath = new char[requiredSize+sizeof(char)];
	WideCharToMultiByte(CP_UTF8, 0, str, -1, tmpPath, requiredSize, NULL, NULL);
	return tmpPath;
}
#endif
#endif //CPLATFORM_H
