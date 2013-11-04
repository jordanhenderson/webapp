
#include <sha.h>
#include <filters.h>
#include <hex.h>
#include <osrng.h>
#include "Session.h"
#include "Server.h"

using namespace std;
using namespace CryptoPP;

Sessions::~Sessions() {
	//Delete maps within session_map
	LockableContainerLock<SessionMap> lock(session_map);
	for(SessionMap::iterator it = lock->begin();
		it != lock->end(); ++it) {
			it->second->destroy();
			delete it->second;
	}
}

SessionStore* Sessions::new_session(Request* request) {
	if (request->host.len == 0 || request->user_agent.len == 0) return NULL; //Cannot build unique session.

	SHA1 hash;
	string output;
	byte digest[SHA1::DIGESTSIZE];

	//Generate a random salt
	const unsigned int BLOCKSIZE = 12;
	byte pcbScratch[ BLOCKSIZE ];

	// Construction
	CryptoPP::AutoSeededRandomPool rng;

	// Random Block
	rng.GenerateBlock( pcbScratch, BLOCKSIZE );

	//Generate a basic random session ID.
	hash.Update((const byte*)request->host.data, request->host.len);
	hash.Update((const byte*)request->user_agent.data, request->user_agent.len);
	hash.Update(pcbScratch, BLOCKSIZE);
	hash.Final(digest);
	
	CryptoPP::HexEncoder encoder;
	encoder.Attach(new StringSink(output));
	encoder.Put(digest, sizeof(digest));
	encoder.MessageEnd();
	//Create a new session store if one does not already exist.
	{
		LockableContainerLock<SessionMap> lock(session_map);
		SessionMap::iterator it = lock->find(output);
		//Delete any existing map, if any (clean up possible existing orphan session data).
		if(it != lock->end()) {
			SessionStore* sto = it->second;
			sto->destroy();
			delete sto;
			lock->erase(it);
		}
	}
	
	SessionStore* session_store = new SESSION_STORE();
	//should use output
	session_store->create(output);
	{
		LockableContainerLock<SessionMap> lock(session_map);
		lock->insert(make_pair(output, session_store));
	}
	
	
	return session_store;
}

SessionStore* Sessions::get_session(const char* sessionid) {
	LockableContainerLock<SessionMap> lock(session_map);

	SessionMap::iterator it = lock->find(sessionid);
	if(it != lock->end())
		return it->second;

	return NULL;
}

