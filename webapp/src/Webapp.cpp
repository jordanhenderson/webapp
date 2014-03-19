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
#include "lpeg.h"
#include <cjson.h>
}

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace std::placeholders;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif

static const char* script_t_names[] = {SCRIPT_NAMES};
/**
 * Request destructor.
*/
Request::~Request()
{
	for(auto it: strings) delete it;
	for(auto it: queries) delete it;
	for(auto it: sessions) delete it;
}

/**
 * Lock/block all workers, then reload all data associated to the webapp
 * @param cleanupTask pointer to an integer which indicates if the 
 * caller has signalled the cleanup process. This is required to be a
 * pointer, as multiple threads may call into this function at the same
 * time, and will affect the cleanupTask value of other workers, 
 * preventing a cleanup task from occuring twice.
 * @param shutdown whether to shut down the webapp after cleaning.
*/
void Webapp::Cleanup(unsigned int* cleanupTask, unsigned int shutdown) 
{
	cleanupLock.lock();
	if(*cleanupTask == 1) {
		workers.Clean();
		bg_workers.Clean();
		if(shutdown) SetParamInt(WEBAPP_PARAM_ABORTED, 1);
		Reload();
		workers.Restart();
		bg_workers.Restart();
		cleanupLock.unlock();
	} else {
		cleanupLock.unlock();
	}
}

/**
 * Compile a lua script (preprocess macros etc.)
 * Stores the compiled script text in scripts[output]
 * Relies on lua script "plugins/process.lua" and LuaMacro.
 * @param filename the script to preprocess
 * @param output the destination webapp_str_t
*/
void Webapp::CompileScript(const char* filename, webapp_str_t* output)
{
	if(filename == NULL || output == NULL) return;
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
	if(lua_pcall(L, 0, 1, 0) != 0)
		printf("Error: %s\n", lua_tostring(L, -1));
	else {
		size_t len;
		const char* chunk = lua_tolstring(L, -1, &len);
		*output = webapp_str_t(chunk, len);
	}
	lua_close(L);
}

/**
 * Reload all cached data as necessary.
*/
void Webapp::Reload()
{
	//Clear templates
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);

	templates.clear();

	//Delete cached preprocessed scripts.
	for(int i = 0; i < WEBAPP_SCRIPTS; i++) {
		if(scripts[i].data != NULL) {
			delete[] scripts[i].data;
			scripts[i].data = NULL;
		}
	}

	//Clear any databases.
	for(auto it = databases.cbegin(); it != databases.cend(); ) {
		delete (*it).second;
		databases.erase(it++);
	}

	//Reset db_count to ensure new databases are created from index 0.
	db_count = 0;

	if(!aborted) {
		//Recompile scripts.
		CompileScript("plugins/core/init.lua", &scripts[SCRIPT_INIT]);
		CompileScript("plugins/core/process.lua", &scripts[SCRIPT_REQUEST]);
		CompileScript("plugins/core/handlers.lua", &scripts[SCRIPT_HANDLERS]);

		//Run init script.
		Request r(svc);
		RequestBase worker(this);
		LuaParam _v[] = { { "app", this }, { "request", &r },
						  { "worker", &worker} };
		RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_INIT);

		if (background_queue_enabled)
			CompileScript("plugins/core/process_queue.lua", &scripts[SCRIPT_QUEUE]);

	} else {
		svc.stop();
	}
}

/**
 * Run script stored in scripts[] array, passing in provided params.
 * @param params parameters to provide to the Lua instance
 * @param nArgs amount of parameters
 * @param index of script in scripts[] to execute.
*/
void Webapp::RunScript(LuaParam* params, int nArgs, script_t script)
{
	if(scripts[script].data == NULL) return;

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

	//Preload handlers.
	if(luaL_loadbuffer(L, scripts[SCRIPT_HANDLERS].data,
					   scripts[SCRIPT_HANDLERS].len, "core/handlers"))
		goto lua_error;

	if(lua_pcall (L, 0, 1, 0))
		goto lua_error;

	//Store loaded script buffer in 'load_handlers' global.
	lua_setfield (L, LUA_GLOBALSINDEX, "load_handlers");

	//Load the lua buffer.
	if(luaL_loadbuffer(L, scripts[script].data, scripts[script].len,
					   script_t_names[script]))
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
			   unsigned int port, asio::io_service& io_svc) :
	session_dir(session_dir),
	port(port),
	svc(io_svc),
	wrk(svc)
{
	leveldb::Options options;
	options.filter_policy = leveldb::NewBloomFilterPolicy(10);
	options.create_if_missing = true;
	leveldb::DB::Open(options, session_dir, &db);

	Reload();

	if(!background_queue_enabled) {
		workers.Start(this, WEBAPP_NUM_THREADS > 1
					  ? WEBAPP_NUM_THREADS * 2 :
					  WEBAPP_NUM_THREADS);
	} else {
		workers.Start(this, WEBAPP_NUM_THREADS);
		bg_workers.Start(this, WEBAPP_NUM_THREADS);
	}

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
	svc.run();
}

