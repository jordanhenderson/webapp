/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include "Platform.h"
#include "CPlatform.h"
#include "Database.h"
#include "Session.h"
#include "Schema.h"
#include <ctemplate/template.h>
#include "readerwriterqueue.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}

/**
 * @brief Provide name, pointer pairs to set as lua globals.
 */
struct LuaParam {
	const char* name;
	void* ptr;
};

/**
 * @brief Iteratable containers accessible by lua.
 */
struct LuaContainer {
	void* container = NULL;
	int type = 0;
};

/**
 * @brief Required information per request.
 */
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
	std::vector<std::string*> strings;
	std::vector<LuaContainer*> containers;
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<Query*> queries;
	~Request() {
		for(auto it: strings) delete it;
		for(auto it: queries) delete it;
		if (socket != NULL) delete socket;
		if (v != NULL) delete v;
		if (headers != NULL) delete headers;
	}
};

/**
 * @brief TaskQueue provides the base locking mechanisms for queues, and support
 * for required webapp functionality.
 */
class TaskQueue {
public:
	unsigned int cleanupTask = 0;
	std::atomic<unsigned int> aborted{0};
	unsigned int finished = 0;
	std::condition_variable cv;
	std::mutex cv_mutex;
	void notify() {cv.notify_one();}
};

/**
 * @brief Process is used to pass requests to Background Queue.
 */
struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};

/**
 * @brief LockedQueue is a bounded multi-thread safe queue that provides
 * signalling capabilities.
 * Uses moodycamel's ReaderWriterQueue (lock free queue)
 */
template<typename T>
class LockedQueue : public TaskQueue {
	moodycamel::ReaderWriterQueue<T> queue;
public:
	//Each operation (enqueue, dequeue) must be done single threaded.
	//We need MP (Multi-Producer), so lock production.
	void enqueue(T i) {cv_mutex.lock(); queue.enqueue(i);
					   cv_mutex.unlock(); cv.notify_one();}
	T dequeue() { 
		T r;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			while (!queue.try_dequeue(r) && !aborted) cv.wait(lk);
		}
		if(aborted) return NULL;
		return r; 
	}
    LockedQueue() : queue(WEBAPP_DEFAULT_QUEUESIZE) {}
};

class Webapp;
/**
 * @brief WebappTask provides a worker thread wrapper for each Lua VM.
 */
class WebappTask {
	friend class Webapp;
	std::thread _worker;
	void handleCleanup();
public:
	void start();
	void join() { _worker.join(); }
	static void start_thread(WebappTask* task) { task->execute(); }
	WebappTask(Webapp* handler, TaskQueue* q): _handler(handler),
		_q(q) { start(); }
	virtual void execute() = 0;
	virtual void Cleanup() = 0;

protected:
	Webapp* _handler;
	TaskQueue* _q;
};

/**
 * @brief BackgroundQueue provides a Lua VM with a Process queue
 */
class BackgroundQueue : public WebappTask {
	LockedQueue<Process*> _lq;
public:
	BackgroundQueue(Webapp* handler) :
		WebappTask(handler, &_lq) {}
	void execute();
	void Cleanup() {}
};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
class RequestQueue : public WebappTask {
	Sessions _sessions;
	ctemplate::TemplateCache _cache;
	LockedQueue<Request*> _rq;
public:
	RequestQueue(Webapp* handler, unsigned int id) :
		WebappTask(handler, &_rq), _sessions(id) {}
	void execute();
	void enqueue(Request* r) { _rq.enqueue(r); }
	void Cleanup();
};

typedef enum {SCRIPT_INIT, SCRIPT_QUEUE, SCRIPT_REQUEST, SCRIPT_HANDLERS} script_t;
static const char* script_t_names[] = {"SCRIPT_INIT", "SCRIPT_QUEUE",
	"SCRIPT_REQUEST", "SCRIPT_HANDLERS"};

class Webapp {
	std::array<webapp_str_t, WEBAPP_SCRIPTS> scripts;
	std::vector<WebappTask*> workers;

	//Keep a clean template for later duplication.
	ctemplate::TemplateDictionary cleanTemplate;
	
	//Parameters
	unsigned int aborted = 0;
	unsigned int background_queue_enabled = 1;
	unsigned int port = WEBAPP_PORT_DEFAULT;
	
	std::mutex cleanupLock;
	
	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	std::atomic<size_t> db_count{0};

	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_conn();
	void accept_conn_async(asio::ip::tcp::socket*, const asio::error_code&);
	void process_header_async(Request *r, const asio::error_code&, std::size_t);
	void process_request(Request* r, size_t len);
	void process_request_async(Request* r, const asio::error_code&, std::size_t);
	asio::io_service& svc;
	asio::io_service::work wrk;

	//Worker/node variables
	unsigned int nWorkers = WEBAPP_NUM_THREADS - 1;
	unsigned int node_counter = 0;
public:
	Webapp(asio::io_service& io_svc);
	~Webapp();
	
	//Public webapp methods
	void Start() { svc.run(); }
	ctemplate::TemplateDictionary* GetTemplate();
	Database* CreateDatabase();
	Database* GetDatabase(size_t index);
	void DestroyDatabase(Database*);
	void CompileScript(const char* filename, webapp_str_t* output);
	void SetParamInt(unsigned int key, int value);
	int GetParamInt(unsigned int key);
	void RunScript(LuaParam* params, int nArgs, script_t script);

	//Worker methods
	void StartWorker(WebappTask*);
	
	//Cleanup methods.
	void refresh_templates();
	void reload_all();
	int start_cleanup(TaskQueue*);
	void perform_cleanup();
};

#endif //WEBAPP_H
