/* Copyright (C) Jordan Henderson - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
*/

#include <msgpack.h>
#include "Webapp.h"
#include "Session.h"

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

Request::Request(io_service& svc) : s(svc), socket_ref(s)
{
	headers.resize(9);
	reset();
}

RequestBase::RequestBase(unsigned int request_size,
						 unsigned int queue_size,
						 unsigned int request_pool_size) : 
							LockedQueue(queue_size),
							finished_requests(queue_size),
							request_pool_size(request_pool_size),
							request_size(request_size),
							wrk(svc),
							client_wrk(client_svc),
							acceptor(svc),
							resolver(client_svc)
{
	//RequestBase specific initialisation handling
	if(request_pool_size == 0) {
		this->request_pool_size = 100;
	}
	
	if(request_size == 0) {
		this->request_size = 1;
	}
}

RequestBase::~RequestBase()
{
	for(Request* r_pool: request_pools) {
		for(int i = 0; i < request_pool_size; i++) {
			r_pool[i].~Request();
		}
		delete[] (char*)r_pool;
	}
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
			char* pool = new char[request_pool_size * sizeof(Request)];
			Request* r_pool = (Request*)pool;
			for(int i = 0; i < request_pool_size; i++) {
				new (r_pool + i) Request(svc);
			}
			request_pools.push_back(r_pool);
			current_pool = request_pools.size() - 1;
			current_request = 1;
			r = &request_pools.back()[0];
		} else {
			r = &request_pools.at(current_pool)[current_request];
			current_request++;
		}
	} else {
		//Using an existing request. Reset it first.
		r->reset();
	}
	acceptor.async_accept(r->s.socket,
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
		printf("Error accepting connection: %s\n", error.message().c_str()); 
		accept_conn();
	}
}

