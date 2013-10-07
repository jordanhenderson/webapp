
#include "Logging.h"
#include "Webapp.h"
#include "Image.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include <sha.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/task.h>

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace tbb;

task* WebappTask::execute() {
	_handler->numInstances++;
	_handler->createWorker();
	_handler->numInstances--;
	return NULL;	
	
}

void Webapp::createWorker() {
	LuaParam _v[] = {{"sessions", &sessions}, {"requests", &requests}, {"app", this}};
	refresh_scripts();
	runScript(SYSTEM_SCRIPT_PROCESS, _v, 3);
}

//Run scripts based on their index (system scripts; see Schema.h)
void Webapp::runScript(int index, LuaParam* params, int nArgs) {
	if(index < systemScripts.size()) {
		LuaChunk script = systemScripts.at(index);
		runScript(&script, params, nArgs);
	}
}

//Run script given a LuaChunk. Called by public runScript methods.
void Webapp::runScript(LuaChunk* c, LuaParam* params, int nArgs) {
	int bytecode_len = c->bytecode.length();
	if(bytecode_len > 0) {
		lua_State* L = luaL_newstate();
		luaL_openlibs(L);

		luaL_loadbuffer(L, c->bytecode.c_str(), bytecode_len, c->filename.c_str());
		if(params != NULL) {
			for(int i = 0; i < nArgs; i++) {
				LuaParam* p = params + i;
				lua_pushlightuserdata(L, p->ptr);
				lua_setglobal(L, p->name);
			}
		}
		if(lua_pcall(L, 0, 0, 0) != 0) {
			printf("Error: %s\n", lua_tostring(L, -1));
		}
		lua_close(L);
	}
}

Webapp::Webapp(Parameters* params, asio::io_service& io_svc) :  contentTemplates(""),
										params(params),
										basepath(params->get("basepath")),
										dbpath(params->get("dbpath")), 
										svc(io_svc) {

	database.connect(DATABASE_TYPE_SQLITE, dbpath);
	//Enable pragma foreign keys.
	database.exec(PRAGMA_FOREIGN);

	//Create the schema. Running this even when tables exist prevent issues later on.
	string schema = CREATE_DATABASE;
	vector<string> schema_queries;
	tokenize(schema, schema_queries, ";");
	for(string s : schema_queries) {
		database.exec(s);
	}
	
	refresh_templates();

	refresh_scripts();

	//Create function map.
	APIMAP(functionMap);

	queue_process_thread = std::thread([this]() {
		while(!shutdown_handler) {
			std::thread* nextThread;
			processQueue.pop(nextThread);
			currentID = nextThread->get_id();
			cv_thread_start.notify_all();
		}
	});

	queue_process_thread.detach();


	asio::io_service::work wrk = asio::io_service::work(svc);
	tcp::endpoint endpoint(tcp::v4(), 5000);
	acceptor = new tcp::acceptor(svc, endpoint, true);
	accept_message();
	svc.run();
}

//This function asynchronously reads chunks of data from the socket.
//Uses a basic state machine.
void Webapp::read_some(ip::tcp::socket* s, Request* r) {
	s->async_read_some(asio::buffer(*r->v),
		[this, s, r](const asio::error_code& error, std::size_t bytes_transferred) {
			std::vector<char>* v = r->v;
			r->headers = v->data();
			//We use this as a state machine to repeatedly read in and process the request (non-blocking).
			if(r->uri.data == NULL && r->length + bytes_transferred >= PROTOCOL_LENGTH_URI && r->state == 0) {
				//STATE: Read URI LEN
				r->method = ntohs(*(int*)r->headers);
				r->uri.len = ntohs(*(int*)(r->headers + PROTOCOL_LENGTH_METHOD));
				r->state = STATE_READ_URI; //move to next state, reading the URI.
			} else if (r->state == STATE_READ_URI) {
				//State: Read URI
				r->uri.data = (const char*) malloc(r->uri.len + 1);
				cmemcpy(r->uri.data, r->headers + PROTOCOL_LENGTH_URI, r->uri.len);
				r->state = STATE_READ_COOKIES;
			} else if(r->state == STATE_READ_COOKIES) {

			} else if (r->state == STATE_FINAL) {
				//Finished reading data. Create lua handler.
				if(shutdown_handler) return;
				if(numInstances < tbb::task_scheduler_init::default_num_threads()) {	
					WebappTask* task = new (task::allocate_additional_child_of(*parent_task)) 
						WebappTask(this);
					parent_task->enqueue(*task);
				} 

				requests.push(r);
				//State machine finished.
				return;
			} else if(r->state != STATE_FINAL) {
				//Something has gone wrong. State not entered.
				exit(1);
			}
			r->length += bytes_transferred;
			//Final statement, we haven't finished reading data. Try again.
			if(r->state != STATE_FINAL) read_some(s, r);

	});
}

