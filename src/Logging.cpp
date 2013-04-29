#include "Logging.h"
using namespace std;
Logging::Logging(string logPath) {
	//Open a file for writing.
	setFile(logPath);
	status = LOGGER_STATUS_PROCESS;
	//Create a logging thread that will continuously queue.pop().
	logger = thread(&Logging::process, this);

}

Logging::Logging() {
	//Empty logging instance. No logFile.
	logFile = NULL;
	status = LOGGER_STATUS_PROCESS;
	logger = thread(&Logging::process, this);

}

void Logging::setFile(string logPath) {
	if(logFile != NULL) 
		FileSystem::Close(logFile);
	logFile = FileSystem::Open(logPath, "w");

}

void Logging::process() {
	while(status == LOGGER_STATUS_PROCESS) {
		string msg = "";
		if(queue.try_pop(msg))  {
			FileSystem::WriteLine(logFile, msg);
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void Logging::log(string msg) {
	//Insert msg into our message queue for the Logging thread to handle.
	if(logFile == NULL || logFile->pszFile != NULL)
		queue.push(msg);
}

void Logging::printf(string format, ...) {
	//Format the string, then insert it into the log.
	if(logFile == NULL || logFile->pszFile == NULL)
		return;
	char* buffer;
	size_t sz;
	va_list args;
	va_start(args, format);
	sz = vsnprintf(NULL, 0, format.c_str(), args);
	buffer = new char[sz + 1];
	vsnprintf(buffer, sz + 1, format.c_str(), args);
	delete[] buffer;
	va_end(args);
	this->log(string(buffer, sz));
}

void Logging::finish() {
	//Abort the logger thread.
	status = LOGGER_STATUS_FINISHED;
	//Join the logging queue thread
	if(logger.joinable())
		logger.join();
	else return;
	//Process any messages still on the queue
	string msg = "";
	while(queue.try_pop(msg)) {
		FileSystem::WriteLine(logFile, msg);
	}
	//Finally, close the file.
	FileSystem::Close(logFile);
}