#ifndef PARAMETERS_H
#define PARAMETERS_H
#include "Platform.h"
#include "tbb/concurrent_unordered_map.h"
//Dynamic parameter class
typedef tbb::concurrent_unordered_map<tstring, tstring> paramMap;
class Parameters : Internal {
public:
	tstring get(tstring param);
	const int getDigit(tstring param);
	void set(tstring param, tstring value);
	inline static void parseBuffer(void* params, TCHAR* paramBuffer) {
		Parameters* t = static_cast<Parameters*>(params);
		TCHAR* val = _tcschr(paramBuffer, '=');
		TCHAR* key = paramBuffer;
		if(!val)
			return;
		val++;
		*(val - 1) = '\0';
		t->set(key, val);
	}
	bool hasParam(tstring param);
	size_t getSize();
	Parameters();
	~Parameters();
private:
	paramMap* params;
};


#endif