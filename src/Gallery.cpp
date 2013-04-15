// gallery.cpp : Defines the entry point for the console application.
//

#include "Logging.h"
#include "Gallery.h"
#include "HttpServer.h"

Gallery::Gallery(Parameters* params) {
	this->port = params->getDigit(_T("port"));
	this->host = params->get(_T("host"));
	logger = new Logging(params->get(_T("logfile")));
}


Logging* Gallery::getLogger() {
	return logger;
}

void Gallery::process() {

}

void Gallery::setLog(tstring file) {
	this->logger->setFile(file);
}

int _tmain(int argc, _TCHAR* argv[]) {
	//Create the gallery instance. Handles all logic
	Parameters* params = new Parameters();
	params->set(_T("port"), _T("8080"));
	params->set(_T("host"), _T("127.0.0.1"));
	params->set(_T("logfile"), _T("gallery.log"));
	Gallery* gallery = new Gallery(params);
	//Create a HTTPServer on port 8080
	HttpServer* server = new HttpServer(gallery->getLogger());
	server->setHandler(gallery);
	server->join();
	return 0;
}

