/* Copyright (C) Jordan Henderson - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
*/

#include "Webapp.h"
#include "Session.h"

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace std::placeholders;

/**
 * Execute a worker task.
 * Worker tasks process queues of recieved requests (each single threaded)
 * The LUA VM must only block after handling each request.
 * Performs cleanup when finished.
*/
void WebappTask::Start()
{
	_worker = std::thread(&WebappTask::Execute, this);
}

void WebappTask::Stop()
{
	if(_worker.joinable()) _worker.join();
}

/**
 * First connection stage. Called to provide an entry point for new
 * connections. Re-called after each async_accept callback is completed.
*/
void RequestBase::accept_conn()
{
	Request* r = finished_requests.try_dequeue();
	if(r == NULL) {
		/* Create a new request. We may need to allocate a new pool here. */
		if(current_request >= request_pool_size || current_request == 0) {
			//Allocate a new pool, use that pool from now on.
			request_pools.emplace_back();
			request_pools.back().resize(request_pool_size);
			current_pool = request_pools.size() - 1;
			current_request = 1;
			r = &r_pool[0];
		} else {
			r = &request_pools.at(current_pool)[current_request];
			current_request++;
		}
	} else {
		//Using an existing request. Reset it first.
		r->reset();
	}
	acceptor->async_accept(r->s.socket,
						   bind(&RequestBase::accept_conn_async, this,
								r, _1));
}

/**
 * Begin to process a Request.
 * At this stage, the connection has been accepted asynchronously.
 * The next stage will be executed after async_read completes as necessary.
 * @param s the asio socket object of the accepted connection.
*/
void RequestBase::accept_conn_async(Request* r, const std::error_code& error)
{
	if(!error) {
		Socket& s = r->s.socket;
		try {
			read_request(r, 5000);
			accept_conn();
		} catch(std::system_error er) {
			s.abort();
			finished_requests.enqueue(r);
			accept_conn();
		}
	} else {
		accept_conn();
	}
}

void RequestBase::begin_read(Request* r, int timeout_ms)
{
	Socket& s = r->s.socket;
	if (timeout_ms > 0) {
		s.timer.expires_from_now(chrono::milliseconds(timeout_ms));
		s.timer.async_wait(bind(&RequestBase::process_msgpack_request, 
								this, r, _1, 0));
	}
	s.async_read_some(null_buffers(), bind(
					  &RequestBase::process_msgpack_request,
					  this, r, _1, _2));
}

/**
 * Main msgpack request processing function. Attempts to read initial
 * length (msgpack encoded "number"). Then attempts to read the 
 * provided amount of bytes.
 * @param r the Request object
 * @param ec the asio error code, if any
 * @param n_bytes the amount bytes transferred
*/
void RequestBase::process_msgpack_async(Request* r, 
										const std::error_code& ec, 
										size_t n_bytes)
{
	Socket& s = r->s.socket;
	if(!ec) {
		s.timer.cancel();
		try {
			//Read in the next chunk of data from the socket
			uint16_t& n = s.ctr;
			int32_t& headers_size = r->headers_buf.len;
			//If headers_size unknown, read 9 bytes (maximum size for number)
			int to_read = headers_size == 0 ? 9 : headers_size;
			//Load max to_read - n bytes into the buffer.
			n += s.read_some(buffer(r->headers_buf.data + n,
											to_read - n));

			if(headers_size == 0 && n >= 1) {
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
						if(headers_size + offset > 9) {
							r->headers.resize(headers_size + offset);
						}
						//Set the headers buffer location (needed by frontend)
						//to the actual header start location.
						r->headers_buf.data = r->headers.data() + offset;
						n -= offset;
					}
				}
				msgpack_unpacked_destroy(&result);
			}
			
			//If the header size has been read, and we have read all headers
			if(headers_size > 0 && n >= headers_size) {
				if(r->lua_request == NULL) {
					r->lua_request = calloc(request_size, 1);
				} else {
					memset(r->lua_request, 0, request_size);
				}
				workers.EnqueueRequest(r);
				return;
			}
			
			read_request(r, 5000);
			
		} catch (...) {
			s.abort();
			finished_requests.enqueue(r);
		}
	} else if(ec != asio::error::operation_aborted) {
		s.abort();
		finished_requests.enqueue(r);
	} 
}



RequestQueue::RequestQueue(const WorkerInit& init) :
	WorkerInit(init), RequestBase(queue_size), 
	endpoint(tcp::v4(), port), 
	acceptor(svc, endpoint, true)
{
}

