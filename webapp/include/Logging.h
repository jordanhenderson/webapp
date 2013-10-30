//This class simply dumps log information to a file.
#ifndef LOGGING_H
#define LOGGING_H
#include "Platform.h"
#include "CPlatform.h"
#include "tbb/concurrent_queue.h"
#include "FileSystem.h"

#define LOGGER_STATUS_PROCESS 0
#define LOGGER_STATUS_FINISHED 1
class Logging : public Internal {
	std::thread* logger;
	tbb::concurrent_bounded_queue<std::string*> queue;
	static void process(Logging*);
	File logFile;
	tbb::atomic<int> abort;

public:
	Logging(const std::string& logPath);
	Logging();
	~Logging();
	void printf(std::string format, ...);
	void log(const std::string& msg);
	inline void setFile(std::string file);
	void finish();
	

};

extern Logging* logger;

#endif