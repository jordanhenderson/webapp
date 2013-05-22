#include "Logging.h"
#include "Gallery.h"
#include "Server.h"


using namespace std;
#ifdef WIN32
#include <Windows.h>
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
	setlocale(LC_CTYPE, "");
	//Create the gallery instance. Handles all logic
	shared_ptr<Parameters> params = unique_ptr<Parameters>(new Parameters());
	//TODO convert params to file
	if(FileSystem::Exists("gallery.conf")) {
		std::unique_ptr<File> conf = FileSystem::Open("gallery.conf", "rb");
		FileSystem::Process(conf, params.get(), (void*)Parameters::parseBuffer);
		FileSystem::Close(conf);
	} else {
		//DEFAULT PARAMETERS
		params->set("logfile", "gallery.log");
		params->set("basepath", "gallery");
		params->set("dbpath", "gallery.sqlite");
		params->set("storepath", "store");
		params->set("thumbspath", "thumbs");
	}
	
	//Create logging instance
	shared_ptr<Logging> logger = unique_ptr<Logging>(new Logging(params->get("basepath") + PATHSEP + params->get("logfile")));

	shared_ptr<ServerHandler> gallery = shared_ptr<ServerHandler>(new Gallery(params, logger));
	//Create a fastcgi server.
	Server* server = new Server(logger, gallery);
	server->setHandler(gallery);
	server->join();
	//Finish any remaining log messages
	logger->finish();
	delete server;
	return 0;
}

