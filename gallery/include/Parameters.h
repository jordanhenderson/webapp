#ifndef PARAMETERS_H
#define PARAMETERS_H
#include "Platform.h"
#include "Container.h"
//Dynamic parameter class
typedef std::unordered_map<std::string, std::string> ParamMap;
class Parameters : public Internal {
public:
	std::string get(const std::string& param);
	const int getDigit(const std::string& param);
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
	~Parameters();
private:
	LockableContainer<ParamMap> params;
};


#endif