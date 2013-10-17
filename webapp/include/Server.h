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
#include "Schema.h"

struct webapp_str_t {
	const char* data;
	int len;
};

struct Request {
	asio::ip::tcp::socket* socket;
	std::vector<char>* v;
	int amount_to_recieve;
	int length;
	int method;
	webapp_str_t uri;
	webapp_str_t host;
	webapp_str_t user_agent;
	webapp_str_t cookies;
	webapp_str_t request_body;
	int content_len;
	//Event vars.
	int read_len;
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<std::vector<char>*> buffers;
};

#define INT_INTERVAL(i) sizeof(int)*i
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