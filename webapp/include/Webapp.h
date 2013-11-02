#ifndef GALLERY_H
#define GALLERY_H

#include <asio.hpp>
#include "Platform.h"
#include "CPlatform.h"
#include "FileSystem.h"
#include "Parameters.h"
#include "Server.h"
#include "Database.h"
#include "Session.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>
#include <tbb/compat/thread>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}


#include "Schema.h"

#define GETCHK(s) s.empty() ? 0 : 1

class Logging;
class Webapp;
//LuaParams are temporary containers to hold stack variables for lua scripts.
struct LuaParam {
	const char* name;
	void* ptr;
};

struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};

struct TemplateData {
	std::string name;
	FileData* data;
};

class TaskBase : public tbb::tbb_thread {
public:
	TaskBase(Webapp* handler) : _handler(handler), tbb::tbb_thread(start_thread, this) {};
	
	virtual void execute() = 0;
	static void start_thread(TaskBase* base) { base->execute(); };
protected:
	Webapp* _handler = NULL;
};

class WebappTask : public TaskBase {
public:
	WebappTask(Webapp* handler) : TaskBase(handler){};
	void execute();
	friend class Webapp;
};

class BackgroundQueue : public TaskBase {
public:
	BackgroundQueue(Webapp* handler) : TaskBase(handler) {};
	void execute();
	friend class Webapp;
};

class CleanupTask : public tbb::task {
	RequestQueue* _requests = NULL;
	Webapp* _handler = NULL;
public:
	CleanupTask(Webapp* handler, RequestQueue* requests) : _handler(handler), _requests(requests) {};
	tbb::task* execute();
};

#define WEBAPP_PARAM_BASEPATH 0
#define WEBAPP_PARAM_DBPATH 1
#define WEBAPP_STATIC_STRINGS 2

class Webapp : public ServerHandler, Internal {
private:
	//template dictionary used by all 'content' templates.
	ctemplate::TemplateDictionary contentTemplates;

	//(content) template filename vector
	std::vector<std::string> contentList;

	//Client Template filedata vector (stores templates for later de-allocation).
	std::vector<TemplateData> clientTemplateFiles;
	std::vector<std::string> serverTemplateFiles;

	void process_thread(std::thread*);
	
	static int LuaWriter(lua_State* L, const void* p, size_t sz, void* ud);
	void runHandler(LuaParam* params, int nArgs, const char* filename);
	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();

	asio::io_service& svc;
	void processRequest(Request* r, int len);

	friend class WebappTask;
	friend class BackgroundQueue;
public:
	CleanupTask* posttask = NULL;
	std::vector<TaskBase*> workers;
	Webapp(Parameters* params, asio::io_service& io_svc);
	~Webapp();
	ctemplate::TemplateDictionary* getTemplate(const char* page);
	void refresh_templates();
	
	const std::string* basepath;
	const std::string* dbpath;

	Parameters* params;
	Database database;
	Sessions sessions;

	tbb::concurrent_bounded_queue<Process*> background_queue;
};

#endif