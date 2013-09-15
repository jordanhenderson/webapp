#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 
#include "fcgiapp.h"
#define SERVER_THREADS 20

class ServerHandler {
protected:
	int shutdown_handler;
public:
	virtual void process(FCGX_Request* request) = 0;
	friend class Server;
	std::mutex lockHandler;
};

class Server : public Internal {
	std::thread* serverpool[SERVER_THREADS];
	ServerHandler* handler;
public:
	Server(ServerHandler*);
	~Server();
	void join();
	void setHandler(ServerHandler*);
	static void run(ServerHandler*, int nThread, int sock);
	
};

#endif