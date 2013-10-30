#include "my_global.h"
#include "mysql.h"

#ifdef snprintf
#undef snprintf
#endif

#include "Database.h"

using namespace std;

Query::Query(const string& dbq, QueryRow* p) {
	this->dbq = new string(dbq);
	if(p != NULL) {
		params = new QueryRow(*p);
	} else {
		params = NULL;
	}
	description = NULL;
	status = DATABASE_QUERY_STARTED;
}

Query::~Query() {
	if(params != NULL)
		delete params;
	if(description != NULL)
		delete description;

	status = DATABASE_QUERY_FINISHED;

	if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if (stmt != NULL) mysql_stmt_close((MYSQL_STMT*)stmt);
		if (size_arr != NULL) delete[] size_arr;
		if (bind_params != NULL) delete[] bind_params;
		if (out_size_arr != NULL) delete[] out_size_arr;
	}
	else if (db_type == DATABASE_TYPE_SQLITE) {
		if (stmt != NULL) sqlite3_finalize((sqlite3_stmt*)stmt);
	}

	delete dbq;
}

void Database::process(Query* qry) {
	if(shutdown_database) return;
	void* stmt = qry->stmt;
	unsigned long* size_arr = qry->size_arr;
	unsigned long* out_size_arr = qry->out_size_arr;
	MYSQL_RES* prepare_meta_result = qry->prepare_meta_result;
	MYSQL_BIND* bind_params = qry->bind_params;
	MYSQL_BIND* bind_output = qry->bind_output;
	int err = 0;
	int havedesc = 0;
	int lasterror = 0;

	qry->db_type = db_type;
	if (stmt == NULL) {
		if (db_type == DATABASE_TYPE_SQLITE) {
			if (sqlite3_prepare_v2(sqlite_db, qry->dbq->c_str(), qry->dbq->length(), (sqlite3_stmt**)&stmt, 0))
				goto cleanup;
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			stmt = mysql_stmt_init(mysql_db);
			if (stmt == NULL)
				goto cleanup;
			err = mysql_stmt_prepare((MYSQL_STMT*)stmt, qry->dbq->c_str(), qry->dbq->length());
		}
	}
	//Check for (and apply) parameters)
	if((size_arr == NULL || bind_params == NULL) && qry->params != NULL) {
		int m = qry->params->size();
		if(db_type == DATABASE_TYPE_MYSQL) {
			size_arr = new unsigned long[m];
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
			
	//Clear previous row.
	qry->row.clear();

	//Retrieve the next row.

	if (db_type == DATABASE_TYPE_SQLITE) {
		if (qry->column_count == 0)
			qry->column_count = sqlite3_column_count((sqlite3_stmt*)stmt);
		if (lasterror = sqlite3_step((sqlite3_stmt*)stmt) == SQLITE_ROW) {
			for (int col = 0; col < qry->column_count; col++) {
				//Push back the column name
				if (qry->description != NULL && !havedesc)
					qry->description->push_back(sqlite3_column_name((sqlite3_stmt*)stmt, col));
				//Push back the column text
				const char* text = (const char*)sqlite3_column_text((sqlite3_stmt*)stmt, col);
				int size = sqlite3_column_bytes((sqlite3_stmt*)stmt, col);
				qry->row.push_back(string(text, size));
			}
			//We have the column description after the first pass.
			havedesc = 1;
		}
		else {
			goto cleanup;
		}
		
		qry->lastrowid = sqlite3_last_insert_rowid(sqlite_db);
		//Finally, destroy the statement.
		
	}
	else if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result == NULL) {
			if (!(prepare_meta_result = mysql_stmt_result_metadata((MYSQL_STMT*)stmt)))
				goto cleanup;

			if (mysql_stmt_execute((MYSQL_STMT*)stmt))
				goto cleanup;
		}
		if (qry->column_count == 0)
			qry->column_count = mysql_num_fields(prepare_meta_result);

		if (bind_output == NULL) {
			bind_output = new MYSQL_BIND[qry->column_count];
			memset(bind_output, 0, sizeof(MYSQL_BIND)*qry->column_count);
			out_size_arr = new unsigned long[qry->column_count];
			//Collect sizes required for each column.
			for (int i = 0; i < qry->column_count; i++) {
				bind_output[i].buffer_type = MYSQL_TYPE_STRING;
				bind_output[i].length = &out_size_arr[i];
			}
			mysql_stmt_bind_result((MYSQL_STMT*)stmt, bind_output);

			if (qry->description != NULL && !havedesc) {
				MYSQL_FIELD *field;
				for (unsigned int i = 0; (field = mysql_fetch_field(prepare_meta_result)); i++) {
					qry->description->push_back(string(field->name, field->name_length));
				}
			}
		}

		//Get the next row.
		if (mysql_stmt_fetch((MYSQL_STMT*)stmt)) {
			for (int i = 0; i < qry->column_count; i++) {
				char* cell = new char[out_size_arr[i] + 1];
				bind_output[i].buffer = (void*)cell;
				bind_output[i].buffer_length = out_size_arr[i] + 1;
				mysql_stmt_fetch_column((MYSQL_STMT*)stmt, &bind_output[i], i, 0);
				delete[] cell;
				qry->row.push_back(string(cell, bind_output[i].buffer_length));

			}
		}
		else {
			goto cleanup;
		}
	}

//Update the query object
	qry->stmt = stmt;
	if (db_type == DATABASE_TYPE_MYSQL) {
		qry->prepare_meta_result = prepare_meta_result;
		qry->size_arr = size_arr;
		qry->bind_params = bind_params;
		qry->out_size_arr = out_size_arr;
	}
	return;
cleanup:
	qry->status = DATABASE_QUERY_FINISHED;

	if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if (stmt != NULL) mysql_stmt_close((MYSQL_STMT*)stmt);
		if (size_arr != NULL) delete[] size_arr;
		if (bind_params != NULL) delete[] bind_params;
		if (out_size_arr != NULL) delete[] out_size_arr;
	}
	else if (db_type == DATABASE_TYPE_SQLITE) {
		if (stmt != NULL) sqlite3_finalize((sqlite3_stmt*)stmt);
	}

} 

	

