#include "Parameters.h"

using namespace std;
string Parameters::get(const string& param) {
	LockableContainerLock<ParamMap> lock(params);
	ParamMap::iterator it = lock->find(param);
	if(it != lock->end())
		return it->second;

	return "";
}

const int Parameters::getDigit(const string& param) {
	return stoi(get(param));
}

void Parameters::set(string param, string value) {
	LockableContainerLock<ParamMap> lock(params);
	lock->insert(make_pair(param, value));
}

bool Parameters::hasParam(string param) {
	if(get(param) != "") return true;
	return false;
}

size_t Parameters::getSize() {
	LockableContainerLock<ParamMap> lock(params);
	return lock->size();
}

Parameters::~Parameters() {

}