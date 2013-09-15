#include "Logging.h"
#include "Gallery.h"
#include "Server.h"

using namespace std;

void Server::listener(ServerHandler* handler, int sock) {
	if(handler == NULL)
		return;
	while(!handler->shutdown_handler) {
		FCGX_Request* request = new FCGX_Request();
		FCGX_InitRequest(request, sock, FCGI_FAIL_ACCEPT_ON_INTR);
		const struct timeval timeout = {1, 0};
		fd_set readfds;
		FD_ZERO(&readfds);
#pragma warning( disable : 4127 ) 
        FD_SET((unsigned int) sock, &readfds);
#pragma warning( default : 4127 ) 
		for(;;) {
			if(select(0, &readfds, NULL, NULL, &timeout) == 0) {
				//Check serverhandler (connections accepted?)
			} else {
				//Check serverhandler (connections accepted?)
				break;
			}
		}

		int rc = FCGX_Accept_r(request);
		
		if(rc < 0) break;
		ServerTask* task = new (tbb::task::allocate_root()) ServerTask(request, handler);
		tbb::task::enqueue(*task);


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
	listener_thread = new thread(&Server::listener, handler, sock);
	
}

Server::~Server() {

}

void Server::setHandler(ServerHandler* handler) {
	this->handler = handler;
}

void Server::join() {
	listener_thread->join();
	delete handler;
	return;
}