void RequestBase::read_request(Request* r, int timeout_ms)
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
void RequestBase::process_msgpack_request(Request* r,
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
				enqueue(r);
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

LuaSocket* RequestBase::create_socket(Request* r, const webapp_str_t& addr,
						 const webapp_str_t& port)
{
	tcp::resolver::query qry(tcp::v4(), addr, port);
	LuaSocket* s = new LuaSocket(client_svc);
	resolver.async_resolve(qry, bind(&RequestBase::resolve_handler,
									 this, s, r, _1, _2));
	return s;
}

/* Socket API */
void RequestBase::connect_handler(LuaSocket* s, Request* r,
					const std::error_code& ec,
					tcp::resolver::iterator it)
{
	Socket& socket = s->socket;
	if(!ec) {
		enqueue(r);
	} else if (it != tcp::resolver::iterator()) {
		//Try next endpoint.
		socket.abort();
		tcp::endpoint ep = *it;
		socket.async_connect(ep, bind(&RequestBase::connect_handler,
									  this, s, r, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		socket.abort();
		enqueue(r);
	}
}

void RequestBase::resolve_handler(LuaSocket *s, Request* r,
								  const std::error_code &ec,
								  tcp::resolver::iterator it)
{
	Socket& socket = s->socket;
	if(!ec) {
		tcp::endpoint ep = *it;
		socket.async_connect(ep, bind(&RequestBase::connect_handler,
									  this, s, r, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		socket.abort();
		enqueue(r);
	}
}

void RequestBase::read_handler(LuaSocket *s, Request *r, webapp_str_t *output,
							   int timeout, const std::error_code &ec,
							   size_t n_bytes)
{
	Socket& socket = s->socket;
	if(!ec) {
		socket.timer.cancel();
		try {
			uint16_t& n = socket.ctr;
			n += socket.read_some(buffer(output->data + n, output->len - n));
			if(n == output->len) {
				//read complete.
				enqueue(r);
			} else {
				socket.timer.expires_from_now(chrono::seconds(timeout));
				socket.timer.async_wait(bind(&RequestBase::read_handler, this, s, r,
											  output, timeout, _1, 0));
				socket.async_read_some(null_buffers(), bind(&RequestBase::read_handler,
					this, s, r, output, timeout, _1, 0));
			}
		} catch (...) {
			socket.abort();
			output->len = 0;
		}
	} else if(ec != asio::error::operation_aborted) {
		//Read failed/timeout.
		socket.abort();
		output->len = 0;
	}
}

webapp_str_t* RequestBase::start_read(LuaSocket* s, Request* r, int bytes, int timeout)
{
	Socket& socket = s->socket;
	webapp_str_t* output = new webapp_str_t(bytes);
	socket.ctr = 0;

	socket.timer.expires_from_now(chrono::seconds(timeout));
	socket.timer.async_wait(bind(&RequestBase::read_handler, this, s, r,
								  output, timeout, _1, 0));
	socket.async_read_some(null_buffers(), bind(&RequestBase::read_handler, this, s,
							r, output, timeout, _1, _2));
	return output;
}

void RequestBase::write_handler(LuaSocket *s, webapp_str_t *buf,
								const std::error_code& error,
								size_t bytes_transferred)
{
	Socket& socket = s->socket;
	socket.waiting--;
	delete buf;
}

void RequestBase::start_write(LuaSocket *s, const webapp_str_t &buf)
{
	Socket& socket = s->socket;
	//TODO: investigate leak here.
	webapp_str_t* tmp_buf = new webapp_str_t(buf);
	socket.waiting++;

	//No try/catch statement needed; async_write always succeeds.
	//Errors handled in callback.
	async_write(socket, buffer(tmp_buf->data, tmp_buf->len),
					  bind(&RequestBase::write_handler, this, s, tmp_buf, _1, _2));
}

void RequestBase::reenqueue(Request* r) {
	//Reenqueue a request to the requestbase using asio.
	svc.post(bind(&RequestBase::enqueue, this, r));
}

Worker::Worker(const WorkerInit& init) :
	WorkerInit(init), RequestBase(init.request_size, init.queue_size,
								  init.request_pool_size),
	endpoint(tcp::v4(), port),
	script(_script)
{
	if(port > 0) {
		try {
			acceptor.open(endpoint.protocol());
			acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
			acceptor.bind(endpoint);
			acceptor.listen();
		} catch (system_error& ec) {
			printf("Error: bind to %d failed: (%s)\n", port, ec.what());
		}
	}

	if(templates_enabled) baseTemplate = new TemplateDictionary("");
}

Worker::~Worker()
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

webapp_str_t* Worker::CompileScript(const webapp_str_t& filename) {
	//Hold parsed lua code.
	webapp_str_t* chunk_str = NULL;
	const char* chunk = NULL;
	size_t len = 0;

	//Convert filename to string for unordered_map storage.
	string filename_str = filename;

	//Attempt to return an existing compiled script.
	auto& scripts = app->scripts;
	
	auto it = scripts.find(filename_str);
	
	if(it != scripts.end()) {
		//Found an existing script. Return it.
		return &it->second;
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
	
	if(lua_pcall(L, 0, 1, 0) != 0) goto lua_error;
	
	chunk = lua_tolstring(L, -1, &len);
	
	//Load the preprocessed chunk into the vm.
	if(luaL_loadbuffer(L, chunk, len, filename.data))
		goto lua_error;
	
	
	chunk_str = scripts.emplace(filename_str, webapp_str_t());
	
	if(lua_dump(L, lua_writer, chunk_str) != 0) goto lua_error;
	
	goto finish;
lua_fail:
	return chunk_str;
lua_error:
	printf("Error: %s\n", lua_tostring(L, -1));
finish:
	lua_close(L);
	return chunk_str;
}

void Worker::Cleanup()
{
	if(templates_enabled) {
		//Clear templates
		mutable_default_template_cache()->
			ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);
	}

	if(!aborted) {
		//Recompile scripts.
		webapp_str_t* init = CompileScript("init.lua");
		if(init != NULL) {
			CompileScript(this->script);

			//Create mock classes to allow init.lua to run functions 
			//that require a request/request worker.
			RequestBase worker(1, 1, 1);
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

void Worker::Start()
{
	if(client_sockets) {
		client_svc.stop();

		if(client_socket_thread.joinable())
			client_socket_thread.join();
		
		client_svc.reset();
		client_socket_thread = std::thread([this] {
			client_svc.run();
		});
	}
	
	svc_thread = std::thread([this] {
		svc.run();
	});
	
	accept_conn();

	//Execute the worker as a separate thread.
	WebappTask::Start();
}

void Worker::Stop()
{
	if(svc_thread.joinable()) svc_thread.join();
	WebappTask::Stop();
}

void Worker::Execute()
{
	Cleanup();
	if(templates_enabled) {
		for(auto tmpl: app->templates) {
			TemplateDictionary* dict = baseTemplate->AddIncludeDictionary(tmpl.first);
			dict->SetFilename(tmpl.second);
			templates.insert({tmpl.first, dict});
		}

		if(templates_cache_enabled) {
			_cache = mutable_default_template_cache()->Clone();
			_cache->Freeze();
		}
	}
	
	LuaParam _v[] = { { "worker", (RequestBase*)this } };
	RunScript(_v, sizeof(_v) / sizeof(LuaParam), script);
	
	//Signal the service to stop.
	svc.stop();
}

void Worker::RunScript(LuaParam* params, int nArgs, 
							 const webapp_str_t& file)
{
	auto& scripts = app->scripts;
	auto it = scripts.find(file);
	if(it == scripts.end()) return;
	
	//Hold the actual script file for lua.
	webapp_str_t actual_file = "plugins/" + file;
	
	auto& script = it->second;
	//Initialize a lua state, loading appropriate libraries.
	lua_State* L = luaL_newstate();
	if(L == NULL) return;
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);

	//Allocate memory for temporary string operations.
	_webapp_str_t ss[WEBAPP_STATIC_STRINGS];

	//Provide and set temporary string memory global.
	lua_pushlightuserdata(L, &ss);
	lua_setglobal(L, "static_strings");
	
	lua_pushinteger(L, WEBAPP_STATIC_STRINGS);
	lua_setglobal(L, "static_strings_count");

	//Pass each param into the Lua state.
	if(params != NULL) {
		for(int i = 0; i < nArgs; i++) {
			LuaParam* p = params + i;
			lua_pushlightuserdata(L, p->ptr);
			lua_setglobal(L, p->name);
		}
	}

	//Load the lua buffer.
	if(luaL_loadbuffer(L, script.data, script.len, file.data))
		goto lua_error;

	if(lua_pcall(L, 0, 0, 0) != 0)
		goto lua_error;

	goto finish;

lua_error:
	printf("Error: %s\n", lua_tostring(L, -1));

finish:
	lua_close(L);

}

webapp_str_t* Worker::RenderTemplate(const webapp_str_t& page)
{
	webapp_str_t* output = new webapp_str_t();
	if(templates_enabled) {
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

