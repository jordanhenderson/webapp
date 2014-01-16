/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include "Platform.h"
#include <asio.hpp>
#include <ctemplate/template.h>
#include <readerwriterqueue.h>

#define WEBAPP_NUM_THREADS 8
#define WEBAPP_STATIC_STRINGS 10
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PARAM_ABORTED 1
#define WEBAPP_PARAM_BGQUEUE 2
#define WEBAPP_PORT_DEFAULT 5000
#define WEBAPP_DEFAULT_QUEUESIZE 1023

//APP specific definitions
//PROTOCOL SCHEMA DEFINITIONS
#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define PROTOCOL_LENGTH_SIZEINFO sizeof(int) * PROTOCOL_VARS

/**
 * Forward definitions for classes referenced in this header.
 * Related headers must be pulled into definitions that make use of
 * this file.
*/
class Database;
class Sessions;
class Query;
class Webapp;

/**
 * @brief Provide name, pointer pairs to set as lua globals.
 */
struct LuaParam {
	const char* name;
	void* ptr;
};

struct webapp_str_t {
	char* data = NULL;
	long long len = 0;
	int allocated = 0;
	webapp_str_t() {}
	webapp_str_t(const char* s, long long len) {
		allocated = 1;
		data = new char[len];
		memcpy(data, s, len);
	}
	webapp_str_t(long long _len) {
		allocated = 1;
		data = new char[len];
		_len = len;
	}
	webapp_str_t(webapp_str_t* other) {
		allocated = 1;
		len = other->len;
		data = new char[len];
		memcpy(data, other->data, len);
	}
	webapp_str_t(const char* s) {
		allocated = 1;
		len = strlen(s);
		data = new char[len];
		memcpy(data, s, len);
	}
	webapp_str_t(const webapp_str_t& other) {
		allocated = 1;
		len = other.len;
		data = new char[len];
		memcpy(data, other.data, len);
	}
	~webapp_str_t() {
		if(allocated) delete[] data;
	}
	operator std::string const () const {
		if(data == NULL) return std::string("");
		return std::string(data, len);
	}
	operator ctemplate::TemplateString const () const {
		if(data == NULL) return ctemplate::TemplateString("");
		return ctemplate::TemplateString(data, len);
	}
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
	std::vector<ctemplate::TemplateDictionary*> dicts;
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<Query*> queries;
	~Request();
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
class Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
public:
	Process(webapp_str_t* f, webapp_str_t* v) {
		func = new webapp_str_t(f);
		v = new webapp_str_t(v);
	}
	~Process() {
		if(func != NULL) delete func;
		if(vars != NULL) delete vars;
	}
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
	WebappTask(Webapp* handler, TaskQueue* q): _handler(handler),
		_q(q) {}
	virtual void Execute() = 0;
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
		WebappTask(handler, &_lq) { start(); }
	void Execute();
	void Cleanup() {}
};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
class RequestQueue : public WebappTask {
	Sessions* _sessions;
	ctemplate::TemplateCache* _cache = NULL;
	LockedQueue<Request*> _rq;
public:
	RequestQueue(Webapp* handler, unsigned int id);
	~RequestQueue();
	void Cleanup();
	void Execute();
	void enqueue(Request* r) { _rq.enqueue(r); }
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
	std::unordered_map<long long, Database*> databases;
	std::atomic<long long> db_count{0};

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
	Database* GetDatabase(long long index);
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
