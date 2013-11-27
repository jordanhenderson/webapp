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
	for(std::unordered_map<std::string, std::string>::iterator it = _store.begin();
		it != _store.end(); ++it) {
			if(it->first == key)
				return it->second;
	}
	return empty;
}

