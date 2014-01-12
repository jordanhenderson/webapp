/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Database.h"
#include "my_global.h"
#include "mysql.h"

using namespace std;

/**
 * Create a new Query instance.
 * @param db The associated database connection
 * @param desc Whether to populate column description fields
*/
Query::Query(Database* db, int desc) : _db(db) {
	params = new QueryRow();
	this->dbq = new string();
	this->desc = desc;
}

/**
 * Create a new Query instance, providing an initial query string.
 * @see Query
 * @param dbq The initial query string
*/
Query::Query(Database* db, const string& dbq, int desc) : _db(db) {
	params = new QueryRow();
	this->dbq = new string(dbq);
	this->desc = desc;
}

/**
 * Destroy a Query instance.
*/
Query::~Query() {
	//Clean up parameters
	if(params != NULL)
		delete params;

	//Clean up description fields.
	if (description != NULL) {
		delete[] description;
	}

	//Clean up any current row fields.
	if (row != NULL) {
		delete[] row;
	}

	unsigned int db_type = _db->GetDBType();
	
	if (size_arr != NULL) delete[] size_arr;
	if (bind_params != NULL) delete[] bind_params;
	if (out_size_arr != NULL) delete[] out_size_arr;
	if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
	if (db_type == DATABASE_TYPE_MYSQL) 
		if (mysql_stmt != NULL) mysql_stmt_close(mysql_stmt);
	else if (db_type == DATABASE_TYPE_SQLITE)
		if (sqlite_stmt != NULL) sqlite3_finalize(sqlite_stmt);

	delete dbq;
}

/**
 * Process a query, possibly retrieving the next row of results.
*/
void Query::process() {
	unsigned int err = 0;
	unsigned int lasterror = 0;
	unsigned int db_type = _db->GetDBType();
	
	//Abort if query set to finished.
	if (status == DATABASE_QUERY_FINISHED) return;
	
	//Initialize database depending on type.
	if (db_type == DATABASE_TYPE_SQLITE) {
		if(sqlite_stmt == NULL) {
			if (sqlite3_prepare_v2(_db->sqlite_db, dbq->c_str(), 
				dbq->length(), &sqlite_stmt, 0))
				goto cleanup;
		}
	}
	else if (db_type == DATABASE_TYPE_MYSQL) {
		if(mysql_stmt == NULL) {
			mysql_stmt = mysql_stmt_init(_db->mysql_db);
			if (mysql_stmt == NULL)
				goto cleanup;
			err = mysql_stmt_prepare(mysql_stmt, dbq->c_str(), dbq->length());
		}
	}
	
	//Check for (and apply) parameters for parameterized queries.
	if((size_arr == NULL || bind_params == NULL) && params != NULL) {
		int m = params->size();
		//Set up MYSQL_BIND and size array for MYSQL parameters.
		if(db_type == DATABASE_TYPE_MYSQL) {
			size_arr = new unsigned long[m];
			bind_params = new MYSQL_BIND[m];
			memset(bind_params,0, sizeof(MYSQL_BIND)*m);
		}
		
		//Iterate over each parameter, binding as appropriate.
		for(int i = 0; i < m; i++) {
			string* p_str = &(*params)[i];
			if(db_type == DATABASE_TYPE_SQLITE)
				sqlite3_bind_text(sqlite_stmt, i+1, p_str->c_str(), 
					p_str->length(), SQLITE_STATIC);
			else if(db_type == DATABASE_TYPE_MYSQL) {
				bind_params[i].buffer_type = MYSQL_TYPE_STRING;
				bind_params[i].buffer = (char*) p_str->c_str();
				bind_params[i].is_null = 0;
				bind_params[i].length = &size_arr[i];
				size_arr[i] = p_str->length();
			}
		}
		if(db_type == DATABASE_TYPE_MYSQL) {
			if(err = mysql_stmt_bind_param(mysql_stmt, bind_params)) {
				goto cleanup;
			}
		}
	}
	
	//Query is new, populate query statistics, allocate row/description memory
	if (status == DATABASE_QUERY_INIT) {
		if (db_type == DATABASE_TYPE_SQLITE) {
			lasterror = sqlite3_step(sqlite_stmt);
			column_count = sqlite3_column_count(sqlite_stmt);
			lastrowid = sqlite3_last_insert_rowid(_db->sqlite_db);
			rows_affected = sqlite3_changes(_db->sqlite_db);
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			if (prepare_meta_result == NULL) {
				if (!(prepare_meta_result = mysql_stmt_result_metadata(mysql_stmt)))
					goto cleanup;
			}
			column_count = mysql_num_fields(prepare_meta_result);
			lastrowid = mysql_insert_id(_db->mysql_db);
			rows_affected = mysql_affected_rows(_db->mysql_db);
		}

		if (column_count == 0)
			goto cleanup;

		//Initialize row memory.
		if (row == NULL) {
			row = new webapp_str_t[column_count]();
		}
		
		//Initialize description memory.
		if (desc && description == NULL) {
			description = new webapp_str_t[column_count]();
		}
	} else if(status == DATABASE_QUERY_STARTED) {
		//Populate next row.
		if (db_type == DATABASE_TYPE_SQLITE) {
			lasterror = sqlite3_step(sqlite_stmt);
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			if (mysql_stmt_execute(mysql_stmt))
				goto cleanup;
		}
	}

	//Populate row data and column description
	if (db_type == DATABASE_TYPE_SQLITE) {
		if (lasterror == SQLITE_ROW) {
			for (int col = 0; col < column_count; col++) {
				//Push back the column name
				if (description != NULL && !havedesc) {
					description[col].data = sqlite3_column_name(sqlite_stmt, col);
					description[col].len = strlen(description[col].data);
				}
				//Push back the column text
				const char* text = (const char*)sqlite3_column_text(sqlite_stmt, col);
				int size = sqlite3_column_bytes(sqlite_stmt, col);
				row[col].data = text;
				row[col].len = size;
			}
			//We have the column description after the first pass.
			if (description != NULL) havedesc = 1;
		}
		else 
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
		}
		else 
			goto cleanup;
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
	mysql_stmt = NULL; sqlite_stmt = NULL;
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
	const char* password, const char* database) {
	db_type = database_type;
	if(database_type == DATABASE_TYPE_SQLITE) {
		int ret = sqlite3_open(host, &sqlite_db);
		if(sqlite_db == NULL || ret != SQLITE_OK) {
			return DATABASE_FAILED;
		}
	} else if(database_type == DATABASE_TYPE_MYSQL) {
		mysql_library_init(0, NULL, NULL);
		mysql_db = mysql_init(NULL);
		if(mysql_db == NULL) {
			return DATABASE_FAILED;
		}
		mysql_real_connect(mysql_db, host, username, password, database, 0, NULL, 0);
		//Disable mysql trunctation reporting.
		bool f = false;
		mysql_options(mysql_db, MYSQL_REPORT_DATA_TRUNCATION, &f);
	}
	return DATABASE_SUCCESS;
}

/**
 * Destroy a Database object.
*/
Database::~Database() {
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
long long Database::exec(const string& query) {
	Query q(this, query);
	q.process();
	return q.lastrowid;
}
