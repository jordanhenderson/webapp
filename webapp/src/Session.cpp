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
#include "Platform.h"
#include "WebappString.h"
#include "Session.h"
#include "Webapp.h"
#include <openssl/sha.h>


using namespace std;

webapp_str_t* DataStore::get(const webapp_str_t &key) {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    webapp_str_t* val = NULL;
    for(it->Seek(key); it->Valid(); it->Next()) {
        leveldb::Slice it_key = it->key();
        leveldb::Slice it_value = it->value();
        if(it_key.compare(key) > 0) break;
        val =
                new webapp_str_t(it_value.data(), it_value.size());
    }
    delete it;

    if(val == NULL) val = new webapp_str_t();
    vals.push_back(val);

    return val;
}

void DataStore::put(const webapp_str_t &key, const webapp_str_t &value) {
    db->Put(leveldb::WriteOptions(), key, value);
}

DataStore::~DataStore() {
    for(auto it: vals) delete it;
}

Session::Session(leveldb::DB* db, const webapp_str_t &sid)
    : DataStore(db), session_id(sid) {
    //Update/write the stored session time.
    time_t current_time = time(0);
    int32_t diff = difftime(current_time, epoch);
    webapp_str_t str_diff;
    str_diff.from_number(diff);
    DataStore::put(sid, str_diff);
}

webapp_str_t* Session::get(const webapp_str_t &key) {
    webapp_str_t actual_key = session_id + key;
    return DataStore::get(actual_key);
}

void Session::destroy() {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for(it->Seek(session_id); it->Valid(); it->Next()) {
        leveldb::Slice it_key = it->key();
        if(it_key.starts_with(session_id)) {
            db->Delete(leveldb::WriteOptions(), it_key);
        } else {
            break;
        }
    }
    delete it;
}

void Session::put(const webapp_str_t &key, const webapp_str_t &value) {
    webapp_str_t actual_key = session_id + key;
    DataStore::put(actual_key, value);
}

Sessions::Sessions(Webapp* _handler) :
    handler(_handler), rng(std::random_device{}()), db(_handler->db) {
}

Sessions::~Sessions() {
}

int32_t Sessions::session_expiry() {
    DataStore s(db);
    webapp_str_t session_exp = s.get("session_exp");
    int32_t n_session_exp = 0;
    if(session_exp.len == 0) {
        n_session_exp = SESSION_TIME_DEFAULT;
        session_exp.from_number(SESSION_TIME_DEFAULT);
        s.put("session_exp", session_exp);
    } else {
        n_session_exp =
                strntol(session_exp.data, session_exp.len);
    }
    return n_session_exp;
}

Session* Sessions::new_session(Request* request) {
	if (request->host.len == 0 || request->user_agent.len == 0) 
		return NULL;
    unsigned char output[SHA256_DIGEST_LENGTH];
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
    for (int i = 0; i <= 16; i++) {
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
            sizeof(SESSIONID_STR) + SESSIONID_SIZE) {
        return NULL;
    }

    webapp_str_t session_id;
    char* cookie_str = cookies->data;
    char* cookie_end = cookie_str + cookies->len;
    for(;cookie_str < cookie_end; cookie_str++) {
        if(cookie_end - cookie_str < sizeof(SESSIONID_STR) +
               SESSIONID_SIZE) break;
        if(*cookie_str == ' ' || cookie_str == cookies->data) {
            if(*cookie_str == ' ') cookie_str++;
            if(strncmp(cookie_str, SESSIONID_STR "=", sizeof(SESSIONID_STR)))
                continue;
            //Found a session ID!
            //Session ID starts at sessionid=X12345.... where X is node ID.
            session_id =
                    webapp_str_t(cookie_str + sizeof(SESSIONID_STR)
                                 , SESSIONID_SIZE);
            break;
        }
    }

    if(session_id.len < SESSIONID_SIZE) return NULL;

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    Session* session = NULL;
    for(it->Seek(session_id); it->Valid(); it->Next()) {
        leveldb::Slice key = it->key();
        leveldb::Slice value = it->value();
        if(key.compare(session_id) > 0) break;

        //Get the current time
        time_t current_time = time(0);
        //Convert stored seconds to time_t
        int32_t n_session_time = strntol(value.data(), value.size());
        struct tm tmp = epoch_tm;
        tmp.tm_sec += n_session_time;
        time_t session_time = mktime(&tmp);

        double time_difference = difftime(current_time, session_time);

        if(time_difference < 0) return NULL; //Session in the past?
        if(time_difference < session_expiry()) {
            session = new Session(db, session_id);
            request->sessions.push_back(session);
        } else {
            db->Delete(leveldb::WriteOptions(), key);
        }
    }
    delete it;
    return session;
}

