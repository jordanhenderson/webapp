#ifndef SESSION_H
#define SESSION_H
#include "Platform.h"
#include "CPlatform.h"
#include "Container.h"
#define SESSION_STORE RamSession

struct Request;

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
	LockableContainer<RamStorage> _store;
public:
	void create(const std::string& sessionid);
	void store(const std::string& key, const std::string& value);
	const std::string& get(const std::string& key);
	void destroy();
};

typedef std::unordered_map<std::string, SessionStore*> SessionMap;
class Sessions {
private:
	LockableContainer<SessionMap> session_map;
public:
	~Sessions();
	//Create a new session based on the request.
	SessionStore* get_session(webapp_str_t* sessionid);
	SessionStore* new_session(Request* request);
};
#endif