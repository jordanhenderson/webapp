#ifndef DATABASE_H
#define DATABASE_H
#include "tbb/concurrent_queue.h"
#include "sqlite3.h"
#include "Platform.h"
#define DATABASE_STATUS_PROCESS 0
#define DATABASE_STATUS_FINISHED 1
#define DATABASE_QUERY_FINISHED 1

typedef std::vector<std::vector<std::string>> QueryResponse;
typedef std::vector<std::string> QueryRow;
class Query {
public:
	std::string dbq;
	QueryRow* params; 
	QueryResponse* response;
	QueryRow* description;
	int status;
	Query(std::string dbq);
	~Query();
};
class Database : Internal {
private:
	sqlite3* db;
	std::thread dbthread;
	tbb::concurrent_queue<Query*> queue;
	void process();
	int status;
public:
	Database(const char* filename);
	~Database();
	Query* select(Query* query, int desc = 0);
	Query* select(const char* query, int desc = 0);
};
#endif