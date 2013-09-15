#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 
#ifdef _WIN32
#define NOMINMAX
#endif
#include "fcgiapp.h"
#include "fcgios.h"
#include <tbb/task.h>

class ServerHandler {
protected:
	int shutdown_handler;
public:
	virtual void process(FCGX_Request* request) = 0;
	friend class Server;
};

class ServerTask : public tbb::task {
public: 
	ServerTask(FCGX_Request* request, ServerHandler* handler)
		: _request(request), _handler(handler)
	{};
	tbb::task* execute() {
		_handler->process(_request);
		FCGX_Finish_r(_request);
		delete _request;
		return NULL;
	}
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