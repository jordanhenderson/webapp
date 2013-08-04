#ifndef SERIALIZE_H
#define SERIALIZE_H
#include <vector>
#include <string>
#include <unordered_map>
#include "document.h"
#include "stringbuffer.h"
#include "Platform.h"
#include "Database.h"

class Serializer {
private:
	rapidjson::Document data;
	rapidjson::StringBuffer buffer;
	void append(std::string&, rapidjson::Value&);
	
public:
	void append(std::vector<std::string>&);
	void append(std::string&);
	void append(std::unordered_map<std::string, std::string>& map, rapidjson::Value* v=NULL );
	void append(std::vector<std::unordered_map<std::string, std::string>>& map);
	void append(std::vector<std::vector<std::string>>&);
	void append(const std::string& key, const std::string& value,  int push_back=1, rapidjson::Value* m=NULL);
	Serializer();
	std::string get(int type);

	//Query functions
	void append(Query& q);
};

#endif