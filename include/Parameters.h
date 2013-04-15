#ifndef PARAMETERS_H
#define PARAMETERS_H
#include "Platform.h"
//Dynamic parameter class
typedef std::unordered_map<tstring, tstring> paramMap;
class Parameters : Internal {
public:
	tstring get(tstring param);
	const int getDigit(tstring param);
	void set(tstring param, tstring value);
	void parseBuffer(tstring paramStr);
	size_t getSize();
	Parameters();
	~Parameters();
private:
	paramMap* params;
};


#endif