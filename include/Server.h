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
	ServerHandler* handler;
	Logging* logger;
public:
	Server(Logging*, ServerHandler*);
	~Server();
	void join();
	void setHandler(ServerHandler*);
	void run(int nThread, int sock);
	
};

#endif