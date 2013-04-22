#ifndef PARAMETERS_H
#define PARAMETERS_H
#include "Platform.h"
#include "tbb/concurrent_unordered_map.h"
//Dynamic parameter class
typedef tbb::concurrent_unordered_map<std::string, std::string> paramMap;
class Parameters : Internal {
public:
	std::string get(std::string param);
	const int getDigit(std::string param);
	void set(std::string param, std::string value);
	inline static void parseBuffer(void* params, char* paramBuffer) {
		Parameters* t = static_cast<Parameters*>(params);
		char* val = strchr(paramBuffer, '=');
		char* key = paramBuffer;
		if(!val)
			return;
		val++;
		*(val - 1) = '\0';
		t->set(key, val);
	}
	bool hasParam(std::string param);
	size_t getSize();
	Parameters();
	~Parameters();
private:
	paramMap* params;
};


#endif