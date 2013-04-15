#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include "ServerSocket.h"

class Gallery; 
class ServerHandler {
public:
	virtual void process() = 0;
};

class HttpServer : Internal {
	ServerSocket* network;
	std::thread server;
	ServerHandler* handler;
	Logging* logger;
public:
	HttpServer(Logging*);
	~HttpServer();
	void join();
	void setHandler(ServerHandler*);
	void run();
	
};

#endif