/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include <leveldb/filter_policy.h>
#include "Webapp.h"
#include "Session.h"
#include "Database.h"
#include "Image.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace std::placeholders;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif 

int32_t strntol(const char* src, size_t n) {
    int32_t x = 0;
    while(isdigit(*src) && n--) {
    	x = x * 10 + (*src - '0');		
    	src++;
    }
    return x;
}

webapp_str_t operator+(const webapp_str_t& w1, const webapp_str_t& w2) {
    webapp_str_t n = w1;
    n += w2;
    return n;
}

webapp_str_t operator+(const char* lhs, const webapp_str_t& rhs) {
    return webapp_str_t(lhs) + rhs;
}

webapp_str_t operator+(const webapp_str_t& lhs, const char* rhs) {
    return lhs + webapp_str_t(rhs);
}

static const char* script_t_names[] = {SCRIPT_NAMES};
/**
 * Request destructor.
*/
Request::~Request() {
	for(auto it: strings) delete it;
	for(auto it: queries) delete it;
    for(auto it: sessions) delete it;
	if (socket != NULL) delete socket;
	if (v != NULL) delete v;
	if (headers != NULL) delete headers;
}

/**
 * Clean up a worker thread. Called as each task is signalled to finish.
 * Performs actual cleanup from originally signalled thread while
 * other threads are waiting/blocked (q->finished = 1)
*/
void WebappTask::handleCleanup() {
	//Set the finished flag to signify this thread is waiting.
	_q->finished = 1;
	
	int cleaning = _handler->start_cleanup(_q);
	//Actual cleaning stage, performed after all workers terminated.
	if(cleaning) {
		if(_q->shutdown) _handler->SetParamInt(WEBAPP_PARAM_ABORTED, 1);
		_handler->perform_cleanup();
	} else {
		//Just (attempt to) grab own lock. 
		unique_lock<mutex> lk(_q->cv_mutex);
		//(Automatically) unlock when done to complete the process.
	}
}

/**
 * Execute a worker task.
 * Worker tasks process queues of recieved requests (each single threaded)
 * The LUA VM must only block after handling each request.
 * Performs cleanup when finished.
*/
void WebappTask::start() {
	 _worker = std::thread([this] {
		 while (!_handler->GetParamInt(WEBAPP_PARAM_ABORTED)) {
			 _q->finished = 0;
			Execute();
			handleCleanup();
		 }
	 });
}

RequestQueue::~RequestQueue() {
}

void RequestQueue::Execute() {
	LuaParam _v[] = { { "worker", this },
					  { "app", _handler } };
	_handler->RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_REQUEST);
}

void RequestQueue::Cleanup() {
    _sessions.CleanupSessions();
	if(_cache != NULL) delete _cache;
	
	if(baseTemplate != NULL) delete baseTemplate;
	baseTemplate = new TemplateDictionary("");
	
	templates.clear();
	
	for(auto tmpl: _handler->templates) {
		TemplateDictionary* dict = baseTemplate->AddIncludeDictionary(tmpl.first);
		dict->SetFilename(tmpl.second);
		templates.insert({tmpl.first, dict});
	}

	if(_handler->GetParamInt(WEBAPP_PARAM_TPLCACHE)) {
		_cache = mutable_default_template_cache()->Clone();
		_cache->Freeze();
	} else {
		_cache = NULL;
	}
}

string* RequestQueue::RenderTemplate(const webapp_str_t& page) {
	string* output = new string();
	if(_cache)
	_cache->ExpandNoLoad(page, STRIP_WHITESPACE, baseTemplate, NULL,
						  output);
	else {
		mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::LAZY_RELOAD);
		ExpandTemplate(page, STRIP_WHITESPACE, baseTemplate, output);
	}
	return output;
}

void BackgroundQueue::Execute() {
	LuaParam _v[] = { { "bgworker", this },
					  { "app", _handler } };
	_handler->RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_QUEUE);
}

/**
 * Begin the reload process by stopping all workers.
 * @param _q the taskqueue of the cleaning up worker.
 * @return 1 if the calling thread should perform the actual cleanup.
*/
int Webapp::start_cleanup(TaskQueue* _q) {
	int cleaning = 0;
	//Lock the following to prevent more than one thread from
	//cleaning at a time.
	unique_lock<mutex> lk(cleanupLock); 
	if (_q->cleanupTask == 1) {
		cleaning = 1;
        workers.Clean();
        bg_workers.Clean();
	}
	return cleaning;
}

/**
 * Perform the actual cleanup process, reloading any cached data.
 * Signals all workers to unpause, when cleanup is finished.
*/
void Webapp::perform_cleanup() {
	//Reload the webapp.
    reload_all();
    workers.Restart();
    bg_workers.Restart();

}

/**
 * Compile a lua script (preprocess macros etc.)
 * Stores the compiled script text in scripts[output]
 * Relies on lua script "plugins/process.lua" and LuaMacro.
 * @param filename the script to preprocess
 * @param output the destination webapp_str_t
*/
void Webapp::CompileScript(const char* filename, webapp_str_t* output) {
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
		if(output->data != NULL) delete[] output->data;
		output->data = new char[len];
		output->len = len;
		memcpy((char*)output->data, chunk, len);
	}
	lua_close(L);
}

