
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
	runHandler(_v, sizeof(_v) / sizeof(LuaParam));
}

//Run script given a LuaChunk. Called by public runScript methods.
void Webapp::runHandler(LuaParam* params, int nArgs) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	luaL_loadfile(L, "plugins/core/process.lua"); //only loaded once per handler. 
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

Webapp::Webapp(Parameters* params, asio::io_service& io_svc) :  contentTemplates(""),
										params(params),
										basepath(params->get("basepath")),
										dbpath(params->get("dbpath")), 
										svc(io_svc),
										tasks(tbb::task_scheduler_init::default_num_threads()) {

	database.connect(DATABASE_TYPE_SQLITE, dbpath);
	//Enable pragma foreign keys.
	database.exec(PRAGMA_FOREIGN);

	//Create the schema. Running this even when tables exist prevent issues later on.
	string schema = CREATE_DATABASE;
	vector<string> schema_queries;
	tokenize(schema, schema_queries, ";");
	for(string& s : schema_queries) {
		database.exec(s);
	}
	
	refresh_templates();

	//Create/allocate initial worker tasks.
	for (unsigned int i = 0; i < tasks.size(); i++) {
		tasks.at(i) = new (task::allocate_additional_child_of(*parent_task)) WebappTask(this);
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
			 
			if (numInstances + 1 < tasks.size()) {
				parent_task->enqueue(*tasks.at(numInstances));
			} 

			requests.push(r);
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
			s->close();
			delete s;
		}
	});
}


Webapp::~Webapp() {
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
	string basepath = this->basepath + '/';
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
