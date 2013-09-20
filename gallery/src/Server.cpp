#include "Logging.h"
#include "Gallery.h"
#include "Server.h"

using namespace std;
using namespace tbb;
task* ServerTask::execute() {
	if(_handler->shutdown_handler) return NULL;
	_handler->process(_request);
	FCGX_Finish_r(_request);
	free(_request);
	
	return NULL;
}

void Server::listener(ServerHandler* handler, int sock) {
	if(handler == NULL)
		return;

	FCGX_Request* request = NULL;
	while(!handler->shutdown_handler) {
		
		request = (FCGX_Request*)malloc(sizeof(FCGX_Request));
		FCGX_InitRequest(request, sock, FCGI_FAIL_ACCEPT_ON_INTR);
		
		const struct timeval timeout = {1, 0};
		fd_set readfds;
		FD_ZERO(&readfds);
		#pragma warning( disable : 4127 ) 
		FD_SET((unsigned int) sock, &readfds);
		#pragma warning( default : 4127 ) 
		
		for(;;) {
			
			if(select(0, &readfds, NULL, NULL, &timeout) == 0) {
				if(handler->shutdown_handler) {
					FCGX_Finish_r(request);
					free(request);
					return;
				}
			}
			else {
				if(handler->shutdown_handler) {
					FCGX_Finish_r(request);
					free(request);
					return;
				}
				else break;
			}
		}
		int rc = FCGX_Accept_r(request);
		handler->handlerLock.lock();
		

		if(rc < 0) break;
		
		ServerTask* task = new (task::allocate_additional_child_of(*handler->parent_task)) 
			ServerTask(request, handler);
		handler->parent_task->enqueue(*task);
		handler->handlerLock.unlock();
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
}