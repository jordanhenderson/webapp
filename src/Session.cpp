
#include <sha.h>
#include <filters.h>
#include <hex.h>
#include <osrng.h>
#include "Session.h"
using namespace std;
using namespace CryptoPP;

Session::Session() {

	session_map = new LockableContainer<SessionMap>();

}

Session::~Session() {
	//Delete maps within session_map
	LockableContainerLock<SessionMap> lock(*session_map);
	for(SessionMap::iterator it = lock->begin();
		it != lock->end(); ++it) {
			it->second->destroy();
			delete it->second;
	}
	delete session_map;
}

SessionStore* Session::new_session(char* host, char* user_agent) {
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
	hash.Update((const byte*)host, strlen(host));
	hash.Update((const byte*)user_agent, strlen(user_agent));
	hash.Update(pcbScratch, BLOCKSIZE);
	hash.Final(digest);
	
	CryptoPP::HexEncoder encoder;
	encoder.Attach(new StringSink(output));
	encoder.Put(digest, sizeof(digest));
	encoder.MessageEnd();
	//Create a new session store if one 
	LockableContainerLock<SessionMap> lock(*session_map);
	SessionMap::iterator it = lock->find(output);
	//Delete any existing map, if any (clean up possible existing orphan session data).
	if(it != lock->end()) {
		SessionStore* sto = it->second;
		sto->destroy();
		delete sto;
		lock->erase(it);
	}
	std::hash<std::string> hashfn;
	std::size_t str_hash = hashfn(output);
	
	SessionStore* session_store = new SESSION_STORE();

	session_store->create(output);
	lock->insert(make_pair(output, session_store));
	
	return session_store;
}

SessionStore* Session::get_session(std::string& sessionid) {

	LockableContainerLock<SessionMap> lock(*session_map);

	SessionMap::iterator it = lock->find(sessionid);
	if(it != lock->end())
		return it->second;

	return NULL;
}
