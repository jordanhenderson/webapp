
#include "Logging.h"
#include "Webapp.h"
#include "Image.h"
#include <sha.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/task.h>

using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;
using namespace tbb;

void WebappTask::execute() {
	LuaParam _v[] = { { "sessions", &_handler->sessions }, { "requests", &_handler->requests }, { "app", _handler }, { "db", &_handler->database } };
	_handler->runHandler(_v, sizeof(_v) / sizeof(LuaParam), "plugins/core/process.lua");

	//VM has quit, problem!
	if (_handler->posttask != NULL) {
		tbb::task::enqueue(*_handler->posttask);
		_handler->posttask = NULL;
		
	}
	_handler->waiting++;
	_handler->requests.lock.lock();
	_handler->requests.lock.unlock();
	_handler->waiting--;
	execute();
}

void BackgroundQueue::execute() {
	LuaParam _v[] = { { "sessions", &_handler->sessions }, { "requests", &_handler->requests }, { "app", _handler }, { "db", &_handler->database } };
	_handler->runHandler(_v, sizeof(_v) / sizeof(LuaParam), "plugins/core/process_queue.lua");
	//Since this task must run at all times (not per request - *should* block in lua vm)
	Sleep(1000); //Lua VM returned, something went wrong.
	execute();
}

tbb::task* CleanupTask::execute() {
	if (_handler->waiting != _handler->workers.size() - 1) {
		recycle_as_continuation();
		return this;
	}
	else {
		_requests->requests.clear();
		_handler->refresh_templates();
		_requests->lock.unlock();
		_requests->aborted = 0;
		return NULL;
	}
}

//Run script given a LuaChunk. Called by public runScript methods.
void Webapp::runHandler(LuaParam* params, int nArgs, const char* filename) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_lpeg(L);
	luaopen_cjson(L);
	//Preprocess the lua file first.

	luaL_loadfile(L, "plugins/process.lua");
	lua_pushlstring(L, filename, strlen(filename));
	lua_setglobal(L, "file");
	webapp_str_t static_strings[WEBAPP_STATIC_STRINGS] = {};
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

	if(lua_pcall(L, 0, 0, 0) != 0) {
		logger->printf("Error: %s", lua_tostring(L, -1));
	}
	lua_close(L);
}

Webapp::Webapp(Parameters* params, asio::io_service& io_svc) :  contentTemplates(""),
										params(params),
										basepath(&params->get("basepath")),
										dbpath(&params->get("dbpath")), 
										svc(io_svc), 
										workers(tbb::task_scheduler_init::default_num_threads() > 2 ?
												tbb::task_scheduler_init::default_num_threads() - 2: 2) {

	//Run init plugin
	LuaParam _v[] = { { "app", this }, { "db", &database } };
	runHandler(_v, sizeof(_v) / sizeof(LuaParam), "plugins/init.lua");

	refresh_templates();

	//Create/allocate initial worker tasks.
	workers.at(0) = new BackgroundQueue(this);

	for (unsigned int i = 1; i < workers.size(); i++) {
		workers.at(i) = new WebappTask(this);
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
			for(int i = 0; i < STRING_VARS; i++) {
				if(r->input_chain[i]->data == NULL) {
					char* h = (char*) r->v->data() + read;
					r->input_chain[i]->data = h;
					h[r->input_chain[i]->len] = '\0';
					read += r->input_chain[i]->len + 1;
				}
			}

			requests.lock.lock();
			requests.requests.push(r);
			requests.lock.unlock();
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
	for (unsigned int i = 0; i < workers.size(); i++) {
		TaskBase* t = workers.at(i);
		if (t != NULL) {
			t->join();
			delete t;
		}
	}
	//Clean up client template files.
	for(TemplateData& file: clientTemplateFiles) {
		delete(file.data);
	}

}

TemplateDictionary* Webapp::getTemplate(const char* page) {
	if(contains(contentList, page)) {
		TemplateDictionary *d = contentTemplates.MakeCopy("");
		for(string& data: serverTemplateFiles) {
			d->AddIncludeDictionary(data)->SetFilename(data);
		}
		for(TemplateData& file: clientTemplateFiles) {
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
	string basepath = *this->basepath + '/';
	string templatepath = basepath + "content/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string& s : files) {
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
		for(string& s: files) {
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
	for(TemplateData& file: clientTemplateFiles) {
		delete file.data;
	}
	clientTemplateFiles.clear();

	templatepath = basepath + "templates/client/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string& s: FileSystem::GetFiles(templatepath, "", 0)) {
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
