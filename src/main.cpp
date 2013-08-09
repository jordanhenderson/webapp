#include "Logging.h"
#include "Gallery.h"
#include "Server.h"
using namespace std;
#ifdef WIN32
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
	setlocale(LC_ALL, "");
	Parameters params;
	//TODO convert params to file
	File conf;
	if(FileSystem::Open("gallery.conf", "rb", &conf)) {
		FileData data;
		FileSystem::Process(&conf, &params, (void*)Parameters::parseBuffer, &data);
		FileSystem::Close(&conf);
	} else {
		//DEFAULT PARAMETERS
		params.set("logfile", "gallery.log");
		params.set("basepath", "gallery");
		params.set("dbpath", "gallery.sqlite");

	}
	string bp = params.get("basepath");
	if(bp[bp.length()] != '/') bp.append("/");
	//Create the base file structure
	FileSystem::MakePath(bp);
	
	//Create logging instance
	logger = new Logging(params.get("basepath") + PATHSEP + params.get("logfile"));
	Gallery* gallery = new Gallery(&params);

	//Create a fastcgi server.
	Server* server = new Server(gallery);
	server->setHandler(gallery);
	server->join();

	delete server;
	delete gallery;
	delete logger;
	return 0;
}

