#include "Database.h"

using namespace std;

Query::Query(const string& dbq, QueryRow* p, int copy) {
	this->dbq = new string(dbq);
	if(p != NULL) {
		params_copy = copy;
		if(copy) 
			params = new QueryRow(*p);
		else
			params = p;
	} else {
		params = NULL;
	}
	response = NULL;
	description = NULL;
	status = DATABASE_QUERY_STARTED;
}

Query::~Query() {
	if(params != NULL && params_copy)
		delete params;
	if(response != NULL)
		delete response;
	if(description != NULL)
		delete description;

	delete dbq;
}

void Database::process(Query* qry) {

	if(abort) return;
		

	sqlite3_stmt *stmt;
	if(qry->status == DATABASE_QUERY_FINISHED) {
		return;
	}

	//Prepare the statement
	if(sqlite3_prepare_v2(db, qry->dbq->c_str(), qry->dbq->length(), &stmt, 0)) {
		//Error preparing statement.
		qry->status = DATABASE_QUERY_FINISHED;
		return;
	}

	//Check for (and apply) parameters)
	if(qry->params != NULL) {
		int m = qry->params->size();
		for(int i = 0; i < m; i++)
			sqlite3_bind_text(stmt, i+1, (*qry->params)[i].c_str(), (*qry->params)[i].length(), SQLITE_STATIC);	
	}
			
			
	int havedesc = 0;
	int lasterror = sqlite3_step(stmt);
	while(lasterror == SQLITE_ROW) {
		if(qry->response != NULL) {
			vector<string> row;
			for(int col = 0; col < sqlite3_column_count(stmt); col++) {
				//Push back the column name
				if(qry->description != NULL && !havedesc)
					qry->description->push_back(sqlite3_column_name(stmt, col));
				//Push back the column text
				const char* text = (const char*)sqlite3_column_text(stmt, col);
				int size = sqlite3_column_bytes(stmt, col);
				row.push_back(string(text,size));
			}
			//We have the column description after the first pass.
			havedesc = 1;
			//Push back the row retrieved.
			qry->response->push_back(row);
		}
		lasterror = sqlite3_step(stmt);
	}

	qry->lastrowid = sqlite3_last_insert_rowid(db);
			
				
	qry->status = DATABASE_QUERY_FINISHED;
			
	//Finally, destroy the statement.
	sqlite3_finalize(stmt);
} 

	


Database::Database(const char* filename) {
	abort = 0;
	db = NULL;
	int ret = sqlite3_open(filename, &db);
	if(db == NULL || ret != SQLITE_OK) {
		nError = ERROR_DB_FAILED;
		return;
	}
	//Create the db queue thread
}

Database::~Database() {
	abort = 1;
	if(nError != ERROR_DB_FAILED) {
		if(dbthread->joinable())
			dbthread->join();
		delete dbthread;
	}
	sqlite3_close(db);
}

void Database::select(Query* query) {
	if(nError != ERROR_DB_FAILED) {
		process(query);
	}
}

int Database::exec(Query* query) {
	select(query);
	if(nError != ERROR_DB_FAILED)
		return query->lastrowid;
	return 0;
}

int Database::exec(const string& query, QueryRow* params) {
	Query q(query, params);
	//Bypass unique_ptr handling
	int nRet = exec(&q);
	return nRet;
}

Query Database::select(const string& query, int desc) {
	return select(query, NULL, desc);
}

Query Database::select(const string& query, QueryRow* params, int desc) {
	Query q(query, params);
	if(desc)
		q.description = new QueryRow();

	q.response = new QueryResponse();

	select(&q);
	return q;
}

