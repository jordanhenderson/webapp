#include "Platform.h"
#include <algorithm>
#include <cctype>

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
