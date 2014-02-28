/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 *
 * This file makes use of headers from the OpenSSL project licensed under
 * BSD-Style Open Source Licenses. See relevant license files for further
 * information.
 */

#include <leveldb/db.h>
#include "WebappString.h"
#include "Session.h"
#include "Webapp.h"
#include <openssl/sha.h>


using namespace std;

Session::Session(leveldb::DB* _db, const webapp_str_t &sid)
    : db(_db), session_id(sid) {
}

Session::~Session() {
	for(auto it: vals) delete it;
}

webapp_str_t* Session::get(const webapp_str_t &key) {
    webapp_str_t actual_key = session_id + key;
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->Seek(actual_key); it->Valid(); it->Next()) {
        leveldb::Slice key = it->key();
        leveldb::Slice value = it->value();
        if(key.compare(actual_key) > 0) break;
        webapp_str_t* val = new webapp_str_t(value.data(), value.size());
        vals.push_back(val);
        return val;
    }
    webapp_str_t* empty = new webapp_str_t();
    vals.push_back(empty);
    return empty;
}

void Session::store(const webapp_str_t &key, const webapp_str_t &value) {
    webapp_str_t actual_key = session_id + key;
    db->Put(leveldb::WriteOptions(), actual_key, value);
}

Sessions::Sessions(Webapp* _handler) :
    handler(_handler), rng(std::random_device{}()), db(_handler->db) {
}

Sessions::~Sessions() {
}

Session* Sessions::new_session(Request* request) {
	if (request->host.len == 0 || request->user_agent.len == 0) 
		return NULL;
	unsigned char output[32];
	char output_hex[32];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, (unsigned char*)request->host.data, 
		request->host.len);
	SHA256_Update(&ctx, (unsigned char*)request->user_agent.data, 
		request->user_agent.len);

	//Calculate random number, add to hash.
	uniform_int_distribution<int> dis;
	int r = dis(rng);
	
	SHA256_Update(&ctx, (unsigned char*)&r, sizeof(int));
	SHA256_Final(output, &ctx);
	const char* hex_lookup = "0123456789ABCDEF";
	char* p = output_hex;
	for (int i = 0; i != 16; i++) {
		*p++ = hex_lookup[output[i] >> 4];
		*p++ = hex_lookup[output[i] & 0x0F];
	}

    webapp_str_t session_id = webapp_str_t((char*)output_hex, SESSIONID_SIZE);
    //Clean up empty sessions.
    CleanupSessions();

    Session* session = new Session(db, session_id);
    request->sessions.push_back(session);
    return session;
}

/**
 * @brief Cleanup sessions removes empty and/or exired sessions.
 */
void Sessions::CleanupSessions() {

}

Session* Sessions::get_session(Request* request) {
    _webapp_str_t* cookies = &request->cookies;
    if(cookies->len <
            sizeof(SESSIONID_STR) + SESSION_NODE_SIZE + SESSIONID_SIZE) {
        return NULL;
    }

    webapp_str_t session_id;
    char* cookie_str = cookies->data;
    char* cookie_end = cookie_str + cookies->len;
    for(;cookie_str < cookie_end; cookie_str++) {
        size_t cookie_left = cookie_end - cookie_str;
        if(strncmp(cookie_str, SESSIONID_STR "=", cookie_left)) {
            if(cookie_left >= SESSION_NODE_SIZE + SESSIONID_SIZE) {
                //Found a session ID!
                session_id = webapp_str_t(cookie_str,
                                          SESSION_NODE_SIZE + SESSIONID_SIZE);

            } else {
                //Session ID not long enough.
                return NULL;
            }
        }
    }

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->Seek(session_id); it->Valid(); it->Next()) {
        leveldb::Slice key = it->key();
        leveldb::Slice value = it->value();
        if(key.compare(session_id) > 0) break;
        time_t current_time = time(0);
        int64_t session_time = strntol(value.data(), value.size());
        if(session_time > current_time) return NULL; //Session in the past?
        if(current_time - session_time > 7000) {
            Session* session = new Session(db, session_id);
            request->sessions.push_back(session);
            return session;
        }
    }
	return NULL;
}

