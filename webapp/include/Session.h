/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef SESSION_H
#define SESSION_H

#include "Platform.h"
#include "CPlatform.h"
#include "Schema.h"

#define SESSION_STORE RamSession

class SessionStore {
public:
	std::string empty;
	std::string sessionid;
	virtual void create(const std::string& sessionid) = 0;
	virtual void store(const std::string& key, const std::string& value) = 0;
	virtual const std::string& get(const std::string& key) = 0;
	virtual void destroy() = 0;
	SessionStore() : empty("") {};
};

//Default ram storage of session data.
typedef std::unordered_map<std::string, std::string> RamStorage;
class RamSession : public SessionStore {
private:
	RamStorage _store;
public:
	void create(const std::string& sessionid);
	void store(const std::string& key, const std::string& value);
	const std::string& get(const std::string& key);
	void destroy();
};

struct Request;

typedef std::unordered_map<std::string, SessionStore*> SessionMap;
class Sessions {
	std::random_device rd;
	std::mt19937_64 rng;
private:
	SessionMap session_map;
	std::string _node;
public:
	Sessions(unsigned int node_id) : rng(rd()) {
		_node = std::to_string(node_id).substr(0, WEBAPP_LEN_SESSIONID);
	};
	~Sessions();
	//Create a new session based on the request.
	SessionStore* get_session(webapp_str_t* sessionid);
	SessionStore* new_session(Request* request);
};
#endif //SESSION_H
