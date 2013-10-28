#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <atomic>
#include <tbb/task.h>
#include <tbb/concurrent_queue.h>
#include <asio.hpp>
#include "CPlatform.h"
#include "Schema.h"

class Webapp;

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
	int content_len = 0;
	std::vector<std::string*>* handler = NULL;
	webapp_str_t* input_chain[STRING_VARS];
	~Request() {
		if (socket != NULL) delete socket;
		if (v != NULL) delete v;
		if (headers != NULL) delete headers;
		if (handler != NULL) delete handler;
	};
	Request() {
		handler = new std::vector<std::string*>();
	}
};

#define INT_INTERVAL(i) sizeof(int)*i
class ServerHandler {
protected:
	std::atomic<unsigned int> numInstances;
	std::mutex handlerLock; //Mutex to control the allowance of new connection handling.
	tbb::empty_task* parent_task;
	tbb::concurrent_bounded_queue<Request*> requests;
public:
	friend class Server;
	friend class WebappTask;
	ServerHandler() {
		parent_task = new (tbb::task::allocate_root()) tbb::empty_task;
		parent_task->set_ref_count(1);
	}
	~ServerHandler() {
		parent_task->wait_for_all();
		parent_task->destroy(*parent_task);
	}
};

#endif