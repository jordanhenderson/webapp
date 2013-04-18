#ifndef HTTPSERVER_H
#define HTTPSERVER_H

class Gallery; 

#define SERVER_THREADS 1
class ServerHandler {
public:
	virtual tstring process(char** request) = 0;
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