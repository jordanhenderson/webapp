#include "Logging.h"
#include "Gallery.h"
#include "Server.h"
#include <Windows.h>
int _tmain(int argc, _TCHAR* argv[]) {
	HANDLE hMutex = ::OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "Global\\TestServer");
	::WaitForSingleObject( hMutex, INFINITE );
	::CloseHandle(hMutex);
	//Create the gallery instance. Handles all logic
	Parameters* params = new Parameters();
	//TODO convert params to file
	params->set(_T("logfile"), _T("gallery.log"));
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

