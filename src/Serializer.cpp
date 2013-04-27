#include "Serializer.h"
#include "writer.h"
using namespace std;
using namespace rapidjson;
Serializer::Serializer() {
	
	data.SetObject();
	Value d(kArrayType);
	
	data.AddMember("data", d, data.GetAllocator());
}


std::string Serializer::get(int type) {
	Writer<StringBuffer> writer(buffer);
	data.AddMember("type", type, data.GetAllocator());
	data.Accept(writer);
	
	return JSON_HEADER + string(buffer.GetString(), buffer.Size());
}

void Serializer::append(string& str) {
	Value& d = data["data"];
	if(is_number(str))
		d.PushBack(stoi(str), data.GetAllocator());
	else
		d.PushBack(str.c_str(), data.GetAllocator());
}

void Serializer::append(string& str, Value& value) {
	if(is_number(str))
		value.PushBack(stoi(str), data.GetAllocator());
	else
		value.PushBack(str.c_str(), data.GetAllocator());
}

void Serializer::append(unordered_map<string, string>& map) {
	Value& d = data["data"];
	Value m;
	m.SetObject();
	for (unordered_map<string, string>::const_iterator it = map.begin(); 
		it != map.end(); ++it) {
			m.AddMember(it->first.c_str(), it->second.c_str(), data.GetAllocator());
	}
	d.PushBack(m, data.GetAllocator());
}

void Serializer::append(unique_ptr<unordered_map<string, string>>& map) {
	append(*(map.get()));
	storedMaps.push_back(move(map));
}

void Serializer::append(vector<string>& vector) {
	Value&d = data["data"];
	Value v;
	v.SetArray();
	for(int i = 0; i < vector.size(); i++) {
		append(vector[i], v);
	}
	d.PushBack(v, data.GetAllocator());
	
}

void Serializer::append(vector<vector<string>>& vector) {
	Value& d = data["data"];
	Value v; 
	v.SetArray();
	
	for(int i = 0; i < vector.size(); i++) {
			Value r;
			r.SetArray();
			for(int j = 0; j < vector[i].size(); j++) {
				append(vector[i][j], r);
			}
			v.PushBack(r, data.GetAllocator());
		}
	d.PushBack(v, data.GetAllocator());
}
