#include "Parameters.h"

using namespace std;
const string& Parameters::get(const string& param) {
	ParamMap::iterator it = params.find(param);
	if(it != params.end())
		return it->second;

	return empty;
}

const int Parameters::getDigit(const string& param) {
	return stoi(get(param));
}

void Parameters::set(string param, string value) {
	params.insert(make_pair(param, value));
}

bool Parameters::hasParam(string param) {
	if(get(param) != "") return true;
	return false;
}

size_t Parameters::getSize() {
	return params.size();
}

Parameters::~Parameters() {

}