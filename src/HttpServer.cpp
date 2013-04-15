#include "Logging.h"
#include "Gallery.h"
#include "HttpServer.h"

using namespace std;
void HttpServer::run() {
	while(true) {
		
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

HttpServer::HttpServer( Logging* logging ) {
	this->logger = logging;
	logger->log(_T("HTTP Server Initialised. Creating server sockets..."));
	network = new ServerSocket(logging);
	server = thread(&HttpServer::run, this);
}

HttpServer::~HttpServer() {
	if(network != NULL)
		delete network;
}

void HttpServer::setHandler(ServerHandler* handler) {
	this->handler = handler;
}

void HttpServer::join() {
	if(server.joinable())
		server.join();
	return;
}