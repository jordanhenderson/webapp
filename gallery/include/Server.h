#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 
#ifdef _WIN32
#define NOMINMAX
#endif
#include "fcgiapp.h"
#include "fcgios.h"
#include <atomic>
#include <tbb/task.h>
#include <tbb/concurrent_queue.h>

class ServerHandler {
protected:
	int shutdown_handler;
	std::atomic<int> numInstances;
	std::mutex handlerLock; //Mutex to control the allowance of new connection handling.
	tbb::empty_task* parent_task;
	tbb::concurrent_bounded_queue<FCGX_Request*> requests;
public:
	virtual void createWorker() = 0;
	friend class Server;
	friend class ServerTask;
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

class ServerTask : public tbb::task {
public: 
	ServerTask(FCGX_Request* request, ServerHandler* handler)
		: _request(request), _handler(handler)
	{};
	tbb::task* execute();
private:
	FCGX_Request* _request;
	ServerHandler* _handler;
};

class Server : public Internal {
	std::thread* listener_thread;
	ServerHandler* handler;
	static void listener(ServerHandler*, int sock);
public:
	Server(ServerHandler*);
	~Server();
	void join();
	void setHandler(ServerHandler*);
	static void run(ServerHandler*, int nThread, int sock);

	
};

#endif