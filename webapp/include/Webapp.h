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
}

#define HTTP_NO_AUTH "Status: 401 Unauthorized\r\n\r\nUnauthorised access."
#define RESPONSE_TYPE_DATA 0
#define RESPONSE_TYPE_MESSAGE 1
#define HTTP_METHOD_GET 2
#define HTTP_METHOD_POST 8

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

class WebappTask : public tbb::task {
public: 
	WebappTask(ServerHandler* handler)
		: _handler(handler)
	{};
	tbb::task* execute();
private:
	ServerHandler* _handler;
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
	void runHandler(LuaParam* params, int nArgs);
	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();

	asio::io_service& svc;
	void processRequest(Request* r, int len);

	std::vector<WebappTask*> tasks;

public:
	Webapp(Parameters* params, asio::io_service& io_svc);
	~Webapp();
	ctemplate::TemplateDictionary* getTemplate(const char* page);

	void createWorker();

	std::string basepath;
	std::string dbpath;

	Parameters* params;
};

#endif