/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include <readerwriterqueue.h>
#include <msgpack.h>
#include "Platform.h"
#include "WebappString.h"
#include "Database.h"
#include "Session.h"

//LevelDB uses 3 threads, leave room for that + server thread.
#define WEBAPP_MAX_THREADS 100
#define WEBAPP_STATIC_STRINGS 10
#define WEBAPP_SCRIPTS 4
#define WEBAPP_PARAM_PORT 0
#define WEBAPP_PARAM_ABORTED 1
#define WEBAPP_PARAM_TPLCACHE 2
#define WEBAPP_PARAM_LEVELDB 3
#define WEBAPP_PARAM_THREADS 4
#define WEBAPP_PARAM_REQUESTSIZE 5
#define WEBAPP_PARAM_CLIENTSOCKETS 6
#define WEBAPP_PORT_DEFAULT 5000
#define WEBAPP_DEFAULT_QUEUESIZE 1023
#define WEBAPP_OPT_SESSION 0
#define WEBAPP_OPT_SESSION_DEFAULT "session"
#define WEBAPP_OPT_PORT 1

extern Webapp* app;

class Webapp;

/**
 * @brief Provide name, pointer pairs to set as lua globals.
 */
struct LuaParam {
	const char* name;
	void* ptr;
};

struct Socket : public asio::ip::tcp::socket {
	uint16_t ctr = 0;
	asio::steady_timer timer;
	std::atomic<int> waiting {0}; //unfinished write() calls
	Socket(asio::io_service& svc)
			: asio::ip::tcp::socket(svc), timer(svc) {};
	void abort() {
		std::error_code ec;
		
		shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		close();
		timer.cancel();
	}
};

struct LuaSocket {
	int8_t status = 0;
	Socket socket;
	LuaSocket(asio::io_service& svc) : socket(svc) {};
};

struct RequestBase;
/**
 * @brief Required information per request.
 */
struct Request {
	webapp_str_t headers_buf;
	LuaSocket& socket_ref;
	void* lua_request = NULL; //request object set/tracked by VM.
	
	//Internal members (not lua accessible)
	LuaSocket s;
	std::vector<char> headers;
	//Per-request containers
	std::vector<webapp_str_t*> strings;
	std::vector<Query*> queries;
	std::vector<Session*> sessions;
	
	//Functions
	//Reset allows request objects to be reused.
	void reset(int destroy=0) 
	{
		s.socket.ctr = 0;
		if(!destroy) {
			headers.clear();
			headers.resize(9);
			headers_buf.data = headers.data();
			headers_buf.len = 0;
		}
		
		for(auto it: strings) delete it;
		for(auto it: queries) delete it;
		for(auto it: sessions) delete it;

		strings.clear();
		queries.clear();
		sessions.clear();
		
		if(lua_request != NULL) free(lua_request);
		lua_request = NULL;
	}
	
	~Request()
	{
		reset();
	}

	Request(asio::io_service& svc) : s(svc), socket_ref(s)
	{
		reset();
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
	}
	
	void enqueue(T* i)
	{
		queue.enqueue(i);
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
	virtual ~WebappTask() { Stop(); }
	virtual void Execute() = 0;
	virtual int IsAborted() = 0;
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
	virtual ~RequestBase();
	webapp_str_t* RenderTemplate(const webapp_str_t& tpl);
};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
struct RequestQueue : public WebappTask, RequestBase {
	RequestQueue() {}
	~RequestQueue() {}
	void Execute();
	int IsAborted();
};

template<class T>
struct WorkerArray {
	std::vector<T*> workers;
	unsigned int counter = 0;
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
			delete it;
		}
		workers.clear();
	}

	void Clean()
	{
		//Ensure each worker is aborted, waiting to be restarted.
		for (auto it: workers) {
			it->FinishTask();
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
	
	void Enqueue(Request* r) {
		unsigned int selected_node = counter++ % workers.size();
		workers[selected_node]->enqueue(r);
	}

	void Stop()
	{
		for(auto it: workers) {
			it->Stop();
		}
	}
};

class Webapp {
	std::unordered_map<std::string, webapp_str_t*> scripts;
	WorkerArray<RequestQueue> workers;
	
	//Parameters
	unsigned int aborted = 0;
	unsigned int template_cache_enabled = 1;
	unsigned int leveldb_enabled = 1;
	unsigned int port = WEBAPP_PORT_DEFAULT;
	unsigned int request_size = 0;
	unsigned int client_sockets = 1;
	int num_threads = 1;
	
	const char* session_dir = WEBAPP_OPT_SESSION_DEFAULT;
	std::mutex cleanupLock;

	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	std::atomic<size_t> db_count {0};

	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	asio::io_service& svc;
	asio::io_service::work wrk;
	
	//Client socket api
	std::thread client_socket_thread;
	asio::io_service client_svc;
	asio::io_service::work client_wrk;
	asio::ip::tcp::resolver resolver;
public:
	leveldb::DB* db = NULL;
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
	void Cleanup(unsigned int cleanupTask, unsigned int shutdown);
	
	webapp_str_t* CompileScript(const char* filename);
	void RunScript(LuaParam* params, int nArgs, const char* file);
	
	//Request methods
	void accept_conn();
	void accept_conn_async(Request* r, const std::error_code&);
	void process_header_async(Request* r, const std::error_code&, 
							  std::size_t);

	//Socket methods
	LuaSocket* create_socket();
	void destroy_socket(LuaSocket* s);
	asio::ip::tcp::resolver& get_resolver();
	
	//Cleanup methods.
	void Reload();
	void ToggleLevelDB();
};

#endif //WEBAPP_H
