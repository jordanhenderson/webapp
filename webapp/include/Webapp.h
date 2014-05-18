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
		cancel(ec);
		timer.cancel();
		shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		close(ec);
	}
};	

struct LuaSocket {
	int8_t status = 0;
	Socket socket;
	LuaSocket(asio::io_service& svc) : socket(svc) {};
};

struct WorkerInit {
	webapp_str_t script; //Worker script
	int port; //Worker operating port
	int request_method; //Worker request handling method
	int request_size; //LuaRequest size
	int queue_size; //Size of requests to handle per queue
	int request_pool_size; //Size of requests to initialize in the pool
	int static_strings; //Amount of static strings to create
	int client_sockets; //Use client sockets
	int templates_enabled;
	
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
	
	//Functions
	//Reset allows request objects to be reused.
	void reset(int destroy=0) 
	{
		s.socket.ctr = 0;
		if(!destroy) {
			if(headers_buf.len > 255) {
				headers.resize(9);
			}
			headers_buf.data = headers.data();
			headers_buf.len = 0;
		} else {
			if(lua_request != NULL) free(lua_request);
			lua_request = NULL;
		}
	}
	
	~Request()
	{
		reset(1);
	}

	Request(asio::io_service& svc);
};

/**
 * @brief LockedQueue is a bounded multi-thread safe queue that provides
 * signalling capabilities.
 * Uses moodycamel's ReaderWriterQueue (lock free queue)
 */
template<typename T>
struct LockedQueue {
	/* Members */
	std::atomic<unsigned int> aborted {0};
	std::condition_variable cv;
	std::mutex cv_mutex;
	moodycamel::ReaderWriterQueue<T*> queue;
	
	/* Procedures */
	void notify() {
		cv.notify_one();
	}
	
	void enqueue(T* i)
	{
		{
			std::lock_guard<std::mutex> lk(cv_mutex);
			queue.enqueue(i);
		}
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
	
	T* try_dequeue()
	{
		T* r = NULL;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			queue.try_dequeue(r);
			if(aborted) return NULL;
			return r;
		}
	}
	LockedQueue(unsigned int size) : queue(size) {}
};

/**
 * @brief WebappTask provides a worker thread wrapper for each Lua VM.
 */
class WebappTask {
	std::thread _worker;
public:
	virtual void Start();
	virtual void Stop();
	WebappTask() : _worker() {}
	virtual ~WebappTask() { Stop(); }
	virtual void Execute() = 0;
};

/**
 * @brief RequestBase provides a base request object (allows Hooks to use
 * mock RequestQueues without the need to start threads or VMs.)
*/
struct RequestBase : LockedQueue<Request> {
	/* Queue finished requests for reuse. */
	LockedQueue<Request> finished_requests;
	/* Concurrent request counter. */
	std::atomic<int> current_request {0};
	/* Current request pool. */
	std::atomic<int> current_pool {0};
	std::vector<std::vector<Request>> request_pools;

	RequestBase(unsigned int size) : LockedQueue(size), 
									 finished_requests(size) {};
	~RequestBase() {};
	
	void accept_conn();
	void accept_conn_async(Request* r, const std::error_code&);
	void read_request(Request* r, int timeout_ms);
	void process_msgpack_request(Request* r, const std::error_code&, 
							  std::size_t);
};

/**
 * @brief RequestQueue provides a Lua VM with a session container, template
 * cache and queue for Requests.
 */
class RequestQueue : public WorkerInit, public WebappTask, public RequestBase {
	asio::io_service svc;
	asio::io_service::work wrk;
	std::thread svc_thread;
	asio::ip::tcp::endpoint endpoint;
	asio::ip::tcp::acceptor acceptor;
	
	//Client socket api
	std::thread client_socket_thread;
	asio::io_service client_svc;
	asio::io_service::work client_wrk;
	asio::ip::tcp::resolver resolver;
	
	leveldb::DB* db = NULL;
public:
	Sessions _sessions;
	ctemplate::TemplateCache* _cache = NULL;
	ctemplate::TemplateDictionary* baseTemplate = NULL;
	std::unordered_map<std::string, ctemplate::TemplateDictionary*> templates;
	
	RequestQueue(const WorkerInit& init);
	~RequestQueue();
	void Start();
	void Stop();
	void Execute();
	void CompileScript(const webapp_str_t& filename);
	void RunScript(LuaParam* params, int n_params, const webapp_str_t& filename);
	void Cleanup();
	webapp_str_t* RenderTemplate(const webapp_str_t& tpl);

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
		Stop();
		for(auto it: workers) {
			delete it;
		}
		workers.clear();
	}
	
	void Add(WorkerInit info)
	{
		workers.emplace_back(info);
	}

	void Start()
	{
		for(auto it: workers) it->Start();
	}

	void Stop()
	{
		for(auto it: workers) {
			it->Stop();
		}
	}
};

class Webapp {
friend class Request;
	unsigned int aborted = 0;
	std::unordered_map<std::string, webapp_str_t*> scripts;
	WorkerArray<RequestQueue> workers;
	
	std::mutex cleanupLock;
	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	std::atomic<size_t> db_count {0};
	
public:
	std::unordered_map<std::string, std::string> templates;
	
/* Public Methods */
	Webapp() {};
	~Webapp();

	void Start();
	Database* CreateDatabase();
	Database* GetDatabase(size_t index);
	void DestroyDatabase(Database*);

	//Worker methods
	void StartCleanup();
	void FinishCleanup();
	void CreateWorker(const WorkerInit& init);
	
	//Socket methods
	LuaSocket* create_socket();
	void destroy_socket(LuaSocket* s);
	asio::ip::tcp::resolver& get_resolver();
	
};

#endif //WEBAPP_H
