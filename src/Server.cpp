#include "Logging.h"
#include "Gallery.h"
#include "Server.h"

using namespace std;
void Server::run(int nThread, int sock) {
	FCGX_Request request;
	if(handler == NULL)
		return;

	FCGX_InitRequest(&request, sock, 0);
	
	while(!handler->abort) {
		
		int rc = FCGX_Accept_r(&request);
		
		if(rc < 0)
			break;
		
		handler->process(&request); 
		FCGX_Finish_r(&request);
	}
	
}

Server::Server(ServerHandler* handler) {
	if(FCGX_IsCGI()) {
		logger->log("Running as CGI Server.");
	}
	this->handler = handler;
	logger->log("Server Initialised. Creating FastCGI sockets...");
	FCGX_Init();
	int sock = FCGX_OpenSocket(":5000", 0);
	
	for(int n = 0; n < SERVER_THREADS; n++) {

		serverpool[n] = new thread(&Server::run, this, n, sock);
	}
	
}

Server::~Server() {

}

void Server::setHandler(ServerHandler* handler) {
	this->handler = handler;
}

void Server::join() {
	for(int n = 0; n < SERVER_THREADS; n++) {
		if(serverpool[n]->joinable())
			serverpool[n]->join();
		delete serverpool[n];
	}
	return;
}