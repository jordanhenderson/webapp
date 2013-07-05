//RamSession storage
#include "Session.h"

using namespace std;

//No initialisation needed, uses STL container.
void RamSession::create(string& sessionid) {
	this->sessionid = string(sessionid);
}

void RamSession::destroy() {

}

void RamSession::store(const string& key, const string& value) {
	LockableContainerLock<RamStorage> lock(*_store);
	lock->insert(make_pair(key, value));
}

string RamSession::get(const string& key) {
	LockableContainerLock<RamStorage> lock(*_store);
	for(std::unordered_map<std::string, std::string>::iterator it = lock->begin();
		it != lock->end(); it++) {
			if(it->first == key)
				return it->second;
	}
	return empty;
}

RamSession::RamSession() {
	_store = new LockableContainer<RamStorage>();
}

RamSession::~RamSession() {
	delete _store;
}