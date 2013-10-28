#ifndef GALLERY_H
#define GALLERY_H

#include <asio.hpp>
#include "Platform.h"
#include "Helpers.h"
#include "FileSystem.h"
#include "Parameters.h"
#include "Server.h"
#include "Database.h"
#include "rapidjson.h"
#include "Session.h"
#include "Serializer.h"
#include "document.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
}


#include "Schema.h"

#define GETCHK(s) s.empty() ? 0 : 1

class Logging;
//LuaParams are temporary containers to hold stack variables for lua scripts.
struct LuaParam {
	const char* name;
	void* ptr;
};

struct TemplateData {
	std::string name;
	FileData* data;
};

class TaskBase : public tbb::task {
public:
	TaskBase(Webapp* handler) : _handler(handler) {};
	virtual tbb::task* execute() = 0;
protected:
	Webapp* _handler = NULL;
};

class WebappTask : public TaskBase {
	WebappTask(Webapp* handler) : TaskBase(handler){};
	tbb::task* execute();
	friend class Webapp;
};

class BackgroundQueue : public TaskBase {
	BackgroundQueue(Webapp* handler) : TaskBase(handler) {};
	tbb::task* execute();
	friend class Webapp;
};

class Webapp : public ServerHandler, Internal {
private:
	Database database;
	Sessions sessions;	

	//template dictionary used by all 'content' templates.
	ctemplate::TemplateDictionary contentTemplates;

	//(content) template filename vector
	std::vector<std::string> contentList;

	//Client Template filedata vector (stores templates for later de-allocation).
	std::vector<TemplateData> clientTemplateFiles;
	std::vector<std::string> serverTemplateFiles;

	void process_thread(std::thread*);
	void refresh_templates();
	
	static int LuaWriter(lua_State* L, const void* p, size_t sz, void* ud);
	void runHandler(LuaParam* params, int nArgs, const char* filename);
	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();

	asio::io_service& svc;
	void processRequest(Request* r, int len);

	std::vector<TaskBase*> tasks;
	friend class WebappTask;
	friend class BackgroundQueue;
public:
	Webapp(Parameters* params, asio::io_service& io_svc);
	~Webapp();
	ctemplate::TemplateDictionary* getTemplate(const char* page);
	
	std::string basepath;
	std::string dbpath;

	Parameters* params;
};

#endif