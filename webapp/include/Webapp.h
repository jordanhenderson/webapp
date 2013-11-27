#ifndef GALLERY_H
#define GALLERY_H

#include <asio.hpp>
#include "Platform.h"
#include "CPlatform.h"
#include "FileSystem.h"
#include "Parameters.h"
#include "Database.h"
#include "Session.h"
#include <ctemplate/template.h>
#include <tbb/compat/thread>
#include <tbb/task.h>
#include <tbb/concurrent_queue.h>
#include <tbb/compat/condition_variable>
#include "readerwriterqueue.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}


#include "Schema.h"

//Default atomic value
static unsigned int atomic_zero = 0;
#define ATOMIC_ZERO (tbb::atomic<unsigned int>&)atomic_zero


class Logging;
class Webapp;
//LuaParams are temporary containers to hold stack variables for lua scripts.
struct LuaParam {
	const char* name;
	void* ptr;
};

struct TemplateData {
	std::string name;
	FileData* data;
};

struct Request {
	asio::ip::tcp::socket* socket = NULL;
	std::vector<char>* v = NULL;
	std::vector<char>* headers = NULL;
	int amount_to_recieve = 0;
	int length = 0;
	int method = 0;
	webapp_str_t uri;
	webapp_str_t host;
	webapp_str_t user_agent;
	webapp_str_t cookies;
	webapp_str_t request_body;
	std::vector<std::string*> handler;
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<Query*> queries;
	~Request() {
		if (socket != NULL) delete socket;
		if (v != NULL) delete v;
		if (headers != NULL) delete headers;

	};

};

struct RequestQueue {
	tbb::atomic<unsigned int> aborted = ATOMIC_ZERO;
	moodycamel::ReaderWriterQueue<Request*> requests;
	tbb::mutex lock; //Mutex to control the allowance of new connection handling.
	tbb::mutex cv_mutex; //Mutex to allow ready cv
	std::condition_variable cv;
	RequestQueue() : requests(WEBAPP_DEFAULT_QUEUESIZE) {};
};

struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};


class TaskBase : public tbb::tbb_thread {
public:
	TaskBase(Webapp* handler) : _handler(handler), tbb::tbb_thread(start_thread, this) {};
	
	virtual void execute() = 0;
	static void start_thread(TaskBase* base) { base->execute(); };
protected:
	Webapp* _handler = NULL;
};

class WebappTask : public TaskBase {
private:
	unsigned int _id;
	Sessions* _sessions;
	RequestQueue* _requests;
public:
	WebappTask(Webapp* handler, Sessions* sessions, RequestQueue* requests) 
		: TaskBase(handler), _sessions(sessions), _requests(requests) {};
	void execute();
	friend class Webapp;
};

class BackgroundQueue : public TaskBase {
public:
	BackgroundQueue(Webapp* handler) : TaskBase(handler) {};
	void execute();
	friend class Webapp;
};

class CleanupTask : public tbb::task {
	RequestQueue* _requests = NULL;
	Webapp* _handler = NULL;
public:
	CleanupTask(Webapp* handler, RequestQueue* requests) : _handler(handler), _requests(requests) {};
	tbb::task* execute();
};

class Webapp : public Internal {
private:
	//template dictionary used by all 'content' templates.
	ctemplate::TemplateDictionary contentTemplates;

	//(content) template filename vector
	std::vector<std::string> contentList;

	//Client Template filedata vector (stores templates for later de-allocation).
	std::vector<TemplateData> clientTemplateFiles;
	std::vector<std::string> serverTemplateFiles;

	void process_thread(std::thread*);
	
	static int LuaWriter(lua_State* L, const void* p, size_t sz, void* ud);
	void runHandler(LuaParam* params, int nArgs, const char* filename);
	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();

	asio::io_service& svc;
	void processRequest(Request* r, int len);

	friend class WebappTask;
	friend class BackgroundQueue;
	unsigned int nWorkers;
public:
	CleanupTask* posttask = NULL;
	std::vector<TaskBase*> workers;
	Webapp(Parameters* params, asio::io_service& io_svc);
	~Webapp();
	ctemplate::TemplateDictionary* getTemplate(std::string& page);
	void refresh_templates();
	
	const std::string* basepath;
	const std::string* dbpath;

	Parameters* params;
	Database database;
	std::vector<Sessions*> sessions;

	tbb::atomic<unsigned int> waiting = ATOMIC_ZERO;
	unsigned int aborted = 0;

	tbb::concurrent_bounded_queue<Process*> background_queue;
	unsigned int background_queue_enabled = 1;
	std::vector<RequestQueue*> requests;
	int node_counter = 0;
};

#endif