/**
 * Reload all cached data as necessary.
*/
void Webapp::reload_all() {
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
		LuaParam _v[] = { { "app", this } };
		RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_INIT);

		if (background_queue_enabled)
			CompileScript("plugins/core/process_queue.lua", &scripts[SCRIPT_QUEUE]);
	} else {
		svc.stop();
	}

    workers.Cleanup();
    bg_workers.Cleanup();
}

/**
 * Run script stored in scripts[] array, passing in provided params.
 * @param params parameters to provide to the Lua instance
 * @param nArgs amount of parameters
 * @param index of script in scripts[] to execute.
*/
void Webapp::RunScript(LuaParam* params, int nArgs, script_t script) {
	if(scripts[script].data == NULL) return;
	Request* r = NULL;
	
	//Initialize a lua state, loading appropriate libraries.
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);
	
	//Allocate memory for temporary string operations.
    _webapp_str_t* static_strings = new _webapp_str_t[WEBAPP_STATIC_STRINGS];
	
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
	
	//If script is SCRIPT_INIT, provide a temporary Request object
	//to operate on (Request needed when calling handlers)
	if(script == SCRIPT_INIT) {
		r = new Request();
		lua_pushlightuserdata(L, r);
		lua_setglobal(L, "tmp_request");
	}
	
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
	delete[] static_strings;
	if(r != NULL) delete r;
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
    reload_all();

    if(!background_queue_enabled) {
        workers.Start(this, WEBAPP_NUM_THREADS > 1
                      ? WEBAPP_NUM_THREADS * 2 :
                        WEBAPP_NUM_THREADS);
    }
    else {
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

/**
 * Process a recieved request. 
 * By this stage, the entire request has been recieved. This stage reads
 * in/sets each variable and passes the request to lua workers.
 * @param request the Request object
 * @param ec the asio error code, if any.
 * @param bytes_transferred the amount of bytes transferred.
*/
void Webapp::process_request_async(
    Request* request, const asio::error_code& ec, std::size_t bytes_transferred) {
	size_t read = 0;
	
	//Read each input chain variable recieved from nginx appropriately.
	for(int i = 0; i < STRING_VARS; i++) {
        char* h = (char*) request->v->data() + read;
        request->input_chain[i]->data = h;
        read += request->input_chain[i]->len;
	}

    unsigned int selected_node = node_counter++ % WEBAPP_NUM_THREADS;
    workers.workers[selected_node]->enqueue(request);
}

/**
 * Begin to process the request header. At this stage, exactly
 * PROTOCOL_LENGTH_SIZEINFO bytes have been recieved, providing all 
 * variable length data for further stages.
 * @param r the Request object
 * @param ec the asio error code, if any
 * @param bytes_transferred the amount bytes transferred
*/
void Webapp::process_header_async(Request* r, const asio::error_code& ec,
	std::size_t bytes_transferred) {
    if(!ec) {
        int16_t* headers = (int16_t*)r->headers->data();
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
		
		r->v = new std::vector<char>(len);
		asio::async_read(*r->socket, asio::buffer(*r->v), transfer_exactly(len),
			std::bind(&Webapp::process_request_async, this, r, _1, _2));
	}
}

/**
 * Begin to process a Request. 
 * At this stage, the connection has been accepted asynchronously.
 * The next stage will be executed after async_read completes as necessary.
 * @param s the asio socket object of the accepted connection.
*/
void Webapp::accept_conn_async(tcp::socket* s, const asio::error_code& error) {
	Request* r = new Request();
	r->socket = s;
	r->headers = new std::vector<char>(PROTOCOL_LENGTH_SIZEINFO);
	try {
		asio::async_read(*r->socket, asio::buffer(*r->headers), 
			transfer_exactly(PROTOCOL_LENGTH_SIZEINFO), bind(
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
void Webapp::accept_conn() {
	tcp::socket* current_socket = new tcp::socket(svc);
	acceptor->async_accept(*current_socket, 
		std::bind(&Webapp::accept_conn_async, this, 
			current_socket, std::placeholders::_1));
}

/**
 * Deconstruct Webapp object.
*/
Webapp::~Webapp() {
    delete db;
	//Abort each worker request queue and session.

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
Database* Webapp::CreateDatabase() {
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
Database* Webapp::GetDatabase(size_t index) {
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
void Webapp::DestroyDatabase(Database* db) {
	size_t id = db->GetID();
	databases.erase(id);
	delete db;
}

/**
 * Set a webapp parameter.
 * @param key the parameter key
 * @param value the value
*/
void Webapp::SetParamInt(unsigned int key, int value) {
	switch(key) {
		case WEBAPP_PARAM_PORT: port = value; break;
		case WEBAPP_PARAM_BGQUEUE: background_queue_enabled = value; break;
		case WEBAPP_PARAM_ABORTED: aborted = value; break;
		case WEBAPP_PARAM_TPLCACHE: template_cache_enabled = value; break;
		default: return; break;
	}
}

/**
 * Get a webapp parameter.
 * @param key the parameter key
 * @return the parameter
*/
int Webapp::GetParamInt(unsigned int key) {
	switch(key) {
		case WEBAPP_PARAM_PORT: return port; break;
		case WEBAPP_PARAM_BGQUEUE: return background_queue_enabled; break;
		case WEBAPP_PARAM_ABORTED: return aborted; break;
		case WEBAPP_PARAM_TPLCACHE: return template_cache_enabled; break;
		default: return 0; break;
	}
}
