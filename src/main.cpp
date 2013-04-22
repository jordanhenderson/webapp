#include "Logging.h"
#include "Gallery.h"
#include "Server.h"
#include <Windows.h>

#ifdef WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
	setlocale(LC_CTYPE, "");
	//Create the gallery instance. Handles all logic
	Parameters* params = new Parameters();
	//TODO convert params to file
	if(FileSystem::Exists("gallery.conf")) {
		File* conf = FileSystem::Open("gallery.conf", "rb");
		FileSystem::Process(conf, params, (void*)Parameters::parseBuffer);
	} else {
		//DEFAULT PARAMETERS
		params->set("logfile", "gallery2.log");
		params->set("basepath", "gallery");
	}
	
	//Create logging instance
	Logging* logger = new Logging(params->get("logfile"));

	Gallery* gallery = new Gallery(params, logger);
	//Create a HTTPServer on port 8080
	Server* server = new Server(logger, gallery);
	server->setHandler(gallery);
	server->join();
	//Finish any remaining log messages
	logger->finish();
	return 0;
}

