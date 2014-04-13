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

/**
 * Lock/block all workers, then reload all data associated to the webapp
 * @param cleanupTask indicates if the caller has signalled the cleanup
 * process.
 * @param shutdown whether to shut down the webapp after cleaning.
*/
void Webapp::Cleanup(unsigned int cleanupTask, unsigned int shutdown) 
{
	cleanupLock.lock();
	if(cleanupTask == 1) {
		workers.Clean();
		if(shutdown) aborted = 1;
		svc.stop();
		cleanupLock.unlock();
	} else {
		cleanupLock.unlock();
	}
}

void Webapp::ToggleLevelDB() {
	//Enable/disable leveldb as required.
	if(!leveldb_enabled && db != NULL) {
		delete db;
		db = NULL;
	}
	else if(leveldb_enabled && db == NULL) {
		leveldb::Options options;
		options.filter_policy = leveldb::NewBloomFilterPolicy(10);
		options.create_if_missing = true;
		leveldb::DB::Open(options, session_dir, &db);
	}
}

/**
 * Reload all cached data as necessary.
*/
void Webapp::Reload()
{
	ToggleLevelDB();
	
	//Clear templates
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);

	templates.clear();

	//Delete cached preprocessed scripts.
	for(auto it: scripts) {
		delete it.second;
	}
	scripts.clear();

	//Clear any databases.
	for(auto it: databases) {
		delete it.second;
	}
	
	databases.clear();

	//Reset db_count to ensure new databases are created from index 0.
	db_count = 0;

	if(!aborted) {
		//Recompile scripts.
		webapp_str_t* init = CompileScript("plugins/core/init.lua");
		CompileScript("plugins/core/process.lua");

		//Run init script.
		RequestBase worker;
		Request r(svc);
		
		LuaParam _v[] = { { "request", &r },
						  { "worker", &worker } };
		RunScript(_v, sizeof(_v) / sizeof(LuaParam), "plugins/core/init.lua");
		
		//LevelDB may now be disabled. Clean it up as necessary.
		ToggleLevelDB();
		
		//Zero the init script (no need to hold in memory).
		*init = webapp_str_t();
		
		
	} else {
		svc.stop();
	}
}

int lua_writer(lua_State* L, const void* p, size_t sz, void* ud) {
	char* d = (char*) p;
	webapp_str_t src;
	webapp_str_t* dest = (webapp_str_t*)ud;
	src.data = d;
	src.len = sz;
	*dest += src;
	return 0;
}


/**
 * Compile a lua script (preprocess macros etc.)
 * Stores the compiled script text in scripts[output]
 * Relies on lua script "plugins/process.lua" and LuaMacro.
 * @param filename the script to preprocess
 * @return the compiled script, stored in scripts.
*/
webapp_str_t* Webapp::CompileScript(const char* filename)
{
	if(filename == NULL) return NULL;
	string filename_str = string(filename);
	//Attempt to return an existing compiled script.
	auto it = scripts.find(filename_str);
	if(it != scripts.end()) {
		//Found an existing script. Return it.
		return it->second;
	}
	
	//Create a new lua state, with minimal libs for LuaMacro.
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);

	//Execute the LuaMacro preprocessor.
	int ret = luaL_loadfile(L, "plugins/process.lua");
	if(ret) printf("Error: %s\n", lua_tostring(L, -1));

	//Provide the target filename to preprocess.
	lua_pushlstring(L, filename, strlen(filename));
	lua_setglobal(L, "file");

	//Execute the VM, store the results.
	webapp_str_t* chunk_str = new webapp_str_t();
	chunk_str->allocated = 1;
	const char* chunk = NULL;
	size_t len;
	if(lua_pcall(L, 0, 1, 0) != 0) goto lua_error;
	
	chunk = lua_tolstring(L, -1, &len);
	
	//Load the preprocessed chunk into the vm.
	if(luaL_loadbuffer(L, chunk, len, filename))
		goto lua_error;
	
	if(lua_dump(L, lua_writer, chunk_str) != 0) goto lua_error;
	
	scripts.emplace(filename_str, chunk_str);
	goto finish;
lua_error:
	delete chunk_str;
	printf("Error: %s\n", lua_tostring(L, -1));

finish:
	lua_close(L);
	return chunk_str;
}

/**
 * Run script stored in scripts map, passing in provided params.
 * @param params parameters to provide to the Lua instance
 * @param nArgs amount of parameters
 * @param filename of script to execute.
*/
void Webapp::RunScript(LuaParam* params, int nArgs, const char* file)
{
	auto it = scripts.find(file);
	if(it == scripts.end()) return;
	
	auto script = it->second;
	//Initialize a lua state, loading appropriate libraries.
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);

	//Allocate memory for temporary string operations.
	_webapp_str_t* static_strings[WEBAPP_STATIC_STRINGS];

	//Provide and set temporary string memory global.
	lua_pushlightuserdata(L, static_strings);
	lua_setglobal(L, "static_strings");

	//Pass each param into the Lua state.
	if(params != NULL) {
		for(int i = 0; i < nArgs; i++) {
			LuaParam* p = params + i;
			lua_pushlightuserdata(L, p->ptr);
			lua_setglobal(L, p->name);
		}
	}

	//Load the lua buffer.
	if(luaL_loadbuffer(L, script->data, script->len, file))
		goto lua_error;

	if(lua_pcall(L, 0, 0, 0) != 0)
		goto lua_error;

	goto finish;

lua_error:
	printf("Error: %s\n", lua_tostring(L, -1));

finish:
	lua_close(L);
}

