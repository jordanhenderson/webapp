/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include <leveldb/filter_policy.h>
#include "Webapp.h"
#include "Session.h"
#include "Database.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <lpeg.h>
#include <cjson.h>
#include <msgpack.h>
}

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace std::placeholders;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif

Request::Request(io_service& svc) : s(svc), socket_ref(s)
{
	headers.resize(9);
	reset();
}

void Webapp::StartCleanup() 
{
	cleanupLock.lock();
}

void Webapp::FinishCleanup()
{
	cleanupLock.unlock();
}

void Webapp::CreateWorker(const WorkerInit& init)
{
	workers.Add(init);
}

void Webapp::Start() {
	while(!aborted) {
		workers.Start();
		//Clear workers
		workers.Cleanup();
	}
}

LuaSocket* Webapp::create_socket()
{
	return new LuaSocket(client_svc);
}

void Webapp::destroy_socket(LuaSocket* s)
{
	s->socket.abort();
	delete s;
}

tcp::resolver& Webapp::get_resolver()
{
	return resolver;
}

/**
 * Deconstruct Webapp object.
*/
Webapp::~Webapp()
{
	workers.Stop();

	//Perform leveldb cleanup if necessary.
	if(db != NULL) delete db;

	for(auto db : databases) {
		delete db.second;
	}
	
	for(auto it: scripts) {
		delete it.second;
	}
}

/**
 * Create a Database object, incrementing the db_count.
 * Store the Database object in the databases hashmap.
 * @return the newly created Database object.
*/
Database* Webapp::CreateDatabase()
{
	size_t id = db_count++;
	Database* db = new Database(id);
	databases.insert({id, db});
	return db;
}

/**
 * Retrieve a Database object from the databases hashmap, using the
 * provided index.
 * @param index the Database object key. See db_count in CreateDatabase.
 * @return the newly created Database object.
*/
Database* Webapp::GetDatabase(size_t index)
{
	try {
		return databases.at(index);
	} catch (...) {
		return NULL;
	}
}

/**
 * Destroy a Database object.
 * @param db the Database object to destroy
*/
void Webapp::DestroyDatabase(Database* db)
{
	databases.erase(db->db_id);
	delete db;
}
