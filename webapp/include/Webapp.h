/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include "Platform.h"
#include <ctemplate/template.h>
#include <readerwriterqueue.h>
#include <leveldb/db.h>

#define WEBAPP_NUM_THREADS 8
#define WEBAPP_STATIC_STRINGS 10
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PARAM_ABORTED 1
#define WEBAPP_PARAM_BGQUEUE 2
#define WEBAPP_PARAM_TPLCACHE 3
#define WEBAPP_PORT_DEFAULT 5000
#define WEBAPP_DEFAULT_QUEUESIZE 1023

//APP specific definitions
//PROTOCOL SCHEMA DEFINITIONS
#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define PROTOCOL_LENGTH_SIZEINFO sizeof(uint16_t) * PROTOCOL_VARS

/**
 * Forward definitions for classes referenced in this header.
 * Related headers must be pulled into definitions that make use of
 * this file.
*/
class Database;
class Session;
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

struct _webapp_str_t {
    char* data = NULL;
    uint32_t len = 0;
    int allocated = 0;
};

struct webapp_str_t {
    char* data = NULL;
    uint32_t len = 0;
    int allocated = 0;
    webapp_str_t() {
        data = new char[1];
        len = 0;
        allocated = 1;
    }
	webapp_str_t(const char* s, uint32_t _len) {
		allocated = 1;
		len = _len;
		data = new char[_len];
		memcpy(data, s, _len);
	}
	webapp_str_t(uint32_t _len) {
		allocated = 1;
		len = _len;
		data = new char[_len];
	}
	webapp_str_t(const char* s) {
		allocated = 1;
		len = strlen(s);
		data = new char[len];
		memcpy(data, s, len);
	}
	webapp_str_t(webapp_str_t* other) {
		if(other == NULL) {
			len = 0;
			data = NULL;
		} else {
			len = other->len;
			data = other->data;
		}
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
		return std::string(data, len);
	}
	operator ctemplate::TemplateString const () const {
		return ctemplate::TemplateString(data, len);
	}
    operator leveldb::Slice const () const {
        return leveldb::Slice(data, len);
    }
	webapp_str_t& operator +=(const webapp_str_t& other) {
		uint32_t newlen = len + other.len;
		char* r = new char[newlen];
        memcpy(r, data, len);
        memcpy(r + len, other.data, other.len);
        if(allocated) delete[] data;
		len = newlen;
		allocated = 1;
		data = r;
		return *this;
	}
	webapp_str_t& operator=(const webapp_str_t& other) {
		if(this != &other) {
			char* r = new char[other.len];
			memcpy(r, other.data, other.len);
            if(allocated) delete[] data;
			data = r;
			allocated = 1;
			len = other.len;
		}
		return *this;
	}
    friend webapp_str_t operator+(const webapp_str_t& w1, const webapp_str_t& w2);
    friend webapp_str_t operator+(const char* lhs, const webapp_str_t& rhs);
    friend webapp_str_t operator+(const webapp_str_t& lhs, const char* rhs);
    void from_number(int num) {
        if(allocated) delete[] data;
        data = new char[21];
        allocated = 1;
        len = snprintf(data, 21, "%d", num);
    }
};

template <class T>
struct webapp_data_t : webapp_str_t {
	webapp_data_t(T _data) : webapp_str_t(sizeof(T)) {
		*(T*)(data) = _data;
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
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<Query*> queries;
    std::vector<Session*> sessions;
	int shutdown = 0;
	std::atomic<int> waiting{0};
	~Request();
};

/**
 * @brief TaskQueue provides the base locking mechanisms for queues, and support
 * for required webapp functionality.
 */
class TaskQueue {
public:
	unsigned int cleanupTask = 0;
	unsigned int shutdown = 0;
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
public:
	moodycamel::ReaderWriterQueue<T> queue;
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
	WebappTask(Webapp* handler, TaskQueue* q): _handler(handler), _q(q) {}
	virtual void Execute() = 0;
	virtual void Cleanup() = 0;
	Webapp* _handler;
	TaskQueue* _q;
};

/**
 * @brief BackgroundQueue provides a Lua VM with a Process queue
 */
class BackgroundQueue : public WebappTask, public LockedQueue<Process*> {
public:
	BackgroundQueue(Webapp* handler) :
		WebappTask(handler, this) { start(); }
	void Execute();
	void Cleanup() {}

};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
struct RequestQueue : public WebappTask, public LockedQueue<Request*> {
	Sessions* _sessions;
	ctemplate::TemplateCache* _cache = NULL;
	ctemplate::TemplateDictionary* baseTemplate = NULL;
	std::unordered_map<std::string, ctemplate::TemplateDictionary*> templates;
	RequestQueue(Webapp* handler, unsigned int id);
	~RequestQueue();
	void Cleanup();
	void Execute();
	std::string* RenderTemplate(const webapp_str_t& tpl);
};

typedef enum {SCRIPT_INIT, SCRIPT_QUEUE, SCRIPT_REQUEST, SCRIPT_HANDLERS} script_t;
#define SCRIPT_NAMES "SCRIPT_INIT", "SCRIPT_QUEUE","SCRIPT_REQUEST", "SCRIPT_HANDLERS"

class Webapp {
	std::array<webapp_str_t, WEBAPP_SCRIPTS> scripts;
	std::vector<WebappTask*> workers;
	
	//Parameters
	unsigned int aborted = 0;
	unsigned int background_queue_enabled = 1;
	unsigned int template_cache_enabled = 1;
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
	void process_request_async(Request* r, const asio::error_code&, std::size_t);
	asio::io_service& svc;
	asio::io_service::work wrk;

	//Worker/node variables
	unsigned int nWorkers = WEBAPP_NUM_THREADS - 1;
	unsigned int node_counter = 0;
public:
	std::unordered_map<std::string, std::string> templates;
	
	Webapp(asio::io_service& io_svc);
	~Webapp();
	
	//Public webapp methods
	void Start() { svc.run(); }
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
