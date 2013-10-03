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

class ServerHandler {
protected:
	int shutdown_handler;
	std::atomic<int> numInstances;
	std::mutex handlerLock; //Mutex to control the allowance of new connection handling.
	tbb::empty_task* parent_task;
	//tbb::concurrent_bounded_queue<FCGX_Request*> requests;
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
	ServerTask(ServerHandler* handler)
		: _handler(handler)
	{};
	tbb::task* execute();
private:
	ServerHandler* _handler;
};

#endif