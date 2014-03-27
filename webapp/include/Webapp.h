/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include <readerwriterqueue.h>
#include "Platform.h"
#include "WebappString.h"
#include "Database.h"
#include "Session.h"

#define WEBAPP_NUM_THREADS 2
#define WEBAPP_STATIC_STRINGS 10
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PARAM_ABORTED 1
#define WEBAPP_PARAM_BGQUEUE 2
#define WEBAPP_PARAM_TPLCACHE 3
#define WEBAPP_PORT_DEFAULT 5000
#define WEBAPP_DEFAULT_QUEUESIZE 1023
#define WEBAPP_OPT_SESSION 0
#define WEBAPP_OPT_SESSION_DEFAULT "session"
#define WEBAPP_OPT_PORT 1

//APP specific definitions
//PROTOCOL SCHEMA DEFINITIONS
#define PROTOCOL_VARS 6
#define STRING_VARS 5
#define PROTOCOL_LENGTH_SIZEINFO sizeof(int16_t) * PROTOCOL_VARS
typedef enum {SCRIPT_INIT, SCRIPT_QUEUE, SCRIPT_REQUEST, SCRIPT_HANDLERS} script_t;
#define SCRIPT_NAMES "SCRIPT_INIT", "SCRIPT_QUEUE","SCRIPT_REQUEST", "SCRIPT_HANDLERS"

extern Webapp* app;

class Webapp;

/**
 * @brief Provide name, pointer pairs to set as lua globals.
 */
struct LuaParam {
	const char* name;
	void* ptr;
};

/**
 * @brief Required information per request.
 */
struct Request {
	int method = 0;
	_webapp_str_t uri;
	_webapp_str_t host;
	_webapp_str_t user_agent;
	_webapp_str_t cookies;
	_webapp_str_t request_body;
	_webapp_str_t* input_chain[STRING_VARS];
	asio::ip::tcp::socket socket;
	std::vector<char> headers;
	std::vector<char> headers_body;
	std::vector<webapp_str_t*> strings;
	std::vector<Query*> queries;
	std::vector<Session*> sessions;
	int shutdown = 0;
	std::atomic<int> waiting {0};
	~Request();
	Request(asio::io_service& svc) :
		socket(svc)
	{
		headers.reserve(PROTOCOL_LENGTH_SIZEINFO);
	}
};

/**
 * @brief Process is used to pass requests to Background Queue.
 */
class Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
public:
	Process(webapp_str_t* f, webapp_str_t* v)
	{
		func = new webapp_str_t(f);
		v = new webapp_str_t(v);
	}
	~Process()
	{
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
struct LockedQueue {
	/* Members */
	unsigned int cleanupTask = 0;
	unsigned int shutdown = 0;
	std::atomic<unsigned int> aborted;
	unsigned int finished = 0;
	std::condition_variable cv;
	std::mutex cv_mutex;
	moodycamel::ReaderWriterQueue<T*> queue;
	
	/* Procedures */
	void notify() {
		cv.notify_one();
	}
	
	void FinishTask() {
		//Ensure no other thread is cleaning.
		cleanupTask = 0;

		//Abort the worker (aborts any new requests).
		aborted = 1;

		//Notify any blocked threads to process the next request.
		while(!finished) {
			cv.notify_one();
			//Sleep to allow thread to finish, then check again.
			std::this_thread::
				sleep_for(std::chrono::milliseconds(100));
		}
		//Prevent any new requests from being queued.
		cv_mutex.lock();
	}
	void RestartTask() {
		aborted = 0;
		cv_mutex.unlock();
	}
	
	//Each operation (enqueue, dequeue) must be done single threaded.
	//We need MP (Multi-Producer), so lock production.
	void enqueue(T* i)
	{
		cv_mutex.lock();
		queue.enqueue(i);
		cv_mutex.unlock();
		cv.notify_one();
	}
	
	T* dequeue()
	{
		T* r = NULL;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			while (!queue.try_dequeue(r) && !aborted) cv.wait(lk);
		}
		if(aborted) return NULL;
		return r;
	}
	LockedQueue() : aborted(0), queue(WEBAPP_DEFAULT_QUEUESIZE) {}
};

