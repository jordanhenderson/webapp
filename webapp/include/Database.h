/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef DATABASE_H
#define DATABASE_H

#include "Platform.h"

#define DATABASE_STATUS_PROCESS 0
#define DATABASE_STATUS_FINISHED 1
#define DATABASE_QUERY_INIT 0
#define DATABASE_QUERY_STARTED 1
#define DATABASE_QUERY_FINISHED 2
#define DATABASE_TYPE_SQLITE 0
#define DATABASE_TYPE_MYSQL 1
#define DATABASE_SUCCESS 0
#define DATABASE_FAILED 1

struct st_mysql;
typedef struct st_mysql MYSQL;
typedef struct st_mysql_res MYSQL_RES;
typedef struct st_mysql_bind MYSQL_BIND;
typedef struct st_mysql_stmt MYSQL_STMT;
struct sqlite3_stmt;
struct sqlite3;
struct webapp_str_t;

class Database;
struct Query {
	int status = DATABASE_QUERY_INIT;
	int64_t lastrowid = 0;
	int column_count = 0;
	webapp_str_t* row = NULL;
	webapp_str_t* description = NULL;
	int desc = 0;
	int havedesc = 0;
	int rows_affected = 0;
	webapp_str_t dbq;
	std::vector<webapp_str_t> params;
	Query(Database* db, int desc=0);
	Query(Database* db, const webapp_str_t& dbq, int desc=0);
	~Query();
	void process();
private:
//Database instance parameters
	Database* _db;
	MYSQL_STMT* mysql_stmt = NULL;
	sqlite3_stmt* sqlite_stmt = NULL;
	unsigned long* size_arr = NULL;
	unsigned long* out_size_arr = NULL;
	MYSQL_RES* prepare_meta_result = NULL;
	MYSQL_BIND* bind_params = NULL;
	MYSQL_BIND* bind_output = NULL;
};

class Database {
	unsigned int db_type = 0;
	size_t _db_id = 0;
public:
	sqlite3* sqlite_db = NULL;
	MYSQL* mysql_db = NULL;
	inline unsigned int GetDBType()
	{
		return db_type;
	}
	inline size_t GetID()
	{
		return _db_id;
	}
	Database(size_t db_id) : _db_id(db_id) {}
	~Database();

	int connect(int database_type, const char* host, const char* username,
				const char* password, const char* database);
	int64_t exec(const webapp_str_t& query);
};

#endif //DATABASE_H
