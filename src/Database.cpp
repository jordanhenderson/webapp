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



void Database::process() {
	while(status == DATABASE_STATUS_PROCESS) {
		Query* qry;

		if(queue.try_pop(qry))  {
			sqlite3_stmt *stmt;
			if(sqlite3_prepare_v2(db, qry->dbq->c_str(), qry->dbq->length(), &stmt, 0)) {
				qry->status = DATABASE_QUERY_FINISHED;
				continue;
			}
			if(qry->params != NULL) {
				
				int m = qry->params->size();
				for(int i = 0; i < m; i++) {
					sqlite3_bind_text(stmt, i+1, (*qry->params)[i].c_str(), (*qry->params)[i].length(), SQLITE_STATIC);
				}
			}
			
				
				int havedesc = 0;
				int lasterror = sqlite3_step(stmt);
				while(lasterror == SQLITE_ROW) {
					if(qry->response != NULL) {
					vector<string> row;
					for(int col = 0; col < sqlite3_column_count(stmt); col++) {
						if(qry->description != NULL && !havedesc) {
							qry->description->push_back(sqlite3_column_name(stmt, col));
							
						}
						const char* text = (const char*)sqlite3_column_text(stmt, col);
						int size = sqlite3_column_bytes(stmt, col);
						
						row.push_back(string(text,size));
					}
					
					havedesc = 1;
					qry->response->push_back(row);
					}
					lasterror = sqlite3_step(stmt);
				}
				sqlite3_finalize(stmt);
				qry->lastrowid = sqlite3_last_insert_rowid(db);
				qry->status = DATABASE_QUERY_FINISHED;
		}
		this_thread::sleep_for(chrono::milliseconds(1));
	}
}

Database::Database(const char* filename) {
	db = NULL;
	sqlite3_open(filename, &db);
	if(db == NULL) {
		nError = ERROR_DB_FAILED;
		return;
	}
	//Create the db queue thread
	status = DATABASE_STATUS_PROCESS;
	dbthread = thread(&Database::process, this);
}

Database::~Database() {
	status = DATABASE_STATUS_FINISHED;
	if(dbthread.joinable())
		dbthread.join();
	sqlite3_close(db);
}

void Database::select(Query* query) {
	if(nError != ERROR_DB_FAILED) {
		//add it to the processing queue, wait for response
		//Release the query object from management, add it to the queue.
		
		queue.push(query);
		//Wait for status to be set to DATABASE_QUERY_FINISHED (blocking the calling thread)
		while(query->status != DATABASE_QUERY_FINISHED) {
			this_thread::sleep_for(chrono::milliseconds(1));
		}

		
	}
}

void Database::select(unique_ptr<Query>& query) {
	Query* q = query.release();
	select(q);
	query.reset(q);
}

int Database::exec(Query* query) {
	select(query);
	if(nError != ERROR_DB_FAILED)
		return query->lastrowid;
	return 0;
}

int Database::exec(unique_ptr<Query>& query) {
	Query* q = query.release();
	int nRet = exec(q);
	query.reset(q);
	return nRet;

}

int Database::exec(const string& query, QueryRow* params) {
	Query q(query, params);
	//Bypass unique_ptr handling
	int nRet = exec(&q);
	return nRet;
}

unique_ptr<Query> Database::select(const string& query, int desc) {
	return select(query, NULL, desc);
}

unique_ptr<Query> Database::select(const string& query, QueryRow* params, int desc) {
	auto q = unique_ptr<Query>(new Query(query, params));
	if(desc)
		q->description = new QueryRow();

	q->response = new QueryResponse();

	select(q);
	return q;
}
