/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include <readerwriterqueue.h>
#include <ctemplate/template.h>
#include <ctemplate/template_emitter.h>
#include <leveldb/db.h>
#include "Platform.h"
#include "WebappString.h"
#include "Database.h"
#include "Session.h"


//Set to the maximum amount of strings required to be passed from Lua in a single call
#define WEBAPP_STATIC_STRINGS 3
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
	int port = 0; //Worker operating port
	int request_method = 0; //Worker request handling method
	int request_size = 1; //LuaRequest size
	int client_sockets = 1; //Use client sockets
	int templates_enabled = 1; //Enable template engine
	int templates_cache_enabled = 0; //Enable template caching
	int queue_size = 100; //Size of requests to handle per queue
	int request_pool_size = 100; //Size of requests to initialize in the pool
};

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

template<class Key, class Ty>
class BaseLockedMap : public std::unordered_map<Key, Ty> {
	std::mutex mtx;
protected:
	std::unordered_map<Key, Ty> map;
public:
	void lock() 
	{
		mtx.lock();
	}
	void unlock() 
	{
		mtx.unlock();
	}
	
	template <class ...Args> 
	Ty* emplace(Args&&... args)
	{
		lock();
		auto p = std::unordered_map<Key, Ty>::emplace(std::forward<Args>(args)...);
		auto ref = &p.first->second;
		unlock();
		return ref;
	}
};

/**
 * @brief LockedMap is a basic std::unordered_map wrapper that provides
 * a locking mutex.
*/
template<class Key, class Ty> 
struct LockedMap : public BaseLockedMap<Key, Ty>
{
	LockedMap() {}
	~LockedMap() {}
};

template<class Key, class Ty>
struct LockedMap<Key, Ty*> : public BaseLockedMap<Key, Ty*>
{
	LockedMap() {}
	~LockedMap()
	{
		this->lock();
		for(auto it: this->map) {
			delete it.second;
		}
		this->unlock();
	}
	void clear() {
		this->lock();
		for(auto it: this->map) {
			delete it.second;
		}
		
		BaseLockedMap<Key, Ty*>::clear();
		this->unlock();
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
	std::atomic<unsigned int> aborted {0};
	std::condition_variable cv;
	std::mutex cv_mutex;
	unsigned long count = 0;
	moodycamel::ReaderWriterQueue<T*> queue;
	unsigned int size;
	
	/* Procedures */
	void notify() {
		cv.notify_one();
	}
	
	void enqueue(T* i)
	{
		if(aborted) return;
		queue.enqueue(i);
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			++count;
			cv.notify_one();
		}
	}
	
	T* dequeue()
	{
		if(aborted) return NULL;
		T* r = NULL;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			while (!count) cv.wait(lk);
			--count;
		}
		queue.try_dequeue(r);
		return r;
	}
	
	T* try_dequeue()
	{
		if(aborted) return NULL;
		T* r = NULL;
		queue.try_dequeue(r);
		if(r != NULL) {
			std::unique_lock<std::mutex> lk(cv_mutex);
			if(count) --count;
		}
		return r;
	}
	LockedQueue(unsigned int sz) : size(sz == 0 ? 100 : sz),
								   queue(size) 
	{}
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
	unsigned int request_size;
	unsigned int request_pool_size;

	RequestBase(unsigned int request_size,
				unsigned int queue_size,
				unsigned int request_pool_size);
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
	void reenqueue(Request* r);
};

/**
 * @brief Worker holds an instance of a request queue and Lua VM.
 * Additionally, Worker can be extended to provide additional worker
 * functionality. For example, LevelDB (Sessions) support.
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
	WorkerArray<Worker> workers;
public:
	std::atomic<unsigned int> aborted {0};
	//Hold objects shared between workers.
	LockedMap<std::string, webapp_str_t> scripts;
	LockedMap<std::string, std::string> templates;
	LockedMap<size_t, Database> databases;
	LockedMap<std::string, leveldb::DB*> leveldb_databases;
/* Public Methods */
	Webapp() {}
	~Webapp();

	void Start();
	
	//Worker methods
	void CreateWorker(const WorkerInit& init);
};

#endif //WEBAPP_H
