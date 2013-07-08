#ifndef DATABASE_H
#define DATABASE_H

#include "tbb/concurrent_queue.h"
#include "sqlite3.h"
#include "Platform.h"
#include <thread>

#define DATABASE_STATUS_PROCESS 0
#define DATABASE_STATUS_FINISHED 1
#define DATABASE_QUERY_STARTED 0
#define DATABASE_QUERY_FINISHED 1

typedef std::vector<std::vector<std::string>> QueryResponse;
typedef std::vector<std::string> QueryRow;
class Query {
public:
	std::string* dbq;
	QueryRow* params; 
	QueryResponse* response;
	QueryRow* description;
	int status;
	int lastrowid;
	int params_copy;
	Query(const std::string& dbq, QueryRow* params=NULL, int copy = 0);
	~Query();
};

class Database : public Internal {
private:
	sqlite3* db;
	std::thread dbthread;
	tbb::concurrent_queue<Query*> queue;
	void process();
	int status;
	

public:
	Database(const char* filename);
	~Database();
	void select(Query* query);
	int exec(Query* query);
	int exec(const std::string& query, QueryRow* params);
	Query select(const std::string& query, int desc = 0);
	Query select(const std::string& query, QueryRow* params, int desc = 0);
};
#endif