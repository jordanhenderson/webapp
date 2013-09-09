#ifndef DATABASE_H
#define DATABASE_H

#include "Platform.h"
#include "sqlite3.h"
#include <thread>

#define DATABASE_STATUS_PROCESS 0
#define DATABASE_STATUS_FINISHED 1
#define DATABASE_QUERY_STARTED 0
#define DATABASE_QUERY_FINISHED 1

#define DATABASE_TYPE_SQLITE 0
#define DATABASE_TYPE_MYSQL 1

struct st_mysql;
typedef struct st_mysql MYSQL;

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
	Query(const std::string& dbq, QueryRow* params=NULL);
	~Query();

};

class Database : public Internal {
private:
	sqlite3* sqlite_db;
	MYSQL* mysql_db;
	std::thread* dbthread;

	void process(Query* q);
	int shutdown_database;
	int db_type;

public:
	Database(int database_type, const std::string& host, const std::string& username="", const std::string& password="", const std::string& database="");
	~Database();
	int exec(Query* query);
	//Returns last row ID inserted
	int exec(const std::string& query, QueryRow* params=NULL);
	Query* select(Query* query, QueryRow* params=NULL, int desc = 0);
	//Return the first cell in the first row of the response as a string.
	std::string select(const std::string& query, QueryRow* params = NULL, int desc = 0);
};
#endif