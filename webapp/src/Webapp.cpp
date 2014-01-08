/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"
#include "Image.h"

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif 

void WebappTask::cleanup(TaskQueue* q) {
	q->cleanupTask = 0; //Only clean up once!
	//Set the queue to aborted
	q->aborted = 1;
	//Notify any blocked threads to process the next request.
	while(!q->finished) {
		q->cv.notify_one();
		this_thread::sleep_for(chrono::milliseconds(100));
	}
	//Prevent any new requests from being queued.
	q->cv_mutex.lock();
}

void WebappTask::execute() {
	while (!_handler->aborted) {
		_q->finished = 0;
		if(!_bg) {
			LuaParam _v[] = { { "sessions", _sessions },
							  { "requests", _q }, 
							  { "app", _handler } };
			_handler->runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_REQUEST);
		} else {
			LuaParam _v[] = { { "queue", _q }, 
							  { "app", _handler } };
			_handler->runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_QUEUE);
		}

		_q->finished = 1;
		//VM has quit.
		int cleaning = 0;
		{
			unique_lock<mutex> lk(_handler->cleanupLock); //Only allow one thread to clean up.
			if (_q->cleanupTask == 1) {
				cleaning = 1;
				//Cleanup worker; worker that recieved CleanCache request
				for (TaskBase* task: _handler->workers) {
					cleanup(task->_q);
				}
			}
		}
		
		if(cleaning) {
			//Delete loaded scripts
			for(int i = 0; i < WEBAPP_SCRIPTS; i++) {
				if(_handler->scripts[i].data != NULL) {
					delete[] _handler->scripts[i].data;
				}
			}
			
			_handler->compileScript("plugins/init.lua", SCRIPT_INIT);
			_handler->compileScript("plugins/core/process.lua", SCRIPT_REQUEST);
			_handler->compileScript("plugins/core/handlers.lua", SCRIPT_HANDLERS);
			if (_handler->background_queue_enabled)
				_handler->compileScript("plugins/core/process_queue.lua", SCRIPT_QUEUE);
			//lua vms finished; clean up.
			_handler->refresh_templates();
			for (TaskBase* task: _handler->workers) {
				task->_q->aborted = 0;
				task->_q->cv_mutex.unlock();
			}
			//Just (attempt to) grab own lock.
		} else {
			unique_lock<mutex> lk(_q->cv_mutex); 
		}
		//Unlock when done to complete the process.
	}
}

void Webapp::compileScript(const char* filename, script_t output) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);

	int ret = luaL_loadfile(L, "plugins/process.lua");
	if(ret) {
		printf("Error: %s\n", lua_tostring(L, -1));
	}
	lua_pushlstring(L, filename, strlen(filename));
	lua_setglobal(L, "file");
	if(lua_pcall(L, 0, 1, 0) != 0) {
		printf("Error: %s\n", lua_tostring(L, -1));
	}
	else {
		size_t* len = &scripts[output].len;
		const char* chunk = lua_tolstring(L, -1, len);
		scripts[output].data = new char[*len + 1];
		memcpy((char*)scripts[output].data, chunk, *len + 1);
	}
	lua_close(L);
}

//Run script given a LuaChunk. Called by public runScript methods.
void Webapp::runWorker(LuaParam* params, int nArgs, script_t script) {
	if(scripts[script].data == NULL) return;
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);
	webapp_str_t* static_strings = new webapp_str_t[WEBAPP_STATIC_STRINGS];
	Request* r = NULL;
	
	//Preload handlers.
	if(luaL_loadbuffer(L, scripts[SCRIPT_HANDLERS].data, scripts[SCRIPT_HANDLERS].len, "core/process"))
		goto lua_error;

	if(lua_pcall (L, 0, 1, 0)) 
		goto lua_error;
	
	// store funky module table in global var
	lua_setfield (L, LUA_GLOBALSINDEX, "load_handlers");
	
	//Create temporary webapp_str_t for string creation between vm/c
	lua_pushlightuserdata(L, static_strings);
	lua_setglobal(L, "static_strings");
	
	if(script == SCRIPT_INIT) {
		r = new Request();
		lua_pushlightuserdata(L, r);
		lua_setglobal(L, "tmp_request");
	}
	
	if(params != NULL) {
		for(int i = 0; i < nArgs; i++) {
			LuaParam* p = params + i;
			lua_pushlightuserdata(L, p->ptr);
			lua_setglobal(L, p->name);
		}
	}
	
	if(luaL_loadbuffer(L, scripts[script].data, scripts[script].len, "")) 
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

Webapp::Webapp(asio::io_service& io_svc) : 
										svc(io_svc), 
										cleanTemplate("")
									{

	//Run init plugin
	LuaParam _v[] = { { "app", this } };
	compileScript("plugins/init.lua", SCRIPT_INIT);
	compileScript("plugins/core/process.lua", SCRIPT_REQUEST);
	compileScript("plugins/core/handlers.lua", SCRIPT_HANDLERS);
	runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_INIT);

	refresh_templates();

	//Create/allocate initial worker tasks.
	if (background_queue_enabled) {
		compileScript("plugins/core/process_queue.lua", SCRIPT_QUEUE);
		workers.push_back(new WebappTask(this, NULL, new LockedQueue<Process*>(), 1));
		nWorkers--;
	}
	
	for (unsigned int i = 0; i < nWorkers; i++) {
		Sessions* session = new Sessions(i);
		LockedQueue<Request*>* requests = new LockedQueue<Request*>();
		sessions.push_back(session);
		request_queues.push_back(requests);
		workers.push_back(new WebappTask(this, session, requests));	
	}

	asio::io_service::work wrk = asio::io_service::work(svc);
	tcp::endpoint endpoint(tcp::v4(), port);
	acceptor = new tcp::acceptor(svc, endpoint, true);
	accept_message();
}

