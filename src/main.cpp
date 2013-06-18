#include "Logging.h"
#include "Gallery.h"
#include "Server.h"

Logging* logger;
using namespace std;
#ifdef WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
	
	setlocale(LC_CTYPE, "");
	Parameters params;
	//TODO convert params to file
	if(FileSystem::Exists("gallery.conf")) {
		File conf;
		FileSystem::Open("gallery.conf", "rb", &conf);
		FileData data;
		FileSystem::Process(&conf, &params, (void*)Parameters::parseBuffer, &data);
		FileSystem::Close(&conf);
	} else {
		//DEFAULT PARAMETERS
		params.set("logfile", "gallery.log");
		params.set("basepath", "gallery");
		params.set("dbpath", "gallery.sqlite");
		params.set("storepath", "store");
		params.set("thumbspath", "thumbs");
	}
	
	//Create logging instance
	logger = new Logging(params.get("basepath") + PATHSEP + params.get("logfile"));
	auto gallery = new Gallery(&params);

	//Create a fastcgi server.
	Server* server = new Server(gallery);
	server->setHandler(gallery);
	server->join();
	//Finish any remaining log messages
	logger->finish();
	delete server;
	
	return 0;
}

