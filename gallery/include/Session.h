#ifndef SESSION_H
#define SESSION_H
#include "fcgiapp.h"
#include "Platform.h"
#include "Container.h"
#define SESSION_STORE RamSession

class SessionStore {
public:
	std::string sessionid;
	virtual void create(std::string& sessionid) = 0;
	virtual void store(const std::string& key, const std::string& value) = 0;
	virtual std::string get(const std::string& key) = 0;
	virtual void destroy() = 0;
};


//Default ram storage of session data.
typedef std::unordered_map<std::string, std::string> RamStorage;
class RamSession : public SessionStore {
private:
	LockableContainer<RamStorage> _store;
public:
	void create(std::string& sessionid);
	void store(const std::string& key, const std::string& value);
	std::string get(const std::string& key);
	void destroy();
};

typedef std::unordered_map<std::string, SessionStore*> SessionMap;
class Sessions {
private:
	LockableContainer<SessionMap> session_map;
public:
	~Sessions();
	//Create a new session based on the FCGX Request.
	SessionStore* get_session(const char* sessionid);
	SessionStore* new_session(const char* host, const char* user_agent);
};
#endif