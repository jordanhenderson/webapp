#include "Webapp.h"
#include "Image.h"

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;

#ifdef _WIN32
#pragma warning(disable:4316)
#endif 

void WebappTask::execute() {
	while (!_handler->aborted) {
		if(!_bg) {
			LuaParam _v[] = { { "sessions", _sessions },
							  { "requests", _requests }, 
							  { "app", _handler }, 
							  { "db", &_handler->database } };
			_handler->runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_REQUEST);
		} else {
			LuaParam _v[] = { { "queue", _requests }, 
							  { "app", _handler }, 
							  { "db", &_handler->database } };
			_handler->runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_QUEUE);
		}

		//VM has quit.
		_handler->waiting++;
		int cleaning = 0;
		{
			unique_lock<mutex> lk(_handler->cleanupLock); //Only allow one thread to clean up.
			if (_requests->cleanupTask == 1) {
				cleaning = 1;
				//Cleanup worker; worker that recieved CleanCache request
				for (TaskQueue* q : _handler->requests) {
					q->cleanupTask = 0; //Only clean up once!
					//Lock the queue, prevent it from finishing until cleaner does
					//Abort the lua vm.
					q->aborted = 1;
					//Lock the queue after aborting; if processRequests() locks, then it will abort
					//shortly after.
					q->lock.lock();
					q->cv.notify_one();
				}
			}
		}
		
		if(cleaning) {
			//Wait for lua vms to finish.
			while (_handler->waiting != _handler->workers.size()) {
				this_thread::sleep_for(chrono::milliseconds(100));
			}

			//Delete loaded scripts
			for(int i = 0; i < WEBAPP_SCRIPTS; i++) {
				if(_handler->scripts[i].data != NULL) {
					delete[] _handler->scripts[i].data;
				}
			}
			
			_handler->compileScript("plugins/init.lua", SCRIPT_INIT);
			if (_handler->background_queue_enabled)
				_handler->compileScript("plugins/core/process_queue.lua", SCRIPT_QUEUE);
			
			_handler->compileScript("plugins/core/process.lua", SCRIPT_REQUEST);
			_handler->compileScript("plugins/core/handlers.lua", SCRIPT_HANDLERS);
			
			//lua vms finished; clean up.
			_handler->refresh_templates();
			for (TaskQueue* q : _handler->requests) {
				q->aborted = 0;
				q->lock.unlock();
			}
		} else {
			//Just (attempt to) grab own lock.
			unique_lock<mutex> lk(_requests->lock); //Unlock when done to complete the process.
			
		}
		
		_handler->waiting--;
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
	
	if(script == SCRIPT_REQUEST) {
		//Preload handlers.
		if(luaL_loadbuffer(L, scripts[SCRIPT_HANDLERS].data, scripts[SCRIPT_HANDLERS].len, "core/process"))
			goto lua_error;

		if(lua_pcall (L, 0, 1, 0)) 
			goto lua_error;
		
		// store funky module table in global var
		lua_setfield (L, LUA_GLOBALSINDEX, "load_handlers");
	}
		
	
	//Create temporary webapp_str_t for string creation between vm/c
	lua_pushlightuserdata(L, static_strings);
	lua_setglobal(L, "static_strings");

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
	lua_close(L);
}

Webapp::Webapp(asio::io_service& io_svc) : 
										svc(io_svc), 
                                        nWorkers(WEBAPP_NUM_THREADS - 1),
                                        cleanTemplate("")
									{

	//Run init plugin
	
	LuaParam _v[] = { { "app", this }, { "db", &database } };
	compileScript("plugins/init.lua", SCRIPT_INIT);
	runWorker(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_INIT);

    refresh_templates();

	//Create/allocate initial worker tasks.
	if (background_queue_enabled) {
		compileScript("plugins/core/process_queue.lua", SCRIPT_QUEUE);
		LockedQueue<Process*>* background_queue = new LockedQueue<Process*>();
		workers.push_back(new WebappTask(this, NULL, background_queue, 1));
	}
	
	compileScript("plugins/core/process.lua", SCRIPT_REQUEST);
	compileScript("plugins/core/handlers.lua", SCRIPT_HANDLERS);
	for (unsigned int i = background_queue_enabled; i < nWorkers; i++) {
		unsigned int worker_id = background_queue_enabled ? i - 1 : i;
		Sessions* session = new Sessions(worker_id);
		RequestQueue* request = new RequestQueue();
		sessions.push_back(session);
		requests.push_back(request);
		workers.push_back(new WebappTask(this, session, request));	
	}

	asio::io_service::work wrk = asio::io_service::work(svc);
	tcp::endpoint endpoint(tcp::v4(), 5000);
	acceptor = new tcp::acceptor(svc, endpoint, true);
	accept_message();
	svc.run(); //block the main thread.
}

void Webapp::processRequest(Request* r, int amount) {
	r->v = new std::vector<char>(amount);
	asio::async_read(*r->socket, asio::buffer(*r->v), transfer_exactly(amount),
		[this, r](const asio::error_code& error, std::size_t bytes_transferred) {
			int read = 0;
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

			RequestQueue* queue = requests.at(selected_node);
			queue->lock.lock();
			queue->requests.enqueue(r);
			queue->cv.notify_one();
			queue->lock.unlock();
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

						int len = 0;
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
		RequestQueue* rq = requests.at(i);
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
    files = FileSystem::GetFiles("templates/server/", "", 0);
    for(string& s: files) {
        if(!contains(serverTemplateList, s)) {
            string t = "T_" + s.substr(0, s.find_last_of("."));
            std::transform(t.begin()+2, t.end(), t.begin()+2, ::toupper);
            cleanTemplate.AddIncludeDictionary(t)->SetFilename("templates/server/" + s);
            serverTemplateList.push_back(s);
        }
    }
}
