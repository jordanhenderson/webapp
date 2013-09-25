#include "Logging.h"
#include "Gallery.h"
#include "Server.h"
#include <tbb/task_scheduler_init.h>

using namespace std;
using namespace tbb;
task* ServerTask::execute() {
	_handler->createWorker();
	_handler->numInstances--;
	return NULL;
}

void Server::listener(ServerHandler* handler, int sock) {
	if(handler == NULL)
		return;

	FCGX_Request* request = NULL;
	int count = 0;
	while(!handler->shutdown_handler) {
		
		request = (FCGX_Request*)malloc(sizeof(FCGX_Request));
		FCGX_InitRequest(request, sock, 0);

		int rc = FCGX_Accept_r(request);
		++count;
		printf("Request number: %d\n", count);
		handler->handlerLock.lock();

		if(rc < 0) break;
		if(handler->shutdown_handler) goto finish;
		
		if(handler->numInstances < tbb::task_scheduler_init::default_num_threads()) {	
			handler->numInstances++;
			ServerTask* task = new (task::allocate_additional_child_of(*handler->parent_task)) 
				ServerTask(handler);
			handler->parent_task->enqueue(*task);
		} 

		//handler->requests.push(request);
		
		handler->handlerLock.unlock();
	}
finish:
	FCGX_Finish_r(request);
	free(request);

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