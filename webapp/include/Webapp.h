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
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <tbb/task.h>
#include "readerwriterqueue.h"
#include <tbb/concurrent_queue.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}


#include "Schema.h"

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
	std::atomic<unsigned int> aborted{0};
	moodycamel::ReaderWriterQueue<Request*> requests;
	std::mutex lock; //Mutex to control the allowance of new connection handling.
	std::mutex cv_mutex; //Mutex to allow ready cv
	std::condition_variable cv;
	RequestQueue() : requests(WEBAPP_DEFAULT_QUEUESIZE) {};
};

struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};


class TaskBase {
public:
	TaskBase(Webapp* handler) : _handler(handler) {};
	void start() {
		_worker = std::thread([this]{execute();});
	}
	void join() {
		_worker.join();
	}
	virtual void execute() = 0;
	static void start_thread(TaskBase* base) { base->execute(); };
protected:
	Webapp* _handler = NULL;
private:
	std::thread _worker;
};

class WebappTask : public TaskBase {
private:
	unsigned int _id;
	Sessions* _sessions;
	RequestQueue* _requests;
public:
	WebappTask(Webapp* handler, Sessions* sessions, RequestQueue* requests) 
		: TaskBase(handler), _sessions(sessions), _requests(requests) {
			start();
		};
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
	ctemplate::TemplateDictionary* getTemplate(const std::string& page);
	void refresh_templates();
	
	const std::string* basepath;
	const std::string* dbpath;

	Parameters* params;
	Database database;
	std::vector<Sessions*> sessions;

	std::atomic<unsigned int> waiting{0};
	unsigned int aborted = 0;

	tbb::concurrent_bounded_queue<Process*> background_queue;
	unsigned int background_queue_enabled = 1;
	std::vector<RequestQueue*> requests;
	int node_counter = 0;
};

#endif
