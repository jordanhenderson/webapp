#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "FileSystem.h"
#include "Parameters.h"
#include "Server.h"
#include "Database.h"
#include "rapidjson.h"
#include "Session.h"
#include "Serializer.h"
#include "document.h"
#include <ctemplate/template.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#define HTTP_NO_AUTH "Status: 401 Unauthorized\r\n\r\nUnauthorised access."
#define RESPONSE_TYPE_DATA 0
#define RESPONSE_TYPE_MESSAGE 1

#include "Schema.h"

#define GETCHK(s) s.empty() ? 0 : 1
#define RESPONSE_VARS RequestVars&, Response&, SessionStore&

typedef std::unordered_map<std::string, std::string> RequestVars;
typedef std::string Response;
typedef int(Gallery::*GallFunc)(RequestVars&, Response&, SessionStore&);
class Logging;

class LuaChunk {
public:
	std::string bytecode;
	std::string filename;
	~LuaChunk() {};
	LuaChunk(const std::string& filename) {
		this->filename = std::string(filename);
	};
};

//LuaParams are temporary containers to hold stack variables for lua scripts.
struct LuaParam {
	const char* p;
	void* d;
};

struct TemplateData {
	std::string name;
	FileData* data;
};

class Gallery : public ServerHandler, Internal {
private:
	Parameters* params;
	Database* database;
	Session session;	
	
	Response getPage(const std::string& page, SessionStore&, int publishSession);

	RequestVars parseRequestVariables(char* vars, RequestVars& v);
	Response processVars(RequestVars&, SessionStore&, int publishSession);
	std::string basepath;
	std::string dbpath;

	//Dynamic function map for public API.
	std::map<std::string, GallFunc> functionMap;

	//Process queue. Threads are executed one by one to ensure no write conflicts occur.
	std::queue<std::thread*> processQueue;
	std::thread::id currentID;
	std::thread* thread_process_queue;

	//Process queue 'add' mutex and condition variable.
	std::condition_variable cv_queue_add;
	std::mutex mutex_queue_add;

	//Process mutex used by the main thread queue. Held by both the process queue and individual processes.
	std::mutex mutex_thread_start;
	//Condition variable used by the main thread queue to signal a thread should begin.
	std::condition_variable cv_thread_start;

	std::string genCookie(const std::string& name, const std::string& value, time_t* date=NULL);
	std::string getCookieValue(const char* cookies, const char* key);
	

	int getData(Query& query, RequestVars&, Response&, SessionStore&);

	//template dictionary used by all 'content' templates.
	ctemplate::TemplateDictionary* contentTemplates;

	//(content) template filename vector
	std::vector<std::string> contentList;

	//Client Template filedata vector (stores templates for later de-allocation).
	std::vector<TemplateData> clientTemplateFiles;

	std::vector<std::string> serverTemplateFiles;

	//Store lua plugin bytecode in a vector.
	std::vector<LuaChunk*> loadedScripts;

	template <typename T>
	void addFiles(T& files, int nGenThumbs, const std::string& path, const std::string& albumID) {
		std::string date = date_format("%Y%m%d",8);
		std::string storepath = database->select(SELECT_SYSTEM("store_path"));
		std::string thumbspath = database->select(SELECT_SYSTEM("thumbs_path"));
		
		if(files.size() > 0) {
			for (typename T::const_iterator it = files.begin(), end = files.end(); it != end; ++it) {
				addFile(*it, nGenThumbs, thumbspath, path, date, albumID);
			}
		}	
	}
	
	void process_thread(std::thread*);
	void refresh_templates();
	void refresh_scripts();

//LUA specific calls (script engine)
	static int LuaWriter(lua_State* L, const void* p, size_t sz, void* ud);
	void runScript(const char* filename, LuaParam* params = NULL, int nArgs = 0);

//Gallery specific calls
	int genThumb(const char* file, double shortmax, double longmax);
	int getDuplicates( std::string& name, std::string& path );
	int hasAlbums();
	void addFile(const std::string&, int, const std::string&, const std::string&, const std::string&, const std::string&);

public:
	Gallery(Parameters* params);
	~Gallery();
	void process(FCGX_Request* request);

	//Main response functions.
	int getAlbums(RESPONSE_VARS);
	int addAlbum(RESPONSE_VARS);
	int addBulkAlbums(RESPONSE_VARS);
	int delAlbums(RESPONSE_VARS);
	int login(RESPONSE_VARS);
	int logout(RESPONSE_VARS);
	int setThumb(RESPONSE_VARS);
	int getFiles(RESPONSE_VARS);
	int search(RESPONSE_VARS);
	int refreshAlbums(RESPONSE_VARS);
	int disableFiles(RESPONSE_VARS);
	int clearCache(RESPONSE_VARS);
	int getBoth(RESPONSE_VARS);
	int updateAlbum(RESPONSE_VARS);
	int updateFile(RESPONSE_VARS);
};

#endif