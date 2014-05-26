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
#include <leveldb/filter_policy.h>
#include "Platform.h"
#include "WebappString.h"
#include "Session.h"
#include "Webapp.h"
#include <openssl/sha.h>


webapp_str_t empty = webapp_str_t();

using namespace std;

webapp_str_t* DataStore::get(const webapp_str_t &key)
{
	webapp_str_t* val = NULL;
	leveldb::DB* db = sessions->db;
	if(db == NULL) return &empty;
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	
	for(it->Seek(key); it->Valid(); it->Next()) {
		leveldb::Slice it_key = it->key();
		leveldb::Slice it_value = it->value();
		if(it_key.compare(key) > 0) break;
		val = new webapp_str_t(it_value.data(), it_value.size());
	}
	delete it;

	//Fallback if value not found.
	if(val == NULL) return &empty;
	
	//Store the value temporarily (for cleanup purposes).
	cache(val);

	return val;
}

void DataStore::put(const webapp_str_t &key, const webapp_str_t &value)
{
	leveldb::DB* db = sessions->db;
	if(db != NULL) db->Put(leveldb::WriteOptions(), key, value);
}

void DataStore::cache(webapp_str_t* buf) 
{
	vals.push_back(buf);
}


DataStore::~DataStore()
{
	for(auto it: vals) delete it;
}

void DataStore::wipe(const webapp_str_t& key)
{
	leveldb::DB* db = sessions->db;
	if(db == NULL) return;
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	for(it->Seek(key); it->Valid(); it->Next()) {
		leveldb::Slice it_key = it->key();
		if(it_key.starts_with(key)) {
			db->Delete(leveldb::WriteOptions(), it_key);
		} else {
			break;
		}
	}
	delete it;
}

Session::Session(Sessions* sessions, const webapp_str_t &sid)
	: id(sid), store(sessions)
{
	if(id.len > 0) {
		//Update/write the stored session time.
		time_t current_time = time(0);
		int32_t diff = (int32_t)difftime(current_time, epoch);
		webapp_str_t str_diff;
		str_diff.from_number(diff);
		store.put("s_" + sid, str_diff);
	}
}

Session::~Session()
{
	if(id.len > 0) store.wipe("s_" + id);
}

webapp_str_t* Session::get(const webapp_str_t &key)
{
	webapp_str_t actual_key = "s_" + id + key;
	return store.get(actual_key);
}

void Session::put(const webapp_str_t &key, const webapp_str_t &value)
{
	store.put("s_" + id + key, value);
}

Sessions::Sessions() :
	rng(std::random_device {}())
{
}

void Sessions::Init(const webapp_str_t &path)
{
	auto& leveldbs = app->leveldb_databases;
	LockedMapLock lock(leveldbs);
	string path_str = path;
	auto it = leveldbs.find(path_str);
	if(it != leveldbs.end()) {
		db = it->second;
	} else {
		//Not found. Create new levelDB connection.
		leveldb::Options options;
		options.filter_policy = leveldb::NewBloomFilterPolicy(10);
		options.create_if_missing = true;
		string path_str = path;
		leveldb::DB::Open(options, path_str, &db);
		leveldbs.emplace(piecewise_construct,
				  forward_as_tuple(path_str), 
				  forward_as_tuple(db));
	}
}

Sessions::~Sessions()
{
	CleanupSessions();
}

int32_t Sessions::session_expiry()
{
	DataStore store(this);
	webapp_str_t session_exp = store.get("session_exp");
	int32_t n_session_exp = 0;
	if(session_exp.len == 0) {
		n_session_exp = SESSION_TIME_DEFAULT;
		session_exp.from_number(SESSION_TIME_DEFAULT);
		store.put("session_exp", session_exp);
	} else {
		n_session_exp =
				strntol(session_exp.data, session_exp.len);
	}
	return n_session_exp;
}

Session* Sessions::new_session(const webapp_str_t& uid)
{
	unsigned char output[SHA256_DIGEST_LENGTH];
	char output_hex[SESSIONID_SIZE + 1]; //(must be odd for loop below)
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, (unsigned char*)uid.data, uid.len);

	//Calculate random number, add to hash.
	uniform_int_distribution<int> dis;
	int r = dis(rng);

	SHA256_Update(&ctx, (unsigned char*)&r, sizeof(int));
	SHA256_Final(output, &ctx);
	const char* hex_lookup = "0123456789ABCDEF";
	char* p = output_hex;
	for (int i = 0; i < SESSIONID_SIZE / 2; i++) {
		*p++ = hex_lookup[output[i] >> 4];
		*p++ = hex_lookup[output[i] & 0x0F];
	}

	webapp_str_t session_id =
			webapp_str_t((char*)output_hex, SESSIONID_SIZE);
	
	Session* session = new Session(this, session_id);
	return session;
}

/**
 * @brief Cleanup sessions removes empty and/or exired sessions.
 * TODO: Call this when appropriate.
 */
void Sessions::CleanupSessions()
{
	if(db == NULL) return;
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	DataStore store(this);
	//Get the current time
	time_t current_time = time(0);
	double time_difference;
	struct tm tmp;
	for(it->SeekToFirst(); it->Valid(); it->Next()) {
		leveldb::Slice key = it->key();
		leveldb::Slice value = it->value();
		if(!key.starts_with("s_") || key.size() != 2 + SESSIONID_SIZE) continue;
		//Convert stored seconds to time_t
		int32_t n_session_time = strntol(value.data(), value.size());
		tmp = epoch_tm;
		tmp.tm_sec += n_session_time;
		time_t session_time = mktime(&tmp);

		time_difference = difftime(current_time, session_time);
		if(time_difference < 0 || time_difference > session_expiry()) {
			store.wipe(webapp_str_t(key));
		}
	}
	delete it;
}

Session* Sessions::get_raw_session(void)
{
	Session* session = new Session(this, "");
	return session;
}

Session* Sessions::get_cookie_session(const webapp_str_t& cookies)
{
	if(db == NULL) return NULL;
	if(cookies.len <
			sizeof(SESSIONID_STR) + SESSIONID_SIZE) {
		return NULL;
	}

	webapp_str_t session_id;
	char* cookie_str = cookies.data;
	char* cookie_end = cookie_str + cookies.len;
	for(; cookie_str < cookie_end; cookie_str++) {
		if(cookie_end - cookie_str < sizeof(SESSIONID_STR) +
				SESSIONID_SIZE) break;
		if(*cookie_str == ' ' || cookie_str == cookies.data) {
			if(*cookie_str == ' ') cookie_str++;
			if(strncmp(cookie_str, SESSIONID_STR "=", sizeof(SESSIONID_STR)))
				continue;
			//Found a session ID!
			session_id =
					webapp_str_t(cookie_str + sizeof(SESSIONID_STR)
								 , SESSIONID_SIZE);
			break;
		}
	}

	if(session_id.len < SESSIONID_SIZE) return NULL;

	return get_session(&session_id);
}

Session* Sessions::get_session(const webapp_str_t& session_id)
{
	webapp_str_t tmp_session_id = "s_" + session_id;
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	Session* session = NULL;
	for(it->Seek(tmp_session_id); it->Valid(); it->Next()) {
		leveldb::Slice key = it->key();
		leveldb::Slice value = it->value();
		if(key.compare(tmp_session_id) > 0) break;

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
			session = new Session(this, session_id);
		} else {
			db->Delete(leveldb::WriteOptions(), key);
		}
	}
	delete it;
	return session;
}