/**
 * Webapp constructor.
 * @param io_svc the asio service object to listen upon
*/
Webapp::Webapp(const char* session_dir,
			   unsigned int port, io_service& io_svc) :
	session_dir(session_dir),
	port(port),
	svc(io_svc),
	wrk(io_svc)
{
	//Create the TCP endpoint and acceptor.
	int bound = 0;
	int failed = 0;

	while(!bound) {
		try {
			tcp::endpoint endpoint(tcp::v4(), port);
			acceptor = new tcp::acceptor(svc, endpoint, true);
			bound = 1;
		} catch (const asio::system_error& ec) {
			failed++;
			printf("Error: bind to %d failed: (%s)\n", port, ec.what());
			this_thread::sleep_for(chrono::milliseconds(1000));
			if(failed == 5) {
				exit(1);
			}
		}
	}
	accept_conn();
}

void Webapp::Start() {
	while(!aborted) {
		Reload();
		
		if(num_threads <= 0 || num_threads > WEBAPP_MAX_THREADS)
			num_threads = 1;
		workers.Start(num_threads);

		svc.run();
		svc.reset();
		
		//Clear workers
		workers.Cleanup();
	}
}

/**
 * Keep reading data in here, until the requested amount of bytes
 * have been read. The total bytes should be encoded and recieved
 * as a MessagePack integer.
 * @param r the Request object
 * @param ec the asio error code, if any
 * @param n_bytes the amount bytes transferred
*/
void Webapp::process_header_async(Request* r, const asio::error_code& ec, size_t n_bytes)
{
	if(!ec) {
		try {
			//Read in the next chunk of data from the socket
			uint16_t& n = r->socket.ctr;
			int32_t& headers_size = r->headers_buf.len;
			//If headers_size unknown, read 9 bytes (maximum size for number)
			int to_read = headers_size == 0 ? 9 : headers_size;
			//Load max to_read - n bytes into the buffer.
			n += r->socket.read_some(buffer(r->headers_buf.data + n,
											to_read - n));

			//If the header size has been read, and we have read all headers
			if(headers_size > 0 && n == headers_size) {
				r->lua_request = calloc(request_size, 1);
				workers.Enqueue(r);
				return;
			} else if(headers_size == 0 && n >= 1) {
				//If the header size hasn't been read, attempt to parse
				//a msgpack number.
				msgpack_unpacked result;
				msgpack_unpacked_init(&result);
				size_t offset = 0;

				if(msgpack_unpack_next(&result, r->headers_buf.data, n, &offset)) {
					msgpack_object obj = result.data;
					//If positive integer read, set the known headers_size.
					if(obj.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
						headers_size = (int32_t) obj.via.u64;
						//Reserve enough room for the entire header chunk.
						r->headers.resize(headers_size + offset);
						//Set the headers buffer location (needed by frontend)
						//to the actual header start location.
						r->headers_buf.data = r->headers.data() + offset;
						n -= offset;
					}
				}
				msgpack_unpacked_destroy(&result);
			}

			r->socket.async_read_some(null_buffers(), bind(
									  &Webapp::process_header_async,
									  this, r, _1, _2));

		} catch (...) {
			delete r;
			return;
		}
	} else {
		//Failed. Destroy request.
		delete r;
	}
}

/**
 * Begin to process a Request.
 * At this stage, the connection has been accepted asynchronously.
 * The next stage will be executed after async_read completes as necessary.
 * @param s the asio socket object of the accepted connection.
*/
void Webapp::accept_conn_async(Request* r, const asio::error_code& error)
{
	//Reset the header buffer. Allows reusing a request object.
	r->reset();
	try {
		r->socket.async_read_some(null_buffers(), bind(
									  &Webapp::process_header_async,
									  this, r, _1, _2));
		accept_conn();
	} catch(std::system_error er) {
		delete r;
		accept_conn();
	}
}

/**
 * First connection stage. Called to provide an entry point for new
 * connections. Re-called after each async_accept callback is completed.
*/
void Webapp::accept_conn()
{
	
	Request* r = new Request(svc);
	acceptor->async_accept(r->socket,
						   std::bind(&Webapp::accept_conn_async, this,
									 r, std::placeholders::_1));
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
	size_t id = db->GetID();
	databases.erase(id);
	delete db;
}

/**
 * Set a webapp parameter.
 * @param key the parameter key
 * @param value the value
*/
void Webapp::SetParamInt(unsigned int key, int value)
{
	switch(key) {
	case WEBAPP_PARAM_PORT:
		port = value;
		break;
	case WEBAPP_PARAM_ABORTED:
		aborted = value;
		break;
	case WEBAPP_PARAM_TPLCACHE:
		template_cache_enabled = value;
		break;
	case WEBAPP_PARAM_LEVELDB:
		leveldb_enabled = value;
		break;
	case WEBAPP_PARAM_THREADS:
		num_threads = value;
		break;
	case WEBAPP_PARAM_REQUESTSIZE:
		request_size = value;
		break;
	default:
		return;
		break;
	}
}

/**
 * Get a webapp parameter.
 * @param key the parameter key
 * @return the parameter
*/
int Webapp::GetParamInt(unsigned int key)
{
	switch(key) {
	case WEBAPP_PARAM_PORT:
		return port;
		break;
	case WEBAPP_PARAM_ABORTED:
		return aborted;
		break;
	case WEBAPP_PARAM_TPLCACHE:
		return template_cache_enabled;
		break;
	case WEBAPP_PARAM_LEVELDB:
		return leveldb_enabled;
		break;
	case WEBAPP_PARAM_THREADS:
		return num_threads;
		break;
	case WEBAPP_PARAM_REQUESTSIZE:
		return request_size;
		break;
	default:
		return 0;
		break;
	}
}
