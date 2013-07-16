#include "Platform.h"
#include <algorithm>
#include <cctype>
#include <ctime>
using namespace std;
bool endsWith(const string &a, const string &b) {

	if (a.length() >= b.length()) {
		return (a.compare (a.length() - b.length(), b.length(), b) == 0);
	} else {
		return false;
	}
}
bool is_number(const string &s) {	
		return !s.empty() && find_if(s.begin(), 
			s.end(), [](char c) { return !isdigit(c); }) == s.end();
}

string replaceAll( string const& original, string const& before, string const& after ) {
	string retval;
	string::const_iterator end     = original.end();
	string::const_iterator current = original.begin();
	string::const_iterator next    =
		search( current, end, before.begin(), before.end() );
	while ( next != end ) {
		retval.append( current, next );
		retval.append( after );
		current = next + before.size();
		next = search( current, end, before.begin(), before.end() );
	}
	retval.append( current, next );
	return retval;
}

string string_format(const std::string fmt, ...) {
	int size = 100;
	std::string str;
	va_list ap;
	while (1) {
		str.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
		va_end(ap);
		if (n > -1 && n < size) {
			str.resize(n);
			return str;
		}
		if (n > -1)
			size = n + 1;
		else
			size *= 2;
	}
	return str;
}

string date_format(const std::string& fmt, const size_t datesize, time_t* t, int gmt) {
	time_t actual_time;
	if(t == NULL) {
		time(&actual_time);
	} else actual_time = *t;
	std::string str;
	str.resize(datesize);
	if(!gmt) strftime((char*)str.c_str(), datesize+1, fmt.c_str(), localtime(&actual_time));
	else strftime((char*)str.c_str(), datesize+1, fmt.c_str(), gmtime(&actual_time));
	return str;
}

void add_days(time_t& t, int days) {
	t += days * 24 * 3600;
}

string empty = "";