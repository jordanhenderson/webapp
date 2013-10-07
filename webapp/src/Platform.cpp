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

const char* date_format(const char* fmt, const size_t datesize, time_t* t, int gmt) {
	time_t actual_time;
	if(t == NULL) {
		time(&actual_time);
	} else actual_time = *t;
	char* str = (char*)malloc(datesize+1);
	if(!gmt) strftime(str, datesize+1, fmt, localtime(&actual_time));
	else strftime(str, datesize+1, fmt, gmtime(&actual_time));
	return str;
}

void add_days(time_t& t, int days) {
	t += days * 24 * 3600;
}

//Copypasta'd from http://stackoverflow.com/questions/154536/encode-decode-urls-in-c
string url_decode(const string& src) {
 string ret;
    char ch;
    int i, ii;
    for (i=0; i<src.length(); i++) {
        if (int(src[i])==37) {
            sscanf(src.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
			if(ch != '\r')
				ret+=ch;
            i=i+2;
        } else if(int(src[i]=='+')) {
			ret+=' ';
        } else {
			ret+=src[i];
		}
    }

    return (ret);
}