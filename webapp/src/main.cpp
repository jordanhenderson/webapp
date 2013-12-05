
#include "Webapp.h"


using namespace std;

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	#ifdef __GNUC__
	setvbuf(stdout, NULL, _IONBF, 0);
	#endif
	Parameters params;
	//TODO convert params to file
	File conf;
	if(FileSystem::Open("webapp.conf", "rb", &conf)) {
		FileData data;
		FileSystem::Process(&conf, &params, (void*)Parameters::parseBuffer, &data);
		FileSystem::Close(&conf);
	} else {
		//DEFAULT PARAMETERS
		params.set("basepath", "./");
		params.set("dbpath", "webapp.sqlite");

	}
	string bp = params.get("basepath");
	if(bp[bp.length()] != '/') bp.append("/");
	//Create the base file structure
	if(!FileSystem::MakePath(bp)) return 1;
	string logPath = params.get("basepath") + '/' + params.get("logfile");
	//Create logging instance
	asio::io_service svc;
	Webapp* gallery = new Webapp(&params, svc);
		
	delete gallery;
	return 0;
}

