#include "Logging.h"
#include "Gallery.h"
#include "Server.h"
#include <Windows.h>


int _tmain(int argc, _TCHAR* argv[]) {
	//Create the gallery instance. Handles all logic
	Parameters* params = new Parameters();
	//TODO convert params to file
	if(FileSystem::Exists("gallery.conf")) {
		File* conf = FileSystem::Open("gallery.conf", "rb");
		FileSystem::Process(conf, params, (void*)Parameters::parseBuffer);
	} else {
		//DEFAULT PARAMETERS
		params->set(_T("logfile"), _T("gallery2.log"));
		params->set(_T("basepath"), _T("gallery"));
	}
	
	//Create logging instance
	Logging* logger = new Logging(params->get(_T("logfile")));

	Gallery* gallery = new Gallery(params, logger);
	//Create a HTTPServer on port 8080
	Server* server = new Server(logger, gallery);
	server->setHandler(gallery);
	server->join();
	//Finish any remaining log messages
	logger->finish();
	return 0;
}