void Webapp::accept_message() {
	ip::tcp::socket* s = new ip::tcp::socket(svc);
	acceptor->async_accept(*s, [this, s](const asio::error_code& error) {
		

		Request* r = new Request();
		r->v = new std::vector<char>(512);
		try {
			read_some(s, r);
			accept_message();
		} catch(std::system_error er) {
			accept_message();
			s->close();
			delete s;
		}
	});
}

//PRODUCER
void Webapp::process_thread(std::thread* t) {
	processQueue.push(t);
}

Webapp::~Webapp() {
	//Clean up client template files.
	for(TemplateData file: clientTemplateFiles) {
		delete(file.data);
	}

	shutdown_handler = 1;
	processQueue.abort();
}

TemplateDictionary* Webapp::getTemplate(const char* page) {
	if(contains(contentList, page)) {
		TemplateDictionary *d = contentTemplates.MakeCopy("");
		for(string data: serverTemplateFiles) {
			d->AddIncludeDictionary(data)->SetFilename(data);
		}
		for(TemplateData file: clientTemplateFiles) {
			d->SetValueWithoutCopy(file.name, TemplateString(file.data->data, file.data->size));
		}
		return d;
	}
	return NULL;
}

void Webapp::refresh_templates() {
	//Force reload templates
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);
	//Load content files (applicable templates)
	contentList.clear();
	string basepath = this->basepath + '/';
	string templatepath = basepath + "content/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string s : files) {
			//Preload templates.
			LoadTemplate(templatepath + s, STRIP_WHITESPACE);
			contentList.push_back(s);
		}
	}

	//Load server templates.
	templatepath = basepath + "templates/server/";
	serverTemplateFiles.clear();

	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		serverTemplateFiles.reserve(files.size());
		for(string s: files) {
			File f;
			FileData data;
			FileSystem::Open(templatepath + s, "rb", &f);
			string template_name = "T_" + s.substr(0, s.find_last_of("."));
			FileSystem::Read(&f, &data);
			std::transform(template_name.begin(), template_name.end(), template_name.begin(), ::toupper);
			mutable_default_template_cache()->Delete(template_name);
			StringToTemplateCache(template_name, data.data, data.size, STRIP_WHITESPACE);
			serverTemplateFiles.push_back(template_name);
		}
	}
	
	//Load client templates (inline insertion into content templates) - these are Handbrake (JS) templates.
	//First delete existing filedata. This should be optimised later.
	for(TemplateData file: clientTemplateFiles) {
		delete file.data;
	}
	clientTemplateFiles.clear();

	templatepath = basepath + "templates/client/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string s: FileSystem::GetFiles(templatepath, "", 0)) {
			File f;
			FileData* data = new FileData();
			FileSystem::Open(templatepath + s, "rb", &f);
			string template_name = "T_" + s.substr(0, s.find_last_of("."));
			FileSystem::Read(&f, data);
			std::transform(template_name.begin(), template_name.end(), template_name.begin(), ::toupper);
			TemplateData d = {template_name, data};
			clientTemplateFiles.push_back(d);
		}
	}
}

//Refresh the LUA plugins
void Webapp::refresh_scripts() {
	loadedScripts.clear();
	systemScripts.clear();

	string basepath = this->basepath + '/';
	string pluginpath = basepath + "plugins/";
	const char* system_script_names[] = SYSTEM_SCRIPT_FILENAMES;

	int system_script_count = sizeof(system_script_names) / sizeof(char*);
	for(int i = 0; i < system_script_count; i++) {
		LuaChunk empty(system_script_names[i]);
		systemScripts.push_back(empty);
	}

	for(string s: FileSystem::GetFiles(pluginpath, "", 1)) {
		LuaChunk b(s);
		lua_State* L = luaL_newstate();
		if(L != NULL) {
			luaL_openlibs(L);

#ifdef _DEBUG
			if(luaL_loadfile(L, (pluginpath + s).c_str())) {
				printf ("%s\n", lua_tostring (L, -1));
				lua_pop (L, 1);
			}
#else
			luaL_loadfile(L, (pluginpath + s).c_str());
#endif

#ifdef _DEBUG
			if(lua_dump(L, LuaWriter, &b, 0)) {
				printf ("%s\n", lua_tostring (L, -1));
				lua_pop (L, 1);
			}
#else
			lua_dump(L, LuaWriter, &b, 1);
#endif
			lua_close(L);
			//Bytecode loaded. Update empty entries.
			loadedScripts.push_back(b);
			for(int i = 0; i < system_script_count; i++) {
				//Map system scripts
				if(s == system_script_names[i]) systemScripts.at(i) = b;
			}
		}
	}
}

int Webapp::LuaWriter(lua_State* L, const void* p, size_t sz, void* ud) {
	LuaChunk* b = (LuaChunk*)ud;
	b->bytecode.append((const char*)p, sz);
	return 0;
}
