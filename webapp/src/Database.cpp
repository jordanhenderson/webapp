

#include "Database.h"
#include "my_global.h"
#include "mysql.h"


using namespace std;
Query::Query(Database* db, int desc) : _db(db) {
	params = new QueryRow();
	this->dbq = new string();
	this->desc = desc;
}

Query::Query(Database* db, const string& dbq, int desc) : _db(db) {
	params = new QueryRow();
	this->dbq = new string(dbq);
	this->desc = desc;
}

Query::~Query() {
	if(params != NULL)
		delete params;
	if (description != NULL) {
		for (int i = 0; i < column_count; i++) {
			if (description[i] != NULL)
				delete[] description[i];
		}
		delete[] description;
	}
	if (row != NULL) {
		for (int i = 0; i < column_count; i++) {
			if (row[i] != NULL)
				delete[] row[i];
		}
		delete[] row;
	}

	unsigned int db_type = _db->GetDBType();
		
	if (size_arr != NULL) delete[] size_arr;
	if (bind_params != NULL) delete[] bind_params;
	if (out_size_arr != NULL) delete[] out_size_arr;
	if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
	if (db_type == DATABASE_TYPE_MYSQL) {
		if (stmt != NULL) mysql_stmt_close((MYSQL_STMT*)stmt);
	}
	else if (db_type == DATABASE_TYPE_SQLITE) {
		if (stmt != NULL) sqlite3_finalize((sqlite3_stmt*)stmt);
	}

	delete dbq;
}

