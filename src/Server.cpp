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
		printf("Thread ID: %i, URL: %s\n", nThread, uri);
		if(handler != NULL)
			handler->process(&request); 
		

		//std::this_thread::sleep_for(std::chrono::seconds(2));
		FCGX_Finish_r(&request);
	}
	
	
}

Server::Server( Logging* logging, ServerHandler* handler) {
	this->logger = logging;
	if(FCGX_IsCGI()) {
		logger->log(_T("Running as CGI Server."));
	}
	this->handler = handler;
	logger->log(_T("Server Initialised. Creating FastCGI sockets..."));
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