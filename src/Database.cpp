#include "Database.h"
using namespace std;

Query::Query(string dbq) {
	this->dbq = dbq;
	params = NULL;
	response = NULL;
	description = NULL;
	
}

Query::~Query() {
}

void Database::process() {
	while(status == DATABASE_STATUS_PROCESS) {
		Query* qry;

		if(queue.try_pop(qry))  {
			sqlite3_stmt *stmt;
			if(sqlite3_prepare_v2(db, qry->dbq.c_str(), qry->dbq.length(), &stmt, 0)) {
				qry->status = DATABASE_QUERY_FINISHED;
				continue;
			}
			std::vector<std::string> params = *qry->params;
			for(int i = 0; i < qry->params->size(); i++) {
				sqlite3_bind_text(stmt, i, params[i].c_str(), params[i].length(), SQLITE_STATIC);
			}
			if(qry->response != NULL) {
				
				int havedesc = 0;
				while(sqlite3_step(stmt) == SQLITE_ROW) {
					
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
				sqlite3_finalize(stmt);
				qry->status = DATABASE_QUERY_FINISHED;
			}

		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
	sqlite3_close(db);
}

void Database::select(unique_ptr<Query>& query, int desc) {
	if(nError != ERROR_DB_FAILED) {
		
		//create the response and description vectors
		if(query->response == NULL) {
			unique_ptr<QueryResponse> response = unique_ptr<QueryResponse>(new QueryResponse());
			query->response = std::move(response);
		} 
		if(query->description == NULL && desc) {
			unique_ptr<QueryRow> description = unique_ptr<QueryRow>(new QueryRow());
			query->description = std::move(description);
		}

		if(query->params == NULL) {
			unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
			query->params = std::move(params);
		}
		//add it to the processing queue, wait for response
		//Release the query object from management, add it to the queue.
		Query* q = query.release();
		queue.push(q);
		//Wait for status to be set to DATABASE_QUERY_FINISHED (blocking the calling thread)
		while(q->status != DATABASE_QUERY_FINISHED) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		query.reset(q);		
	}
}

unique_ptr<Query> Database::select(const char* query, int desc) {
	unique_ptr<Query> q = unique_ptr<Query>(new Query(query));
	select(q, desc);
	return q;
}

unique_ptr<Query> Database::select(const char* query, unique_ptr<QueryRow>& params, int desc) {
	unique_ptr<Query> q = unique_ptr<Query>(new Query(query));
	q->params = std::move(params);
	select(q, desc);
	return q;
}

