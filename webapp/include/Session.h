/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef SESSION_H
#define SESSION_H

#include "Platform.h"

#define SESSIONID_STR "sessionid"
#define SESSIONID_SIZE 32
#define SESSION_TIME_DEFAULT 60*60*24*7

struct webapp_str_t;
struct Request;
class Webapp;
namespace leveldb {
class DB;
}

extern webapp_str_t empty;

struct DataStore {
	DataStore() {}
	virtual ~DataStore();
	virtual webapp_str_t* get(const webapp_str_t& key);
	virtual void put(const webapp_str_t& key, const webapp_str_t& value);
	virtual void wipe(const webapp_str_t& key);
	virtual void cache(webapp_str_t* buf) = 0;
	virtual void clear_cache() = 0;
};

struct Session : DataStore {
	webapp_str_t session_id;
	std::vector<webapp_str_t*> vals;
	
	Session(const webapp_str_t& key);
	~Session();
	webapp_str_t* get(const webapp_str_t& key);
	void put(const webapp_str_t& key, const webapp_str_t& value);
	void cache(webapp_str_t* buf);
	void clear_cache();
};

class Sessions {
	std::mt19937_64 rng;
	int32_t session_expiry();
public:
	Sessions();
	~Sessions();
	void CleanupSessions();
	Session* new_session(const webapp_str_t& primary,
						 const webapp_str_t& secondary);
	Session* get_cookie_session(const webapp_str_t& cookies);
	Session* get_session(const webapp_str_t& id);
	Session* get_raw_session(void);
};

#endif //SESSION_H
