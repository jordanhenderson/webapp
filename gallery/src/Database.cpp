#include "Database.h"
#include "my_global.h"
#include "mysql.h"
using namespace std;

Query::Query(const string& dbq, QueryRow* p) {
	this->dbq = new string(dbq);
	if(p != NULL) {
		params = new QueryRow(*p);
	} else {
		params = NULL;
	}
	response = NULL;
	description = NULL;
	status = DATABASE_QUERY_STARTED;
}

Query::~Query() {
	if(params != NULL)
		delete params;
	if(response != NULL)
		delete response;
	if(description != NULL)
		delete description;

	delete dbq;
}

void Database::process(Query* qry) {

	if(shutdown_database) return;
	void* stmt = NULL;
	unsigned long* size_arr = NULL;
	unsigned long* out_size_arr = NULL;
	MYSQL_RES* prepare_meta_result = NULL;
	MYSQL_BIND* bind_params = NULL;
	MYSQL_BIND* bind_output = NULL;
	int err = 0;
	int havedesc = 0;
	int lasterror = 0;
	if(db_type == DATABASE_TYPE_SQLITE) {
		if(sqlite3_prepare_v2(sqlite_db, qry->dbq->c_str(), qry->dbq->length(), (sqlite3_stmt**)&stmt, 0)) 
			goto cleanup;
	} else if(db_type == DATABASE_TYPE_MYSQL) {
		stmt = mysql_stmt_init(mysql_db);
		if(stmt == NULL) 
			goto cleanup;
		err = mysql_stmt_prepare((MYSQL_STMT*)stmt, qry->dbq->c_str(), qry->dbq->length());
	}

	//Check for (and apply) parameters)
	if(qry->params != NULL) {
		
		if(db_type == DATABASE_TYPE_MYSQL) 
			size_arr = new unsigned long[qry->params->size()];
		
		int m = qry->params->size();
		if(db_type == DATABASE_TYPE_MYSQL) {
			bind_params = new MYSQL_BIND[m];
			memset(bind_params,0, sizeof(MYSQL_BIND)*m);
		}
		for(int i = 0; i < m; i++) {
			string* p_str = &(*qry->params)[i];
			if(db_type == DATABASE_TYPE_SQLITE)
				sqlite3_bind_text((sqlite3_stmt*)stmt, i+1, p_str->c_str(), p_str->length(), SQLITE_STATIC);	
			else if(db_type == DATABASE_TYPE_MYSQL) {
				
				
				bind_params[i].buffer_type = MYSQL_TYPE_STRING;
				bind_params[i].buffer = (char*) p_str->c_str();
				bind_params[i].is_null = 0;
				bind_params[i].length = &size_arr[i];
				size_arr[i] = p_str->length();
			}
		}
		if(db_type == DATABASE_TYPE_MYSQL) {
			if(err = mysql_stmt_bind_param((MYSQL_STMT*)stmt, bind_params)) {
				goto cleanup;
			}
		}
	}
			
	if(db_type == DATABASE_TYPE_SQLITE) {
		while((lasterror = sqlite3_step((sqlite3_stmt*)stmt)) == SQLITE_ROW) {
			vector<string> row;
			if(qry->response != NULL) {
				for(int col = 0; col < sqlite3_column_count((sqlite3_stmt*)stmt); col++) {
					//Push back the column name
					if(qry->description != NULL && !havedesc)
						qry->description->push_back(sqlite3_column_name((sqlite3_stmt*)stmt, col));
					//Push back the column text
					const char* text = (const char*)sqlite3_column_text((sqlite3_stmt*)stmt, col);
					int size = sqlite3_column_bytes((sqlite3_stmt*)stmt, col);
					row.push_back(string(text,size));
				}
				//We have the column description after the first pass.
				havedesc = 1;
				//Push back the row retrieved.
				qry->response->push_back(row);
			}
		}
		qry->lastrowid = sqlite3_last_insert_rowid(sqlite_db);
		//Finally, destroy the statement.
		
	} else if(db_type == DATABASE_TYPE_MYSQL) {
		if(qry->response != NULL) {
			if(!(prepare_meta_result = mysql_stmt_result_metadata((MYSQL_STMT*)stmt)))
				goto cleanup;
		
			if(mysql_stmt_execute((MYSQL_STMT*)stmt)) 
				goto cleanup;

			int column_count = mysql_num_fields(prepare_meta_result);
			bind_output = new MYSQL_BIND[column_count];
			memset(bind_output,0, sizeof(MYSQL_BIND)*column_count);
			out_size_arr = new unsigned long[column_count];
			//Collect sizes required for each column.
			for(int i = 0; i < column_count; i++) {
				bind_output[i].buffer_type = MYSQL_TYPE_STRING;
				bind_output[i].length = &out_size_arr[i];
			}
			mysql_stmt_bind_result((MYSQL_STMT*)stmt, bind_output);
			//Get the next row.
			

			//Get the cell.


			while(!mysql_stmt_fetch((MYSQL_STMT*)stmt)) {
				vector<string> row;
				for(int i = 0; i < column_count; i++) {
			
					char* cell =  new char[out_size_arr[i]+1];
					bind_output[i].buffer = (void*)cell;
					bind_output[i].buffer_length = out_size_arr[i]+1;
					mysql_stmt_fetch_column((MYSQL_STMT*)stmt, &bind_output[i], i, 0);
					string c(cell);
					delete[] cell;
					row.push_back(c);

				}
				qry->response->push_back(row);
			}

		}
	}

cleanup:
	qry->status = DATABASE_QUERY_FINISHED;
			
	if(db_type == DATABASE_TYPE_MYSQL) {
		if(prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if(stmt != NULL) mysql_stmt_close((MYSQL_STMT*)stmt);
		if(size_arr != NULL) delete[] size_arr;
		if(bind_params != NULL) delete[] bind_params;
		if(out_size_arr != NULL) delete[] out_size_arr;
	} else if(db_type == DATABASE_TYPE_SQLITE) {
		if(stmt != NULL) sqlite3_finalize((sqlite3_stmt*)stmt);
	}
} 

	

Database::Database(int database_type, const std::string& host, const std::string& username, const std::string& password, const std::string& database) {
	shutdown_database = 0;
	sqlite_db = NULL;
	mysql_db = NULL;
	db_type = database_type;
	if(database_type == DATABASE_TYPE_SQLITE) {
		int ret = sqlite3_open(host.c_str(), &sqlite_db);

		if(sqlite_db == NULL || ret != SQLITE_OK) {
			nError = ERROR_DB_FAILED;
			return;
		}
	} else if(database_type == DATABASE_TYPE_MYSQL) {
		mysql_db = mysql_init(NULL);
		if(mysql_db == NULL) {
			nError = ERROR_DB_FAILED;
			return;
		}
		mysql_real_connect(mysql_db, host.c_str(), username.c_str(), password.c_str(), database.c_str(), 0, NULL, 0);
		//Disable mysql trunctation reporting.
		bool f = false;
		mysql_options(mysql_db, MYSQL_REPORT_DATA_TRUNCATION, &f);
	}
}

Database::~Database() {
	shutdown_database = 1;
	if(nError != ERROR_DB_FAILED) {
		if(dbthread->joinable())
			dbthread->join();
		delete dbthread;
	}
	if(db_type == DATABASE_TYPE_SQLITE)
		sqlite3_close(sqlite_db);
	else if(db_type == DATABASE_TYPE_MYSQL)
		mysql_close(mysql_db);
}


int Database::exec(Query* query) {
	select(query, NULL);
	if(nError != ERROR_DB_FAILED)
		return query->lastrowid;
	return -1;
}

int Database::exec(const string& query, QueryRow* params) {
	Query q(query, params);
	return exec(&q);
}


Query* Database::select(Query* q, QueryRow* params, int desc) {
	if(params != NULL && q->params == NULL) {
		q->params = new QueryRow(*params);
		q->params_copy = 1;
	}
	if(desc)
		q->description = new QueryRow();
	q->response = new QueryResponse();
	if(nError != ERROR_DB_FAILED) {
		process(q);
	}
	return q;
}

string Database::select(const std::string& query, QueryRow* params, int desc) {
	Query q(query);
	select(&q, params, desc);
	return q.response->at(0).at(0);
}