RequestQueue::~RequestQueue()
{
	if(_cache != NULL) delete _cache;
	if(baseTemplate != NULL) delete baseTemplate;
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

void RequestQueue::CompileScript(const webapp_str_t& filename) {
	webapp_str_t* chunk_str = NULL;

	//Hold parsed lua code.
	const char* chunk = NULL;
	size_t len = 0;

	//Convert filename to string for unordered_map storage.
	string filename_str = filename;

	//Attempt to return an existing compiled script.
	auto it = scripts.find(filename_str);
	if(it != scripts.end()) {
		//Found an existing script. Return it.
		return it->second;
	}
	
	//Create a new lua state, with minimal libs for LuaMacro.
	webapp_str_t actual_filename = "plugins/" + filename;
	lua_State* L = luaL_newstate();
	if(L == NULL) goto lua_fail;
	luaL_openlibs(L);
	luaopen_lpeg(L);

	//Execute the LuaMacro preprocessor.
	if(luaL_loadfile(L, "plugins/parse.lua")) goto lua_error;

	//Provide the target filename to preprocess.
	lua_pushlstring(L, actual_filename.data, actual_filename.len);
	lua_setglobal(L, "file");

	//Execute the VM, store the results.
	chunk_str = new webapp_str_t();
	chunk_str->allocated = 1;

	if(lua_pcall(L, 0, 1, 0) != 0) goto lua_error;
	
	chunk = lua_tolstring(L, -1, &len);
	
	//Load the preprocessed chunk into the vm.
	if(luaL_loadbuffer(L, chunk, len, filename))
		goto lua_error;
	
	if(lua_dump(L, lua_writer, chunk_str) != 0) goto lua_error;
	
	scripts.insert(make_pair(filename_str, chunk_str));
	goto finish;
lua_fail:
	delete chunk_str;
	return NULL;
lua_error:
	delete chunk_str;
	chunk_str = NULL;
	printf("Error: %s\n", lua_tostring(L, -1));
finish:
	lua_close(L);
	return chunk_str;

}

void RequestQueue::Cleanup()
{
	if(db != NULL) {
		delete db;
		db = NULL;
	}
	
	if(templates_enabled) {
		//Clear templates
		mutable_default_template_cache()->
			ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);
	}

	for(auto it: scripts) delete it.second;
	scripts.clear();
	
	for(auto it: databases) delete it.second;
	databases.clear();
	db_count = 0;
	
	if(!app->aborted) {
		templates.clear();
		//Recompile scripts.
		webapp_str_t* init = CompileScript("init.lua");
		if(init != NULL) {
			CompileScript(this->script);

			//Create mock classes to allow init.lua to run functions 
			//that require a request/request worker.
			RequestBase worker(WEBAPP_DEFAULT_QUEUESIZE);
			Request r(client_svc);

			LuaParam _v[] = { { "request", &r },
							  { "worker", &worker } };
			RunScript(_v, sizeof(_v) / sizeof(LuaParam), "init.lua");

			//Zero the init script (no need to hold in memory).
			*init = webapp_str_t();
		}
	} else {
		if(client_sockets) client_svc.stop();
	}
}

void RequestQueue::Start()
{
	Reload();
	if(leveldb_enabled && db == NULL) {
		leveldb::Options options;
		options.filter_policy = leveldb::NewBloomFilterPolicy(10);
		options.create_if_missing = true;
		leveldb::DB::Open(options, session_dir, &db);
	}
	
	client_svc.stop();
	client_svc.reset();
	
	if(client_socket_thread.joinable())
		client_socket_thread.join();
	
	if(client_sockets) {
		client_socket_thread = std::thread([this] {
			client_svc.run();
		});
	}
	
	svc_thread = std::thread([this] {
		svc.run();
	});
	
	accept_conn();

	WebappTask::Start();
}

void RequestQueue::Stop()
{
	if(svc_thread.joinable()) svc_thread.join();
	WebappTask::Stop();
}

void RequestQueue::Execute()
{
	if(app->templates_enabled) {
		baseTemplate = new TemplateDictionary("");
		for(auto tmpl: app->templates) {
			TemplateDictionary* dict = baseTemplate->AddIncludeDictionary(tmpl.first);
			dict->SetFilename(tmpl.second);
			templates.insert({tmpl.first, dict});
		}
		if(app->template_cache_enabled) {
			_cache = mutable_default_template_cache()->Clone();
			_cache->Freeze();
		}
	}
	
	//FAfinished = 0;
	LuaParam _v[] = { { "worker", (RequestBase*)this } };
	RunScript(_v, sizeof(_v) / sizeof(LuaParam));
	
	if(_cache != NULL) {
		delete _cache;
		_cache = NULL;
	}

	if(baseTemplate != NULL) {
		delete baseTemplate;
		baseTemplate = NULL;
	}
	//Set the finished flag to signify this thread is waiting.
	//FAfinished = 1;
}

void RequestQueue::RunScript(LuaParam* params, int nArgs, 
							 const webapp_str_t& file)
{
	auto it = scripts.find(file);
	if(it == scripts.end()) return;
	
	auto script = it->second;
	//Initialize a lua state, loading appropriate libraries.
	lua_State* L = luaL_newstate();
	if(L == NULL) return;
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);

	//Allocate memory for temporary string operations.
	_webapp_str_t ss[static_strings];

	//Provide and set temporary string memory global.
	lua_pushlightuserdata(L, &ss);
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

webapp_str_t* RequestQueue::RenderTemplate(const webapp_str_t& page)
{
	webapp_str_t* output = new webapp_str_t();
	if(app->templates_enabled) {
		WebappStringEmitter wse(output);
		if(_cache != NULL)
			_cache->ExpandNoLoad(page, STRIP_WHITESPACE, baseTemplate, NULL, &wse);
		else {
			mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::LAZY_RELOAD);
			ExpandTemplate(page, STRIP_WHITESPACE, baseTemplate, &wse);
		}
	}
	return output;
}
