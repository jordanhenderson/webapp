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
		params.set("basepath", ".");
		params.set("dbpath", "gallery.sqlite");

	}
	string bp = params.get("basepath");
	if(bp[bp.length()] != '/') bp.append("/");
	//Create the base file structure
	if(!FileSystem::MakePath(bp)) return 1;
	string logPath = params.get("basepath") + '/' + params.get("logfile");
	//Create logging instance
	logger = new Logging(logPath);
	Gallery* gallery = new Gallery(&params);



	delete gallery;
	delete logger;
	return 0;
}

