#ifndef PARAMETERS_H
#define PARAMETERS_H
#include "Platform.h"
#include "tbb/concurrent_unordered_map.h"
//Dynamic parameter class
typedef tbb::concurrent_unordered_map<std::string, std::string> paramMap;
class Parameters : public Internal {
public:
	std::string get(std::string param);
	const int getDigit(std::string param);
	void set(std::string param, std::string value);
	inline static void parseBuffer(void* params, char* paramBuffer, int bytesRead) {
		Parameters* t = static_cast<Parameters*>(params);
		char* val = strchr(paramBuffer, '=');
		char* key = paramBuffer;
		if(!val)
			return;
		*(val) = '\0';
		val++;
		
		if(paramBuffer[bytesRead-2] == '\r') {
			paramBuffer[bytesRead-2] = '\0';
		} else if(paramBuffer[bytesRead-1] == '\n') {
			paramBuffer[bytesRead-1] = '\0';
		}
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