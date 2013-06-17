#include "Parameters.h"

using namespace std;
string Parameters::get(string param) {
	string retVal;
	try {
		retVal = params->at(param);
		//param does not exist
	}
	catch(const out_of_range&) {
		retVal = "";
	}
	return retVal;
}

const int Parameters::getDigit(string param) {
	return stoi(get(param));
}

void Parameters::set(string param, string value) {
	params->insert(make_pair(param, value));
}

bool Parameters::hasParam(string param) {
	try {
		string val = params->at(param);
		return true;
	} catch(const out_of_range&) {
		return false;
	}
}
Parameters::Parameters() {
	//Create new parameter map
	params = new paramMap();
}

size_t Parameters::getSize() {
	return params->size();
}

Parameters::~Parameters() {
	delete params;
}