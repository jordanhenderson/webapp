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
			: asio::ip::tcp::socket(svc), timer(svc) {}
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
	LuaSocket(asio::io_service& svc) : socket(svc) {}
};

struct WorkerInit {
	const char* _script = NULL; //Worker script
	int port = 5000; //Worker operating port
	int request_method = 0; //Worker request handling method
	int request_size = 1; //LuaRequest size
	int queue_size = 100; //Size of requests to handle per queue
	int request_pool_size = 100; //Size of requests to initialize in the pool
	int static_strings = 4; //Amount of static strings to create
	int client_sockets = 1; //Use client sockets
	int templates_enabled = 1; //Enable template engine
	int templates_cache_enabled = 0; //Enable template caching
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
 * @brief LockedMap is a basic std::unordered_map wrapper that provides
 * a locking mutex.
*/
template<class Key, class Ty, class Hash, class Pred, class Alloc>
struct LockedMap : public std::unordered_map<Key, Ty, Hash, Pred, Alloc>
{
	LockedMap() {}
	~LockedMap() {}
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
 * mock Workeres without the need to start threads or VMs.)
*/
struct RequestBase : LockedQueue<Request> {
	asio::io_service svc;
	asio::io_service::work wrk;
	asio::ip::tcp::acceptor acceptor;
	//Client socket api
	asio::io_service client_svc;
	asio::io_service::work client_wrk;
	asio::ip::tcp::resolver resolver;

	/* Queue finished requests for reuse. */
	LockedQueue<Request> finished_requests;
	/* Concurrent request counter. */
	std::atomic<int> current_request {0};
	/* Current request pool. */
	std::atomic<int> current_pool {0};
	//TODO: Use deque when asio implements proper move/copy semantics.
	std::vector<Request*> request_pools;
	unsigned int request_pool_size;
	unsigned int request_size;

	RequestBase(unsigned int request_size,
				unsigned int queue_size,
				unsigned int request_pool_size) :
									LockedQueue(queue_size),
									request_size(request_size),
									finished_requests(queue_size),
									request_pool_size(request_pool_size),
									wrk(svc),
									client_wrk(client_svc),
									acceptor(svc),
									resolver(client_svc) {}
	~RequestBase();
	
	void accept_conn();
	void accept_conn_async(Request* r, const std::error_code&);
	void read_request(Request* r, int timeout_ms);
	void process_msgpack_request(Request* r, const std::error_code&,
							  std::size_t);
	void resolve_handler(LuaSocket* s, Request* r,
						const std::error_code& ec,
						asio::ip::tcp::resolver::iterator it);
	void connect_handler(LuaSocket* s, Request* r,
						 const std::error_code& ec,
						 asio::ip::tcp::resolver::iterator it);
	LuaSocket* create_socket(Request* r, const webapp_str_t& addr,
							 const webapp_str_t& port);
	void read_handler(LuaSocket* s, Request* r, webapp_str_t* output,
					  int timeout, const std::error_code& ec, size_t n_bytes);
	webapp_str_t* start_read(LuaSocket* s, Request* r, int bytes, int timeout);
	void write_handler(LuaSocket* s, webapp_str_t* buf,
					   const std::error_code& error,
					   size_t bytes_transferred);
	void start_write(LuaSocket* s, const webapp_str_t& buf);
};

/**
 * @brief Worker holds an instance of a request queue and Lua VM.
 * Additionally, Worker can be extended to provide additional worker
 * functionality. For example, LevelDB support.
 */
class Worker : public WorkerInit, public WebappTask, public RequestBase {
	webapp_str_t script;
	asio::ip::tcp::endpoint endpoint;
	std::thread svc_thread;
	std::thread client_socket_thread;
public:
	Sessions sessions;
	ctemplate::TemplateCache* _cache = NULL;
	ctemplate::TemplateDictionary* baseTemplate = NULL;
	std::unordered_map<std::string, ctemplate::TemplateDictionary*> templates;
	
	Worker(const WorkerInit& init);
	~Worker();
	void Start();
	void Stop();
	void Execute();
	void Cleanup();
	
	webapp_str_t* CompileScript(const webapp_str_t& filename);
	void RunScript(LuaParam* params, int n_params, const webapp_str_t& filename);
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
	void Clear()
	{
		Stop();
		for(auto it: workers) {
			delete it;
		}
		workers.clear();
	}
	
	void Add(const WorkerInit& init)
	{
		Worker* w = new Worker(init);
		workers.push_back(w);
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
friend class Worker;
	unsigned int aborted = 0;
	WorkerArray<Worker> workers;
	
	std::mutex cleanupLock;
	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	size_t db_count = 0;
	
public:
	//Keep track of templates
	std::unordered_map<std::string, std::string> templates;
	std::unordered_map<std::string, webapp_str_t*> scripts;
	std::unordered_map<std::string, leveldb::DB*> leveldb_databases;
/* Public Methods */
	Webapp() {}
	~Webapp();

	void Start();
	Database* CreateDatabase();
	Database* GetDatabase(size_t index);
	void DestroyDatabase(Database*);

	//Worker methods
	void StartCleanup();
	void FinishCleanup();
	void CreateWorker(const WorkerInit& init);

};

#endif //WEBAPP_H
