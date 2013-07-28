//This class simply dumps log information to a file.
#ifndef LOGGING_H
#define LOGGING_H
#include "Platform.h"
#include "tbb/concurrent_queue.h"
#include "FileSystem.h"

#define LOGGER_STATUS_PROCESS 0
#define LOGGER_STATUS_FINISHED 1
class Logging : public Internal {
	std::thread* logger;
	tbb::concurrent_queue<std::string*> queue;
	void process();
	File logFile;
	std::condition_variable cv;
	std::mutex m;
	int abort;

public:
	Logging(std::string logPath);
	Logging();
	~Logging();
	void printf(std::string format, ...);
	void log(const std::string& msg);
	inline void setFile(std::string file);
	void finish();
	

};

extern Logging* logger;

#endif