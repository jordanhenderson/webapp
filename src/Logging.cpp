#include "Logging.h"
using namespace std;
Logging::Logging(string logPath) {
	//Open a file for writing.
	setFile(logPath);
	status = LOGGER_STATUS_PROCESS;
	//Create a logging thread that will continuously queue.pop().
	logger = new thread(&Logging::process, this);
	logger->detach();

}

Logging::~Logging() {
	//Abort the logger thread.
	status = LOGGER_STATUS_FINISHED;
	//Join the logging queue thread
	if(logger->joinable())
		logger->join();

	//Delete the thread object.
	delete logger;

	//Process any messages still on the queue
	string* msg;
	while(queue.try_pop(msg)) {
		FileSystem::WriteLine(&logFile, *msg);
		delete msg;
	}
	//Finally, close the file.
	if(logFile.pszFile != NULL)
		FileSystem::Close(&logFile);
}

Logging::Logging() {
	//Empty logging instance. No logFile.
	status = LOGGER_STATUS_PROCESS;
	logger = new thread(&Logging::process, this);
}

void Logging::setFile(string logPath) {
	if(logFile.pszFile != NULL) 
		FileSystem::Close(&logFile);
	FileSystem::Open(logPath, "w", &logFile);

}

void Logging::process() {
	while(status == LOGGER_STATUS_PROCESS) {
		string* msg;
		if(queue.try_pop(msg))  {
			FileSystem::WriteLine(&logFile, *msg);
			delete msg;
		}
		this_thread::sleep_for(chrono::microseconds(100));
	}
}

void Logging::log(const string& msg) {
	//Insert msg into our message queue for the Logging thread to handle.
	if(logFile.pszFile != NULL)
		queue.push(new string(msg));
}

void Logging::printf(string format, ...) {
	//Format the string, then insert it into the log.
	if(logFile.pszFile == NULL)
		return;
	char* buffer;
	size_t sz;
	va_list args;
	va_start(args, format);
	sz = vsnprintf(NULL, 0, format.c_str(), args);
	buffer = new char[sz + 1];
	vsnprintf(buffer, sz + 1, format.c_str(), args);
	this->log(string(buffer, sz));
	delete[] buffer;
	va_end(args);
	
	
}

//Define a global pointer for global logger access.
Logging* logger;