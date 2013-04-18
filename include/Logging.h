//This class simply dumps log information to a file.
#ifndef LOGGING_H
#define LOGGING_H
#include "Platform.h"
#include "tbb/concurrent_queue.h"
#include "FileSystem.h"
#include <thread>

#define LOGGER_STATUS_PROCESS 0
#define LOGGER_STATUS_FINISHED 1
class Logging : Internal {
	std::thread logger;
	tbb::concurrent_queue<tstring> queue;
	void process();
	File* logFile;
	int status;
public:
	Logging(tstring logPath);
	Logging();
	~Logging(){};
	void _tprintf(tstring format, ...);
	void log(tstring msg);
	inline void setFile(tstring file);
	void finish();
	

};

#endif