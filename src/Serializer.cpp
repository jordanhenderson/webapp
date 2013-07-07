#include "Serializer.h"
#include "writer.h"
using namespace std;
using namespace rapidjson;
Serializer::Serializer() {
	
	data.SetObject();
	Value d(kArrayType);
	
	data.AddMember("data", d, data.GetAllocator());
}


string Serializer::get(int type) {
	Writer<StringBuffer> writer(buffer);
	data.AddMember("type", type, data.GetAllocator());
	data.Accept(writer);
	
	return string(buffer.GetString(), buffer.Size());
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

//if push_back==1, push onto data when complete.
void Serializer::append(const string& key, const string& value,  int push_back, Value* m) {
	Value* actualMap;
	Value _m;
	if(m == NULL) actualMap = &_m;
	else actualMap = m;
	if(!actualMap->IsObject())
		actualMap->SetObject();

	Value f, s;
	f.SetString(key.c_str(), key.length(), data.GetAllocator());
	if(is_number(value)) s.SetInt(stoi(value));
	else s.SetString(value.c_str(), value.length(), data.GetAllocator());
	actualMap->AddMember(f, s, data.GetAllocator());
	if(push_back)
		&data["data"].PushBack(*actualMap, data.GetAllocator());

}

void Serializer::append(unordered_map<string, string>& map, Value* v) {
	Value* d;
	if(v == NULL) d = &data["data"];
	else d = v;

	Value m;
	m.SetObject();
	for (unordered_map<string, string>::const_iterator it = map.begin(); 
		it != map.end(); ++it) {
			append(it->first, it->second, 0, &m);	
	}
	d->PushBack(m, data.GetAllocator());
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

void Serializer::append(vector<unordered_map<string, string>>& vec) {
	if(!vec.size()>0)
		return;
	Value& d = data["data"];
	Value v; 
	v.SetArray();
	for(unordered_map<string,string> m: vec) {
		append(m, &v);
	}
	d.PushBack(v, data.GetAllocator());
}