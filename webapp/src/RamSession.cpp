/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */
//RamSession storage
#include "Session.h"

using namespace std;

//No initialisation needed, uses STL container.
void RamSession::create(const string& sessionid) {
	this->sessionid = string(sessionid);
}

void RamSession::destroy() {
	_store.clear();
}

void RamSession::store(const string& key, const string& value) {
	_store.insert(make_pair(key, value));
}

const string& RamSession::get(const string& key) {
	for(auto it = _store.begin(); it != _store.end(); ++it) {
		if(it->first == key) return it->second;
	}
	return empty;
}

int RamSession::count() {
	return _store.size();
}
