#include "Logging.h"
#include "Gallery.h"
#include "Server.h"

using namespace std;
void Server::run(int nThread, int sock) {
	FCGX_Request request;
	int rc = 0;



	FCGX_InitRequest(&request, sock, 0);
	for(;;) {
		
		rc = FCGX_Accept_r(&request);
		
		if(rc < 0)
			break;

		char* uri = FCGX_GetParam("REQUEST_URI", request.envp);
		logger->printf("Thread ID: %i, URL: %s", nThread, uri);
		if(handler != NULL)
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

		serverpool[n] = thread(&Server::run, this, n, sock);
	}
	
}

Server::~Server() {

}

void Server::setHandler(ServerHandler* handler) {
	this->handler = handler;
}

void Server::join() {
	for(int n = 0; n < SERVER_THREADS; n++) {
		if(serverpool[n].joinable())
			serverpool[n].join();
	}
	return;
}