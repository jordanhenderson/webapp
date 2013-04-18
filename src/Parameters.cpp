#include "Parameters.h"


tstring Parameters::get(tstring param) {
	tstring retVal;
	try {
		retVal = params->at(param);
		//param does not exist
	}
	catch(const std::out_of_range&) {
		retVal = _T("");
	}
	return retVal;
}

const int Parameters::getDigit(tstring param) {
	tstring str = get(param);
	return _ttoi(str.c_str());
}

void Parameters::set(tstring param, tstring value) {
	params->insert(std::make_pair(param, value));
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