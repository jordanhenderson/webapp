/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef SESSION_H
#define SESSION_H

#include "Platform.h"

#define WEBAPP_LEN_SESSIONID 1
#define WEBAPP_DEFAULT_SESSION_LIMIT 10000

struct webapp_str_t;
struct Request;

#define SESSION_STORE RamSession

struct SessionStore {
	std::string empty;
	std::string sessionid;
	virtual void create(const std::string& sessionid) = 0;
	virtual void store(const std::string& key, const std::string& value) = 0;
	virtual const std::string& get(const std::string& key) = 0;
	virtual int count() = 0;
	virtual ~SessionStore() {};
	SessionStore() : empty("") {}
};

//Default ram storage of session data.
typedef std::unordered_map<std::string, std::string> RamStorage;
class RamSession : public SessionStore {
	RamStorage _store;
public:
	void create(const std::string& sessionid);
	void store(const std::string& key, const std::string& value);
	const std::string& get(const std::string& key);
	int count();
};

typedef std::unordered_map<std::string, SessionStore*> SessionMap;
class Sessions {
	std::random_device rd;
	std::mt19937_64 rng;
	SessionMap session_map;
	std::string _node;
	int max_sessions = WEBAPP_DEFAULT_SESSION_LIMIT;
public:
	Sessions(unsigned int node_id) : rng(rd()) {
		_node = std::to_string(node_id).substr(0, WEBAPP_LEN_SESSIONID);
		session_map.reserve(max_sessions);
	}
	~Sessions();
	//Create a new session based on the request.
	SessionStore* get_session(const webapp_str_t& sessionid);
	SessionStore* new_session(Request* request);
	void SetMaxSessions(int value);
	void CleanupSessions();
};

#endif //SESSION_H
