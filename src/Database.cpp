#include "Database.h"
using namespace std;

Query::Query(string dbq) {
	this->dbq = dbq;
	params = NULL;
	response = NULL;
	description = NULL;
	
}

Query::~Query() {
	if(params != NULL)
		delete params;
	if(response != NULL)
		delete response;
	if(description != NULL)
		delete description;
}

void Database::process() {
	while(status == DATABASE_STATUS_PROCESS) {
		Query* qry;

		if(queue.try_pop(qry))  {
			sqlite3_stmt *stmt;
			if(sqlite3_prepare_v2(db, qry->dbq.c_str(), qry->dbq.length(), &stmt, 0))
				continue;
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

Query* Database::select(Query* query, int desc) {
	if(nError != ERROR_DB_FAILED) {
		
		//create the response and description vectors
		if(query->response == NULL) {
			QueryResponse* response = new QueryResponse();
			query->response = response;
		} 
		if(query->description == NULL && desc) {
			QueryRow* description = new QueryRow();
			query->description = description;
		}

		if(query->params == NULL) {
			QueryRow* params = new QueryRow();
			query->params = params;
		}
		//add it to the processing queue, wait for response
		
		queue.push(query);
		//Wait for status to be set to DATABASE_QUERY_FINISHED (blocking the calling thread)
		while(query->status != DATABASE_QUERY_FINISHED) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		//Clean up params.
		delete query->params;
		query->params = NULL;
		
	}
	return query;
}

Query* Database::select(const char* query, int desc) {
	Query* q = new Query(query);
	return select(q, desc);
}