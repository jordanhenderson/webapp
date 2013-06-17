#ifndef SERIALIZE_H
#define SERIALIZE_H
#include <vector>
#include <string>
#include <unordered_map>
#include "document.h"
#include "stringbuffer.h"
#include "Platform.h"

class Serializer {
private:
	rapidjson::Document data;
	rapidjson::StringBuffer buffer;
	void append(std::string&, rapidjson::Value&);
	
public:
	void append(std::vector<std::string>&);
	void append(std::string&);
	void append(std::unordered_map<std::string, std::string>& map);
	void append(std::vector<std::vector<std::string>>&);
	Serializer();
	std::string get(int type);
};

#endif