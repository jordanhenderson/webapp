#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 
#include "fcgiapp.h"
#define SERVER_THREADS 20
class ServerHandler {
public:
	virtual void process(FCGX_Request* request) = 0;
};

class Server : Internal {

	std::thread serverpool[SERVER_THREADS];
	std::shared_ptr<ServerHandler> handler;
	std::shared_ptr<Logging> logger;
public:
	Server(std::shared_ptr<Logging>&, std::shared_ptr<ServerHandler>&);
	~Server();
	void join();
	void setHandler(std::shared_ptr<ServerHandler>&);
	void run(int nThread, int sock);
	
};

#endif