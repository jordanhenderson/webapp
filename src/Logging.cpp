#include "Logging.h"
using namespace std;
Logging::Logging(tstring logPath) {
	logFile = NULL;
	//Open a file for writing.
	setFile(logPath);
	
	//Create a logging thread that will continuously queue.pop().
	logger = thread(&Logging::process, this);

}

Logging::Logging() {
	//Empty logging instance. No logFile.
	logFile = NULL;
	logger = thread(&Logging::process, this);
}

void Logging::setFile(tstring logPath) {
	if(logFile) 
		FileSystem::Close(logFile);
	logFile = FileSystem::Open(logPath, _T("w"));

}

void Logging::process() {
	while(true) {
		tstring msg = _T("");
		queue.try_pop(msg);
		if(!msg.empty()) {
			//Print the message to our file.
			FileSystem::Write(logFile, msg);
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void Logging::log(tstring msg) {
	//Insert msg into our message queue for the Logging thread to handle.
	if(logFile != NULL)
		queue.push(msg);
}

void Logging::_tprintf(tstring format, ...) {
	//Format the string, then insert it into the log.
	if(logFile == NULL)
		return;
	TCHAR* buffer;
	size_t sz;
	va_list args;
	va_start(args, format);
	sz = _sntprintf(NULL, 0, format.c_str(), args);
	buffer = new TCHAR[sz + 1];
	_sntprintf(buffer, sz + 1, format.c_str(), args);
	va_end(args);
	this->log(buffer);
}