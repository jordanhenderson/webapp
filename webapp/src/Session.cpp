/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 *
 * This file makes use of headers from the OpenSSL project licensed under
 * BSD-Style Open Source Licenses. See relevant license files for further
 * information.
 */

#include "Session.h"
#include "Webapp.h"
#include <openssl/sha.h>

using namespace std;

Sessions::~Sessions() {
	//Delete maps within session_map
	for(SessionMap::iterator it = session_map.begin();
		it != session_map.end(); ++it) {
			delete it->second;
	}
}

SessionStore* Sessions::new_session(Request* request) {
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

	string str_hex = _node + string((char*)output_hex, 32);
	
	SessionMap::iterator it = session_map.find(str_hex);
	//If session map already exists, try to generate a new one.
	if (it != session_map.end()) return new_session(request);

	if(session_map.size() >= max_sessions) {
		//Sessions have filled up completely.
		//Ram needs to be increased, sessions moved to different storage (disk)
		//or server is being DOS'ed.
		//TODO: Alert server operators.
		//Try cleaning up empty sessions.
		CleanupSessions();
	}

	SessionStore* session_store = new SESSION_STORE();
	session_store->create(str_hex);
	session_map.insert({str_hex, session_store});
	
	return session_store;
}

/**
 * @brief Cleanup sessions removes empty sessions or (as a last resort)
 * removes the first session. This *should* never happen, as max_sessions
 * and available RAM should be appropriately adjusted to allow more sessions.
 * This function is also used to cull empty sessions.
 */
void Sessions::CleanupSessions() {
	auto it = session_map.begin();
	while(it != session_map.end()) {
		SessionStore* session = it->second;
		if(session->count() == 0) {
			delete session;
			session_map.erase(it++);
		} else {
			++it;
		}
	}
	//If still a problem - we have no other option than to destroy a session.
	if(session_map.size() >= max_sessions) {
		auto it = session_map.begin();
		SessionStore* session = it->second;
		delete session;
		session_map.erase(it);
	}
}

SessionStore* Sessions::get_session(const webapp_str_t& sessionid) {
	auto it = session_map.find(sessionid);
	if(it != session_map.end())
		return it->second;

	return NULL;
}

void Sessions::SetMaxSessions(int value) {
	max_sessions = value;
	session_map.reserve(value);
}
