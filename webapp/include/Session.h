/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef SESSION_H
#define SESSION_H

#include "Platform.h"

#define SESSION_NODE_SIZE 1
#define SESSIONID_STR "sessionid"
#define SESSIONID_SIZE 32

struct webapp_str_t;
struct Request;
class Webapp;
namespace leveldb {
    class DB;
}

class Session {
    leveldb::DB* db;
public:
    webapp_str_t session_id;
    std::vector<webapp_str_t*> vals;
    Session(leveldb::DB*, const webapp_str_t&);
    ~Session();
    webapp_str_t* get(const webapp_str_t& key);
    void store(const webapp_str_t& key, const webapp_str_t& value);
};

class Sessions {
	Webapp* handler;
	webapp_str_t _node;
	std::random_device rd;
	std::mt19937_64 rng;
    leveldb::DB* db;
public:
    int status = 0;
    Sessions(Webapp* _handler, unsigned int node_id);
	~Sessions();
	//Create a new session based on the request.
	void CleanupSessions();
    Session* new_session(Request* request);
    Session* get_session(Request* request);
};

#endif //SESSION_H
