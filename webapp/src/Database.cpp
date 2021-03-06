/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"
#include "Database.h"
#include "sqlite3.h"
#include "my_global.h"
#include "mysql.h"

using namespace std;

/**
 * Create a new Query instance.
 * @param db The associated database connection
*/
Query::Query(Database* db) : _db(db)
{
}

/**
 * Create a new Query instance, providing an initial query string.
 * @see Query
 * @param dbq The initial query string
*/
Query::Query(Database* db, const webapp_str_t& _dbq)
	: dbq(_dbq), _db(db)
{
}

/**
 * Destroy a Query instance.
*/
Query::~Query()
{
	//Clean up description fields.
	if (description != NULL) {
		delete[] description;
	}

	//Clean up any current row fields.
	if (row != NULL) {
		delete[] row;
	}

	unsigned int db_type = _db->db_type;

	//Clean up mysql.
	if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if (size_arr != NULL) delete[] size_arr;
		if (bind_params != NULL) delete[] bind_params;
		if (out_size_arr != NULL) delete[] out_size_arr;
		prepare_meta_result = NULL;
		size_arr = NULL;
		bind_params = NULL;
		out_size_arr = NULL;
	}

	//Clean up stmt objects.
	if (mysql_stmt != NULL) mysql_stmt_close(mysql_stmt);
	if (sqlite_stmt != NULL) sqlite3_finalize(sqlite_stmt);
	mysql_stmt = NULL;
	sqlite_stmt = NULL;
}

/**
 * Process a query, possibly retrieving the next row of results.
*/
void Query::process()
{
	unsigned int lasterror = 0;
	unsigned int db_type = _db->db_type;
	sqlite3* sqlite_db = _db->sqlite_db;
	MYSQL* mysql_db = _db->mysql_db;
	
	//Abort if query set to finished.
	if (status == DATABASE_QUERY_FINISHED) return;
	
	//Abort if db not initialised.
	if ((db_type == DATABASE_TYPE_SQLITE && sqlite_db == NULL) ||
		(db_type == DATABASE_TYPE_MYSQL && mysql_db == NULL))
	{
		status = DATABASE_QUERY_FINISHED;
		goto cleanup;
	}

	//Initialize database depending on type.
	if (db_type == DATABASE_TYPE_SQLITE) {
		if(sqlite_stmt == NULL) {
			if (sqlite3_prepare_v2(sqlite_db, dbq.data,
								   dbq.len, &sqlite_stmt, 0))
				goto cleanup;
		}
	} else if (db_type == DATABASE_TYPE_MYSQL) {
		if(mysql_stmt == NULL) {
			mysql_stmt = mysql_stmt_init(mysql_db);
			if (mysql_stmt == NULL)
				goto cleanup;
			lasterror = mysql_stmt_prepare(mysql_stmt, dbq.data, dbq.len);
		}
	}

	//Check for (and apply) parameters for parameterized queries.
	if(size_arr == NULL || bind_params == NULL) {
		int m = params.size();
		//Set up MYSQL_BIND and size array for MYSQL parameters.
		if(db_type == DATABASE_TYPE_MYSQL && m > 0) {
			size_arr = new unsigned long[m];
			bind_params = new MYSQL_BIND[m];
			memset(bind_params,0, sizeof(MYSQL_BIND)*m);
		}

		//Iterate over each parameter, binding as appropriate.
		for(int i = 0; i < m; i++) {
			webapp_str_t& p_str = params[i];
			if(db_type == DATABASE_TYPE_SQLITE)
				sqlite3_bind_text(sqlite_stmt, i+1, p_str.data,
								  p_str.len, SQLITE_STATIC);
			else if(db_type == DATABASE_TYPE_MYSQL) {
				bind_params[i].buffer_type = MYSQL_TYPE_STRING;
				bind_params[i].buffer = (char*) p_str.data;
				bind_params[i].is_null = 0;
				bind_params[i].length = &size_arr[i];
				size_arr[i] = p_str.len;
			}
		}
		
		if(db_type == DATABASE_TYPE_MYSQL && m > 0) {
			if((lasterror = mysql_stmt_bind_param(mysql_stmt, bind_params))) {
				goto cleanup;
			}
		}
	}

	//Query is new, populate query statistics, allocate row/description memory
	if (status == DATABASE_QUERY_INIT) {
		if (db_type == DATABASE_TYPE_SQLITE) {
			//SQLite needs to exec in order to get column info.
			lasterror = sqlite3_step(sqlite_stmt);
			const char* err_str = sqlite3_errmsg(sqlite_db);
			if(err_str != NULL) err = err_str;
			//Populate statistics
			column_count = sqlite3_column_count(sqlite_stmt);
			lastrowid = sqlite3_last_insert_rowid(sqlite_db);
			rows_affected = sqlite3_changes(sqlite_db);
			
		} else if (db_type == DATABASE_TYPE_MYSQL) {
			//Populate statistics
			if (prepare_meta_result == NULL) {
				prepare_meta_result = mysql_stmt_result_metadata(mysql_stmt);
			}
			
			if(prepare_meta_result != NULL) {
				column_count = mysql_num_fields(prepare_meta_result);
			}
			if (mysql_stmt_execute(mysql_stmt)) goto cleanup;
			//Populate error message
			const char* err_str = mysql_stmt_error(mysql_stmt);
			if(err_str != NULL) err = err_str;
			
			lastrowid = mysql_stmt_insert_id(mysql_stmt);
			rows_affected = mysql_stmt_affected_rows(mysql_stmt);
			
		}

		if (column_count == 0) goto cleanup;
		
	} else if(status == DATABASE_QUERY_STARTED) {
		//Populate next row.
		if (db_type == DATABASE_TYPE_SQLITE) {
			lasterror = sqlite3_step(sqlite_stmt);
			//Populate error message
			const char* err_str = sqlite3_errmsg(sqlite_db);
			if(err_str != NULL) err = err_str;
		} else if (db_type == DATABASE_TYPE_MYSQL) {
			if (mysql_stmt_execute(mysql_stmt)) goto cleanup;
			//Populate error message
			const char* err_str = mysql_stmt_error(mysql_stmt);
			if(err_str != NULL) err = err_str;
		}
	}
	
	//Initialize description memory.
	if (desc && description == NULL) {
		description = new webapp_str_t[column_count]();
	}
	
	//Initialize row memory.
	if (row == NULL) row = new webapp_str_t[column_count]();

	//Populate row data and column description
	if (db_type == DATABASE_TYPE_SQLITE) {
		if (lasterror == SQLITE_ROW) {
			for (int col = 0; col < column_count; col++) {
				//Push back the column name
				if (description != NULL && !havedesc) {
					description[col].data = (char*)sqlite3_column_name(sqlite_stmt, col);
					description[col].len = strlen(description[col].data);
				}
				//Push back the column text
				char* text = (char*)sqlite3_column_text(sqlite_stmt, col);
				int size = sqlite3_column_bytes(sqlite_stmt, col);
				row[col].data = text;
				row[col].len = size;
			}
			//We have the column description after the first pass.
			if (description != NULL) havedesc = 1;
		} else
			goto cleanup;
	} else if (db_type == DATABASE_TYPE_MYSQL) {
		//If bind_output is NULL, populate description field.
		if (bind_output == NULL) {
			bind_output = new MYSQL_BIND[column_count];
			memset(bind_output, 0, sizeof(MYSQL_BIND)*column_count);
			out_size_arr = new unsigned long[column_count];
			//Collect sizes required for each column.
			for (int i = 0; i < column_count; i++) {
				bind_output[i].buffer_type = MYSQL_TYPE_STRING;
				bind_output[i].length = &out_size_arr[i];
			}
			mysql_stmt_bind_result(mysql_stmt, bind_output);

			if (description != NULL && !havedesc) {
				MYSQL_FIELD *field;
				for (unsigned int i = 0;
						(field = mysql_fetch_field(prepare_meta_result)); i++) {
					description[i].data = field->name;
					description[i].len = field->name_length;
				}
				havedesc = 1;
			}
		}

		//Get the next row.
		if (mysql_stmt_fetch(mysql_stmt)) {
			for (int col = 0; col < column_count; col++) {
				bind_output[col].buffer = (void*)&row[col].data;
				bind_output[col].buffer_length = out_size_arr[col] + 1;
				bind_output[col].length = (unsigned long*)&row[col].len;
				mysql_stmt_fetch_column(mysql_stmt, &bind_output[col], col, 0);
			}
		} else goto cleanup;
	}
	
	//Set the Query to started, to ensure future calls don't reinit.
	status = DATABASE_QUERY_STARTED;
	return;

	//Database query is finished.
cleanup:
	status = DATABASE_QUERY_FINISHED;

	//Clean up mysql.
	if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if (size_arr != NULL) delete[] size_arr;
		if (bind_params != NULL) delete[] bind_params;
		if (out_size_arr != NULL) delete[] out_size_arr;
		prepare_meta_result = NULL;
		size_arr = NULL;
		bind_params = NULL;
		out_size_arr = NULL;
	}

	//Clean up stmt objects.
	if (mysql_stmt != NULL) mysql_stmt_close(mysql_stmt);
	if (sqlite_stmt != NULL) sqlite3_finalize(sqlite_stmt);
	mysql_stmt = NULL;
	sqlite_stmt = NULL;
}

