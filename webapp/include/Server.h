#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Webapp; 
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <atomic>
#include <tbb/task.h>
#include <tbb/concurrent_queue.h>
#include <asio.hpp>

struct webapp_str_t {
	const char* data;
	int len;
};

struct Request {
	asio::ip::tcp::socket* socket;
	std::vector<char>* v;
	const char* headers;
	int length;
	int method;
	webapp_str_t uri;
	//Event vars.
	int state;
};

class ServerHandler {
protected:
	int shutdown_handler;
	std::atomic<int> numInstances;
	std::mutex handlerLock; //Mutex to control the allowance of new connection handling.
	tbb::empty_task* parent_task;
	tbb::concurrent_bounded_queue<Request*> requests;
public:
	virtual void createWorker() = 0;
	friend class Server;
	friend class WebappTask;
	ServerHandler() {
		shutdown_handler = 0;
		parent_task = new (tbb::task::allocate_root()) tbb::empty_task;
		parent_task->set_ref_count(1);
	}
	~ServerHandler() {
		parent_task->wait_for_all();
		parent_task->destroy(*parent_task);
	}
};

#endif