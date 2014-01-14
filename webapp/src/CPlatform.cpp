/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Platform.h"
#include "CPlatform.h"

using namespace std;

/**
 * returns if string a ends with string b.
 * @param a entire string
 * @param b substring 
 * @return if string a ends with b.
*/
bool endsWith(const string &a, const string &b) {
	if (a.length() >= b.length()) {
		return (a.compare (a.length() - b.length(), b.length(), b) == 0);
	} else {
		return false;
	}
}

/**
 * returns if string s is a string representation of a number.
 * @param s string in the possible form of a number.
 * @return if string s is a numeric string. 
*/
bool is_number(const string &s) {	
		return !s.empty() && find_if(s.begin(), 
			s.end(), [](char c) { return !isdigit(c); }) == s.end();
}

/**
 * replaces all occurences of before in the string original with after,
 * returning the modified string.
 * @param origin the original string
 * @param before the substring to replace
 * @param after the value to replace before with
 * @return the resultant string
*/
string replaceAll( string const& original, string const& before,
				   string const& after ) {
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

/**
 * Decodes a URI encoded string.
 * Source: http://stackoverflow.com/questions/154536/encode-decode-urls-in-c
 * @param src the URI encoded string
 * @return the decoded string
*/
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
