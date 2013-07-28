#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 
#include "fcgiapp.h"
#define SERVER_THREADS 20

class ServerHandler {
protected:
	int abort;
public:
	virtual void process(FCGX_Request* request) = 0;
	friend class Server;
};

class Server : public Internal {
	std::thread* serverpool[SERVER_THREADS];
	ServerHandler* handler;
public:
	Server(ServerHandler*);
	~Server();
	void join();
	void setHandler(ServerHandler*);
	void run(int nThread, int sock);
	
};

#endif