/**
 * Process a recieved request.
 * By this stage, the entire request has been recieved. This stage reads
 * in/sets each variable and passes the request to lua workers.
 * @param request the Request object
 * @param ec the asio error code, if any.
 * @param bytes_transferred the amount of bytes transferred.
*/
void Webapp::process_request_async(
	Request* r, const asio::error_code& ec, size_t n_bytes)
{
	if(!ec) {
		int n = r->headers_body.size();
		int max = r->headers_body.capacity();
		n += r->socket.read_some(buffer(&r->headers_body[n], max - n));
		if(n != max) {
			r->socket.async_read_some(null_buffers(),
									  std::bind(&Webapp::process_request_async, this, r, _1, _2));
			return;
		}
		size_t read = 0;

		//Read each input chain variable recieved from nginx appropriately.
		for(int i = 0; i < STRING_VARS; i++) {
			char* h = (char*) r->headers_body.data() + read;
			r->input_chain[i]->data = h;
			read += r->input_chain[i]->len;
		}

		unsigned int selected_node = node_counter++ % WEBAPP_NUM_THREADS;
		workers.workers[selected_node]->enqueue(r);
	} else {
		delete r;
	}
}

/**
 * Begin to process the request header. At this stage, exactly
 * PROTOCOL_LENGTH_SIZEINFO bytes have been recieved, providing all
 * variable length data for further stages.
 * @param r the Request object
 * @param ec the asio error code, if any
 * @param bytes_transferred the amount bytes transferred
*/
void Webapp::process_header_async(Request* r, const asio::error_code& ec, size_t n_bytes)
{
	if(!ec) {
		int n = r->headers.size();
		n += r->socket.read_some(buffer(&r->headers[n], PROTOCOL_LENGTH_SIZEINFO - n));
		if(n != PROTOCOL_LENGTH_SIZEINFO) {
			r->socket.async_read_some(null_buffers(), bind(
										  &Webapp::process_header_async, this, r, _1, _2));
			return;
		}

		int16_t* headers = (int16_t*)r->headers.data();
		//At this stage, at least PROTOCOL_SIZELENGTH_INFO has been read into the buffer.
		//STATE: Read protocol.
		r->uri.len =          ntohs(*headers++);
		r->host.len =         ntohs(*headers++);
		r->user_agent.len =   ntohs(*headers++);
		r->cookies.len =      ntohs(*headers++);
		r->request_body.len = ntohs(*headers++);
		r->method =           ntohs(*headers++);
		//Update the input chain.
		r->input_chain[0] = &r->uri;
		r->input_chain[1] = &r->host;
		r->input_chain[2] = &r->user_agent;
		r->input_chain[3] = &r->cookies;
		r->input_chain[4] = &r->request_body;

		size_t len = 0;
		for(int i = 0; i < STRING_VARS; i++) {
			len += r->input_chain[i]->len;
		}

		r->headers_body.reserve(len);
		r->socket.async_read_some(null_buffers(),
								  std::bind(&Webapp::process_request_async, this, r, _1, _2));
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
	try {
		r->socket.async_read_some(null_buffers(), bind(
									  &Webapp::process_header_async, this, r, _1, _2));
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
	bg_workers.Stop();

	delete db;

	//Delete databases
	for(auto db_entry : databases) {
		delete db_entry.second;
	}

	//Delete loaded scripts
	for(int i = 0; i < WEBAPP_SCRIPTS; i++) {
		if(scripts[i].data != NULL) {
			delete[] scripts[i].data;
		}
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
	case WEBAPP_PARAM_BGQUEUE:
		background_queue_enabled = value;
		break;
	case WEBAPP_PARAM_ABORTED:
		aborted = value;
		break;
	case WEBAPP_PARAM_TPLCACHE:
		template_cache_enabled = value;
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
	case WEBAPP_PARAM_BGQUEUE:
		return background_queue_enabled;
		break;
	case WEBAPP_PARAM_ABORTED:
		return aborted;
		break;
	case WEBAPP_PARAM_TPLCACHE:
		return template_cache_enabled;
		break;
	default:
		return 0;
		break;
	}
}
