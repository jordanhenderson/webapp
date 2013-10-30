#ifndef DATABASE_H
#define DATABASE_H

#include "CPlatform.h"
#include "Platform.h"

#include "sqlite3.h"

#define DATABASE_STATUS_PROCESS 0
#define DATABASE_STATUS_FINISHED 1
#define DATABASE_QUERY_STARTED 0
#define DATABASE_QUERY_FINISHED 1

#define DATABASE_TYPE_SQLITE 0
#define DATABASE_TYPE_MYSQL 1

#define DATABASE_SUCCESS 0
#define DATABASE_FAILED 1

struct st_mysql;
typedef struct st_mysql MYSQL;
typedef struct st_mysql_res MYSQL_RES;
typedef struct st_mysql_bind MYSQL_BIND;

typedef std::vector<std::string> QueryRow;
class Query {
public:
	std::string* dbq;
	QueryRow* params; 
	QueryRow* description;
	int status;
	int db_type;
	long long lastrowid;
	int params_copy;

//Database instance parameters
	void* stmt = NULL;
	unsigned long* size_arr = NULL;
	unsigned long* out_size_arr = NULL;
	MYSQL_RES* prepare_meta_result = NULL;
	MYSQL_BIND* bind_params = NULL;
	MYSQL_BIND* bind_output = NULL;
	int column_count = 0;

	std::vector<std::string> row;
	Query(const std::string& dbq, QueryRow* params=NULL);
	~Query();

};

class Database : public Internal {
private:
	sqlite3* sqlite_db = NULL;
	MYSQL* mysql_db = NULL;
	int shutdown_database = 0;
	int db_type;
	void process(Query* q);


public:
	Database();
	~Database();
	int connect(int database_type, const char* host, const char* username, const char* password, const char* database);
	long long exec(Query* query);
	long long exec(const std::string& query, QueryRow* params=NULL);
	Query* select(Query* query, QueryRow* params=NULL, int desc = 0);
	//Return the first cell in the first row of the response as a string.
	std::string select_single(const std::string& query, QueryRow* params = NULL, const std::string& def = "");
};
#endif