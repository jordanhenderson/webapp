#include "Platform.h"
#include "CPlatform.h"
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

//Copypasta'd from http://stackoverflow.com/questions/154536/encode-decode-urls-in-c
string url_decode(const string& src) {
 string ret;
    char ch;
    unsigned int i, ii;
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

//Webapp string, helper functions

//webapp_strdup
//returns a new allocated copy of src. Must be cleaned up by caller.
webapp_str_t* webapp_strdup(webapp_str_t* src) {
	webapp_str_t* dest = new webapp_str_t();
	dest->data = new char[src->len + 1];
	memcpy((void*)dest->data, src->data, src->len);
	dest->len = src->len;
	return dest;
}
