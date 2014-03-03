/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include "Platform.h"
#include "WebappString.h"
#include "Session.h"
#include <readerwriterqueue.h>

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
    _webapp_str_t uri;
    _webapp_str_t host;
    _webapp_str_t user_agent;
    _webapp_str_t cookies;
    _webapp_str_t request_body;
	std::vector<std::string*> strings;
    _webapp_str_t* input_chain[STRING_VARS];
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
struct TaskQueue {
	unsigned int cleanupTask = 0;
	unsigned int shutdown = 0;
    std::atomic<unsigned int> aborted;
	unsigned int finished = 0;
	std::condition_variable cv;
	std::mutex cv_mutex;
	void notify() {cv.notify_one();}
    TaskQueue() : aborted(0) {}
    TaskQueue(TaskQueue&& other) :
        aborted(other.aborted.load()) {}
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
struct LockedQueue : public TaskQueue {
	moodycamel::ReaderWriterQueue<T> queue;
	//Each operation (enqueue, dequeue) must be done single threaded.
	//We need MP (Multi-Producer), so lock production.
	void enqueue(T i) {cv_mutex.lock(); queue.enqueue(i);
					   cv_mutex.unlock(); cv.notify_one();}
	T dequeue() { 
		T r = NULL;
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
protected:
	Webapp* _handler;
private:
	std::thread _worker;
	void handleCleanup();

public:
	void start();
	void join() { _worker.join(); }
    WebappTask(Webapp* handler, TaskQueue* q):
        _handler(handler), _worker(), _q(q) {}
    WebappTask(WebappTask&& other) :
        _worker(std::move(other._worker)),
        _handler(other._handler),
        _q(other._q) {}

    virtual ~WebappTask() {}
	virtual void Execute() = 0;
	virtual void Cleanup() = 0;
	TaskQueue* _q;
};

/**
 * @brief BackgroundQueue provides a Lua VM with a Process queue
 */
struct BackgroundQueue : public WebappTask, public LockedQueue<Process*> {
    BackgroundQueue(Webapp* handler) :
        WebappTask(handler, this) {}
    BackgroundQueue(BackgroundQueue&& q) : WebappTask(std::move(q)) {}
	void Execute();
	void Cleanup() {}

};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
struct RequestQueue : public WebappTask, public LockedQueue<Request*> {
    Sessions _sessions;
	ctemplate::TemplateCache* _cache = NULL;
	ctemplate::TemplateDictionary* baseTemplate = NULL;
	std::unordered_map<std::string, ctemplate::TemplateDictionary*> templates;
    RequestQueue(Webapp *handler)
        : WebappTask(handler, this), _sessions(handler) { Cleanup(); }
    RequestQueue(RequestQueue&& q) :
        WebappTask(std::move(q)),
        _sessions(std::move(q._sessions)),
        _cache(q._cache),
        baseTemplate(q.baseTemplate),
        templates(std::move(q.templates)) {}
	~RequestQueue();
	void Cleanup();
	void Execute();
	std::string* RenderTemplate(const webapp_str_t& tpl);  
};

typedef enum {SCRIPT_INIT, SCRIPT_QUEUE, SCRIPT_REQUEST, SCRIPT_HANDLERS} script_t;
#define SCRIPT_NAMES "SCRIPT_INIT", "SCRIPT_QUEUE","SCRIPT_REQUEST", "SCRIPT_HANDLERS"

template<class T>
struct WorkerArray {
    std::vector<T> workers;
    WorkerArray(){}
    void Cleanup() {
        for(auto& it: workers) {
            it.Cleanup();
        }
    }
    void Clean() {
        //Ensure each worker is aborted, waiting to be restarted.
        for (auto& it: workers) {
            TaskQueue* q = it._q;
            //Ensure no other thread is cleaning.
            q->cleanupTask = 0;

            //Abort the worker (aborts any new requests).
            q->aborted = 1;

            //Notify any blocked threads to process the next request.
            while(!q->finished) {
                q->cv.notify_one();
                //Sleep to allow thread to finish, then check again.
                std::this_thread::
                        sleep_for(std::chrono::milliseconds(100));
            }
            //Prevent any new requests from being queued.
            q->cv_mutex.lock();
        }
    }
    void Restart() {
        for (auto& it: workers) {
            it._q->aborted = 0;
            it._q->cv_mutex.unlock();
        }
    }
    void Start(Webapp* handler, int nWorkers) {
        for(unsigned int i = 0; i < nWorkers; i++)
            workers.emplace_back(handler);
        for(auto& it: workers) it.start();
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
    unsigned int nWorkers = WEBAPP_NUM_THREADS;
	unsigned int node_counter = 0;

public:
    leveldb::DB* db;
    const char* session_dir = WEBAPP_OPT_SESSION_DEFAULT;

    std::unordered_map<std::string, std::string> templates;
	
    Webapp(const char* session_dir, asio::io_service& io_svc);
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