void Query::process() {
	if (status == DATABASE_QUERY_FINISHED) return;
	unsigned int err = 0;
	unsigned int lasterror = 0;
	unsigned int db_type = _db->GetDBType();

	if (stmt == NULL) {
		if (db_type == DATABASE_TYPE_SQLITE) {
			if (sqlite3_prepare_v2(_db->sqlite_db, dbq->c_str(), dbq->length(), (sqlite3_stmt**)&stmt, 0))
				goto cleanup;
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			stmt = mysql_stmt_init(_db->mysql_db);
			if (stmt == NULL)
				goto cleanup;
			err = mysql_stmt_prepare((MYSQL_STMT*)stmt, dbq->c_str(), dbq->length());
		}
	}
	//Check for (and apply) parameters)
	if((size_arr == NULL || bind_params == NULL) && params != NULL) {
		int m = params->size();
		if(db_type == DATABASE_TYPE_MYSQL) {
			size_arr = new unsigned long[m];
			bind_params = new MYSQL_BIND[m];
			memset(bind_params,0, sizeof(MYSQL_BIND)*m);
		}
		for(int i = 0; i < m; i++) {
			string* p_str = &(*params)[i];
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
	
	if (status == DATABASE_QUERY_INIT) {
		if (db_type == DATABASE_TYPE_SQLITE) {
			lasterror = sqlite3_step((sqlite3_stmt*)stmt);
			column_count = sqlite3_column_count((sqlite3_stmt*)stmt);
			lastrowid = sqlite3_last_insert_rowid(_db->sqlite_db);
			rows_affected = sqlite3_changes(_db->sqlite_db);
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			if (prepare_meta_result == NULL) {
				if (!(prepare_meta_result = mysql_stmt_result_metadata((MYSQL_STMT*)stmt)))
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
			row = new webapp_str_t*[column_count]();
			for (int i = 0; i < column_count; i++) {
				row[i] = new webapp_str_t;
			}
		}
		if (desc && description == NULL) {
			description = new webapp_str_t*[column_count]();
			for (int i = 0; i < column_count; i++) {
				description[i] = new webapp_str_t;
			}
		}
		
	}
	else if(status == DATABASE_QUERY_STARTED) {
		//Clean up existing row results.
		for (int i = 0; i < column_count; i++) {
			delete row[i]->data;
		}
		if (db_type == DATABASE_TYPE_SQLITE) {
			lasterror = sqlite3_step((sqlite3_stmt*)stmt);
		}
		else if (db_type == DATABASE_TYPE_MYSQL) {
			if (mysql_stmt_execute((MYSQL_STMT*)stmt))
				goto cleanup;
		}
	}

	if (db_type == DATABASE_TYPE_SQLITE) {
		if (lasterror == SQLITE_ROW) {
			for (int col = 0; col < column_count; col++) {
				//Push back the column name
				if (description != NULL && !havedesc) {
					const char* column = sqlite3_column_name((sqlite3_stmt*)stmt, col);
					int size = strlen(column);
					description[col]->data = new char[size + 1](); //TODO: investigate strlen alternative.
					description[col]->len = size;
					memcpy((char*)description[col]->data, column, size);
				}
				//Push back the column text
				const char* text = (const char*)sqlite3_column_text((sqlite3_stmt*)stmt, col);
				int size = sqlite3_column_bytes((sqlite3_stmt*)stmt, col);
				row[col]->data = new char[size + 1]();
				row[col]->len = size;
				memcpy((char*)row[col]->data, text, size);
			}
			//We have the column description after the first pass.
			if (description != NULL) havedesc = 1;
		}
		else {
			goto cleanup;
		}
		
		
	} else if (db_type == DATABASE_TYPE_MYSQL) {
		if (bind_output == NULL) {
			bind_output = new MYSQL_BIND[column_count];
			memset(bind_output, 0, sizeof(MYSQL_BIND)*column_count);
			out_size_arr = new unsigned long[column_count];
			//Collect sizes required for each column.
			for (int i = 0; i < column_count; i++) {
				bind_output[i].buffer_type = MYSQL_TYPE_STRING;
				bind_output[i].length = &out_size_arr[i];
			}
			mysql_stmt_bind_result((MYSQL_STMT*)stmt, bind_output);

			if (description != NULL && !havedesc) {
				MYSQL_FIELD *field;
				for (unsigned int i = 0; (field = mysql_fetch_field(prepare_meta_result)); i++) {
					description[i]->data = new char[field->name_length + 1]();
					description[i]->len = field->name_length;
					memcpy((char*)description[i]->data, field->name, field->name_length);
				}
				havedesc = 1;
			}
		}

		//Get the next row.
		if (mysql_stmt_fetch((MYSQL_STMT*)stmt)) {
			for (int i = 0; i < column_count; i++) {
				row[i] = new webapp_str_t[out_size_arr[i] + 1];
				bind_output[i].buffer = (void*)row[i]->data;
				bind_output[i].buffer_length = out_size_arr[i] + 1;
				mysql_stmt_fetch_column((MYSQL_STMT*)stmt, &bind_output[i], i, 0);
			}
		}
		else {
			goto cleanup;
		}

	}

//Update the query object
	status = DATABASE_QUERY_STARTED;
	return;
cleanup:
	status = DATABASE_QUERY_FINISHED;

	if (db_type == DATABASE_TYPE_MYSQL) {
		if (prepare_meta_result != NULL) mysql_free_result(prepare_meta_result);
		if (stmt != NULL) mysql_stmt_close((MYSQL_STMT*)stmt);
		if (size_arr != NULL) delete[] size_arr;
		if (bind_params != NULL) delete[] bind_params;
		if (out_size_arr != NULL) delete[] out_size_arr;
		size_arr = NULL;
		bind_params = NULL;
		out_size_arr = NULL;
		prepare_meta_result = NULL;
	}
	else if (db_type == DATABASE_TYPE_SQLITE) {
		if (stmt != NULL) sqlite3_finalize((sqlite3_stmt*)stmt);
	}
	stmt = NULL;


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
	query->process();
	if(nError != ERROR_DB_FAILED)
		return query->lastrowid;
	return -1;
}

long long Database::exec(const string& query) {
	Query q(this, query);
	return exec(&q);
}