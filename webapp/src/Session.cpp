#include "Session.h"
#include "Webapp.h"
#include "sha2.h"


using namespace std;

Sessions::~Sessions() {
	//Delete maps within session_map
	for(SessionMap::iterator it = session_map.begin();
		it != session_map.end(); ++it) {
			it->second->destroy();
			delete it->second;
	}
}

SessionStore* Sessions::new_session(Request* request) {
	if (request->host.len == 0 || request->user_agent.len == 0) return NULL; //Cannot build unique session.
	u_int8_t output[32];
	char output_hex[32];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, (u_int8_t*)request->host.data, request->host.len);
	SHA256_Update(&ctx, (u_int8_t*)request->user_agent.data, request->user_agent.len);

	//Calculate random number, add to hash.
	uniform_int_distribution<int> dis;
	int r = dis(rng);
	
	SHA256_Update(&ctx, (u_int8_t*)&r, sizeof(int));
	SHA256_Final(output, &ctx);
	const char* hex_lookup = "0123456789ABCDEF";
	char* p = output_hex;
	for (int i = 0; i != 16; i++) {
		*p++ = hex_lookup[output[i] >> 4];
		*p++ = hex_lookup[output[i] & 0x0F];
	}

	string str_hex = _node + string((char*)output_hex, 32);
	
	
	SessionMap::iterator it = session_map.find(str_hex);
	//Delete any existing map, if any (clean up possible existing orphan session data).
	if (it != session_map.end()) {
		SessionStore* sto = it->second;
		sto->destroy();
		delete sto;
		session_map.erase(it);
	}
	

	SessionStore* session_store = new SESSION_STORE();
	session_store->create(str_hex);
	//should use output
	session_map.insert(make_pair(str_hex, session_store));
	
	return session_store;
}

SessionStore* Sessions::get_session(webapp_str_t* sessionid) {
	SessionMap::iterator it = session_map.find(string(sessionid->data, sessionid->len));
	if(it != session_map.end())
		return it->second;

	return NULL;
}

