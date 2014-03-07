/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef SESSION_H
#define SESSION_H

#include "Platform.h"
#include <MemoryPool.h>

#define SESSIONID_STR "sessionid"
#define SESSIONID_SIZE 32
#define SESSION_TIME_DEFAULT 60*60*24*7

struct webapp_str_t;
struct Request;
class Webapp;
namespace leveldb {
class DB;
}

class DataStore {
	std::vector<webapp_str_t*> vals;
protected:
	leveldb::DB* db;
public:
	DataStore(leveldb::DB* db) : db(db) {}
	virtual ~DataStore();
	virtual webapp_str_t* get(const webapp_str_t& key);
	virtual void put(const webapp_str_t& key, const webapp_str_t& value);
};

struct Session : public DataStore {
	webapp_str_t session_id;
	void destroy();
	Session(leveldb::DB*, const webapp_str_t&);
	~Session() {}
	webapp_str_t* get(const webapp_str_t& key);
	void put(const webapp_str_t& key, const webapp_str_t& value);
};

class Sessions {
	Webapp* handler;
	std::mt19937_64 rng;
	leveldb::DB* db;
	int32_t session_expiry();
public:
	Sessions(Webapp* _handler);
	~Sessions();
	//Create a new session based on the request.
	void CleanupSessions();
	Session* new_session(Request* request);
	Session* get_session(Request* request);
};

#endif //SESSION_H