/**
 * Connect a Database object.
 * @param database_type the database type to connect to.
 * @param host the hostname (or filename, for sqlite) provided to the DB
 * @param username the username provided to the db api
 * @param password the password provided to the db api
 * @param database the database to use (for multi-database servers)
 * @return connection fail or success.
*/
int Database::connect(int database_type, const char* host, const char* username,
					  const char* password, const char* database)
{
	db_type = database_type;
	if(database_type == DATABASE_TYPE_SQLITE) {
		int ret = sqlite3_open(host, &sqlite_db);
		if(sqlite_db == NULL || ret != SQLITE_OK) {
			return DATABASE_FAILED;
		}
	} else if(database_type == DATABASE_TYPE_MYSQL) {
		mysql_library_init(0, NULL, NULL);
		mysql_db = mysql_init(NULL);
		mysql_db = mysql_real_connect(mysql_db, host, username, password, database, 0, NULL, 0);
		if(mysql_db == NULL) {
			return DATABASE_FAILED;
		} else {
			//Disable mysql trunctation reporting.
			bool f = false;
			mysql_options(mysql_db, MYSQL_REPORT_DATA_TRUNCATION, &f);
		}
	}
	return DATABASE_SUCCESS;
}

/**
 * Destroy a Database object.
*/
Database::~Database()
{
	if(db_type == DATABASE_TYPE_SQLITE && sqlite_db != NULL)
		sqlite3_close(sqlite_db);
	else if(db_type == DATABASE_TYPE_MYSQL && mysql_db != NULL) {
		mysql_close(mysql_db);
		mysql_library_end();
	}
}

/**
 * Execute a string, creating a temporary query object.
 * @param query the string to execute
 * @return the last inserted row ID
*/
int64_t Database::exec(const webapp_str_t& query)
{
	Query q(this, query);
	q.process();
	return q.lastrowid;
}