/**
 * @brief WebappTask provides a worker thread wrapper for each Lua VM.
 */
class WebappTask {
	std::thread _worker;
public:
	void Start();
	void Stop();
	WebappTask() : _worker() {}
	virtual ~WebappTask() { Stop(); };
	virtual void Execute() = 0;
	virtual void Cleanup() = 0;
	virtual int IsAborted() = 0;
};

/**
 * @brief BackgroundQueue provides a Lua VM with a Process queue
 */
struct BackgroundQueue : public WebappTask, LockedQueue<Process> {
	BackgroundQueue() {}
	void Execute();
	void Cleanup();
	int IsAborted();
};

/**
 * @brief RequestBase provides a base request object (allows Hooks to use
 * mock RequestQueues without the need to start threads or VMs.)
*/
struct RequestBase : LockedQueue<Request> {
	ctemplate::TemplateCache* _cache = NULL;
	ctemplate::TemplateDictionary* baseTemplate = NULL;
	std::unordered_map<std::string, ctemplate::TemplateDictionary*> templates;
	Sessions _sessions;
	
	RequestBase();
	webapp_str_t* RenderTemplate(const webapp_str_t& tpl);
	void Cleanup();
};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
struct RequestQueue : public WebappTask, RequestBase {
	RequestQueue() {}
	~RequestQueue();
	void Execute();
	void Cleanup();
	int IsAborted();
};

template<class T>
struct WorkerArray {
	std::vector<T*> workers;
	WorkerArray() {}
	~WorkerArray()
	{
		for (auto it : workers) {
			delete it;
		}
	}
	void Cleanup()
	{
		for(auto it: workers) {
			it->Cleanup();
		}
	}

	void Clean()
	{
		//Ensure each worker is aborted, waiting to be restarted.
		for (auto it: workers) {
			it->FinishTask();
		}
	}

	void Restart()
	{
		for (auto it: workers) {
			it->RestartTask();
		}
	}

	void Start(unsigned int nWorkers)
	{
		for (unsigned int i = 0; i < nWorkers; i++) {
			T* worker = new T();
			workers.emplace_back(worker);
			worker->Start();
		}
	}

	void Stop()
	{
		for(auto it: workers) {
			it->Stop();
		}
	}
};

class Webapp {
	std::array<webapp_str_t, WEBAPP_SCRIPTS> scripts;
	WorkerArray<RequestQueue> workers;
	WorkerArray<BackgroundQueue> bg_workers;

	//Parameters
	unsigned int aborted = 0;
	unsigned int background_queue_enabled = 1;
	unsigned int template_cache_enabled = 1;
	unsigned int port = WEBAPP_PORT_DEFAULT;

	const char* session_dir = WEBAPP_OPT_SESSION_DEFAULT;
	std::mutex cleanupLock;

	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	std::atomic<size_t> db_count {0};

	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_conn();
	void accept_conn_async(Request* r, const asio::error_code&);
	void process_header_async(Request* r, const asio::error_code&, 
							  std::size_t);
	void process_request_async(Request* r, const asio::error_code&, 
							  std::size_t);
	asio::io_service& svc;
	asio::io_service::work wrk;

	//Worker/node variables
	unsigned int nWorkers = WEBAPP_NUM_THREADS;
	unsigned int node_counter = 0;

public:
	leveldb::DB* db;
	std::unordered_map<std::string, std::string> templates;

	Webapp(const char* session_dir, unsigned int port, 
		   asio::io_service& io_svc);
	~Webapp();

	void Start();
	Database* CreateDatabase();
	Database* GetDatabase(size_t index);
	void DestroyDatabase(Database*);

	//Parameter Store
	void SetParamInt(unsigned int key, int value);
	int  GetParamInt(unsigned int key);

	//Worker methods
	void Cleanup(unsigned int* cleanupTask, unsigned int shutdown);
	
	void CompileScript(const char* filename, webapp_str_t* output);
	void RunScript(LuaParam* params, int nArgs, script_t script);

	//Cleanup methods.
	void Reload();
};

#endif //WEBAPP_H