Database::Database() {
}

int Database::connect(int database_type, const char* host, const char* username, const char* password, const char* database) {
	db_type = database_type;
	if(database_type == DATABASE_TYPE_SQLITE) {
		int ret = sqlite3_open(host, &sqlite_db);

		if(sqlite_db == NULL || ret != SQLITE_OK) {
			nError = ERROR_DB_FAILED;
			return DATABASE_FAILED;
		}
	} else if(database_type == DATABASE_TYPE_MYSQL) {
		mysql_library_init(0, NULL, NULL);
		mysql_db = mysql_init(NULL);
		if(mysql_db == NULL) {
			nError = ERROR_DB_FAILED;
			return DATABASE_FAILED;
		}
		mysql_real_connect(mysql_db, host, username, password, database, 0, NULL, 0);
		//Disable mysql trunctation reporting.
		bool f = false;
		mysql_options(mysql_db, MYSQL_REPORT_DATA_TRUNCATION, &f);
	}
	return DATABASE_SUCCESS;
}

Database::~Database() {
	if(db_type == DATABASE_TYPE_SQLITE && sqlite_db != NULL)
		sqlite3_close(sqlite_db);
	else if(db_type == DATABASE_TYPE_MYSQL && mysql_db != NULL) {
		mysql_close(mysql_db); 
		mysql_library_end();
	}

}


long long Database::exec(Query* query) {
	select(query, NULL);
	if(nError != ERROR_DB_FAILED)
		return query->lastrowid;
	return -1;
}

long long Database::exec(const string& query, QueryRow* params) {
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

	if(nError != ERROR_DB_FAILED) {
		process(q);
	}
	return q;
}

string Database::select_single(const std::string& query, QueryRow* params, const std::string& def) {
	Query q(query);
	select(&q, params, 0);
	if(q.row.size() > 0)
		return q.row.at(0);
	else return def;

}