void Webapp::processRequest(Request* r, size_t amount) {
	r->v = new std::vector<char>(amount);
	asio::async_read(*r->socket, asio::buffer(*r->v), transfer_exactly(amount),
		[this, r](const asio::error_code& error, std::size_t bytes_transferred) {
			size_t read = 0;
			//Read each input chain variable recieved from nginx appropriately.
			for(int i = 0; i < STRING_VARS; i++) {
				if(r->input_chain[i]->data == NULL) {
					char* h = (char*) r->v->data() + read;
					r->input_chain[i]->data = h;
					h[r->input_chain[i]->len] = '\0';
					read += r->input_chain[i]->len + 1;
				}
			}
			//Choose a node, queue the request.
			int selected_node = -1;
			if (r->cookies.len > 0) {
				const char* sessionid = strstr(r->cookies.data, "sessionid=");
				int node = -1;
				if (sessionid && sscanf(sessionid + 10, "%" XSTR(WEBAPP_LEN_SESSIONID) "d", &node) == 1) {
					selected_node = node % nWorkers;
				}
			}
			if (selected_node == -1) {
				//Something went wrong - node not found, maybe session id missing. 
				selected_node = node_counter++ % nWorkers;
				
			}

			LockedQueue<Request*>* queue = request_queues.at(selected_node);
			queue->enqueue(r);
	});
}

void Webapp::accept_message() {
	ip::tcp::socket* s = new ip::tcp::socket(svc);
	acceptor->async_accept(*s, [this, s](const asio::error_code& error) {
		Request* r = new Request();
		r->socket = s;
		r->headers = new std::vector<char>(PROTOCOL_LENGTH_SIZEINFO);

		try {
			asio::async_read(*r->socket, asio::buffer(*r->headers), transfer_exactly(PROTOCOL_LENGTH_SIZEINFO),
				[this, r](const asio::error_code& error, std::size_t bytes_transferred) {
					if(r->uri.data == NULL && !r->method) {
						const char* headers = r->headers->data();
						//At this stage, at least PROTOCOL_SIZELENGTH_INFO has been read into the buffer.
						//STATE: Read protocol.
						r->uri.len = ntohs(*(int*)(headers));
						r->host.len = ntohs(*(int*)(headers + INT_INTERVAL(1)));
						r->user_agent.len = ntohs(*(int*)(headers + INT_INTERVAL(2)));
						r->cookies.len = ntohs(*(int*)(headers + INT_INTERVAL(3)));
						r->method = ntohs(*(int*)(headers + INT_INTERVAL(4)));
						r->request_body.len = ntohs(*(int*)(headers + INT_INTERVAL(5)));

						//Update the input chain.
						r->input_chain[0] = &r->uri;
						r->input_chain[1] = &r->host;
						r->input_chain[2] = &r->user_agent;
						r->input_chain[3] = &r->cookies;
						r->input_chain[4] = &r->request_body;

						size_t len = 0;
						for(int i = 0; i < STRING_VARS; i++) {
							len += r->input_chain[i]->len + 1;
						}
						processRequest(r, len);
					} 
				});
			accept_message();
		} catch(std::system_error er) {
			delete r;
			accept_message();
		}
	});
}

Webapp::~Webapp() {
	for (unsigned int i = 0; i < nWorkers; i++) {
		LockedQueue<Request*>* rq = request_queues.at(i);
		rq->aborted = 1;
		rq->cv.notify_one();
		delete rq;

		Sessions* session = sessions.at(i);
		delete session;
	}

	for (unsigned int i = 0; i < nWorkers; i++) {
		TaskBase* t = workers.at(i);
		if (t != NULL) {
			t->join();
			delete t;
		}
	}
	
	//Delete databases
	for(auto db_entry : databases) {
		auto db = db_entry.second;
		if (db != NULL) delete db;
	}

	//Delete loaded scripts
	for(int i = 0; i < WEBAPP_SCRIPTS; i++) {
		if(scripts[i].data != NULL) {
			delete[] scripts[i].data;
		}
	}
}

TemplateDictionary* Webapp::getTemplate(const std::string& page) {
	if(contains(contentList, page)) {
		return cleanTemplate.MakeCopy("base");
	}
	return NULL;
}

void Webapp::refresh_templates() {
	//Force reload templates
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);

	//Load content files (applicable templates)
	contentList.clear();

	vector<string> files = FileSystem::GetFiles("content/", "", 0);
	contentList.reserve(files.size());
	for(string& s : files) {
		//Preload templates.
		LoadTemplate("content/" + s, STRIP_WHITESPACE);
		contentList.push_back(s);
	}

	//Load server templates.
	files = FileSystem::GetFiles("templates/", "", 0);
	for(string& s: files) {
		if(!contains(serverTemplateList, s)) {
			string t = "T_" + s.substr(0, s.find_last_of("."));
			std::transform(t.begin()+2, t.end(), t.begin()+2, ::toupper);
			cleanTemplate.AddIncludeDictionary(t)->SetFilename("templates/" + s);
			serverTemplateList.push_back(s);
		}
	}
}

Database* Webapp::CreateDatabase() {
	size_t id = db_count++;
	Database* db = new Database(id);
	auto db_entry = make_pair(id, db);
	databases.insert(db_entry);
	return db;
}

Database* Webapp::GetDatabase(size_t index) {
	try {
		return databases.at(index);
	} catch (...) {
		return NULL;
	}
}

void Webapp::DestroyDatabase(Database* db) {
	unsigned int id = db->GetID();
	databases.erase(id);
	delete db;
}
