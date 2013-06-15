#include "Platform.h"

using namespace std;
bool endsWith(const string &a, const string &b) {
	if (a.length() >= b.length()) {
		return (a.compare (a.length() - b.length(), b.length(), b) == 0);
	} else {
		return false;
	}
}
bool is_number(const string &s) {	
		return !s.empty() && std::find_if(s.begin(), 
			s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

std::string replaceAll( std::string const& original, std::string const& before, std::string const& after ) {
	std::string retval;
	std::string::const_iterator end     = original.end();
	std::string::const_iterator current = original.begin();
	std::string::const_iterator next    =
		std::search( current, end, before.begin(), before.end() );
	while ( next != end ) {
		retval.append( current, next );
		retval.append( after );
		current = next + before.size();
		next = std::search( current, end, before.begin(), before.end() );
	}
	retval.append( current, next );
	return retval;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}