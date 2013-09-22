#include "Logging.h"
#include "Gallery.h"
#include "Image.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include <sha.h>
#include <tbb/task_scheduler_init.h>

using namespace std;
using namespace ctemplate;

void Gallery::runScript(const char* filename, LuaParam* params, int nArgs) {
	for(LuaChunk c: loadedScripts) {
		if(c.filename == filename) {
			runScript(&c, params, nArgs);
		}
	}
}

//Run scripts based on their index (system scripts; see Schema.h)
void Gallery::runScript(int index, LuaParam* params, int nArgs) {
	if(index < systemScripts.size()) {
		LuaChunk script = systemScripts.at(index);
		runScript(&script, params, nArgs);
	}
}

//Run script given a LuaChunk. Called by public runScript methods.
void Gallery::runScript(LuaChunk* c, LuaParam* params, int nArgs) {
	int bytecode_len = c->bytecode.length();
	if(bytecode_len > 0) {
		lua_State* L = luaL_newstate();
		luaL_openlibs(L);
		
		luaL_loadbuffer(L, c->bytecode.c_str(), bytecode_len, c->filename.c_str());
		if(params != NULL) {
			for(int i = 0; i < nArgs; i++) {
				LuaParam* p = params + i;
				lua_pushlightuserdata(L, p->d);
				lua_setglobal(L, p->p);
			}
		}
		if(lua_pcall(L, 0, 0, 0) != 0) {
			printf("Error: %s\n", lua_tostring(L, -1));
		}
		lua_close(L);
	}
}

Gallery::Gallery(Parameters* params) :  contentTemplates(""),
										params(params),
										basepath(params->get("basepath")),
										dbpath(params->get("dbpath")) {

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

	//Create function map.
	APIMAP(functionMap);

	systemScripts.reserve(SYSTEM_SCRIPT_COUNT);

	refresh_scripts();

	queue_process_thread = thread([this]() {
		while(!shutdown_handler) {
			thread* nextThread;
			processQueue.pop(nextThread);
			currentID = nextThread->get_id();
			cv_thread_start.notify_all();
		}
	});

	queue_process_thread.detach();
}

//PRODUCER
void Gallery::process_thread(std::thread* t) {
	processQueue.push(t);
}

Gallery::~Gallery() {
	//Clean up client template files.
	for(TemplateData file: clientTemplateFiles) {
		delete(file.data);
	}

	shutdown_handler = 1;
	processQueue.abort();
}



Response Gallery::getPage(const string& uri, SessionStore& session, int publishSession) {
	/*
	Response r;
	string page;
	if(publishSession) {
		time_t t; time(&t); add_days(t, 1);
		r = GenCookie("sessionid", session.sessionid, &t);
	}

	if(uri[0] == '/' && (uri[1] == '\0' || uri[1] == '?')) {
		page = "index";
	} else {
		std::size_t found = uri.find_first_of("?&#");
		page = uri.substr(std::min(1, (int)uri.size()), found-1);
	}

	//Template parsing
	if(contains(contentList, page + ".html")) {
		r = HTML_HEADER + r + END_HEADER;
		string output;
		TemplateDictionary* d = contentTemplates.MakeCopy("");
		for(string data: serverTemplateFiles) {
			d->AddIncludeDictionary(data)->SetFilename(data);
		}
		for(TemplateData file: clientTemplateFiles) {
			d->SetValueWithoutCopy(file.name, TemplateString(file.data->data, file.data->size));
		}
		LuaParam luaparams[] = {{"template", d}, {"session", &session}};

		runScript(SYSTEM_SCRIPT_TEMPLATE, (LuaParam*)luaparams, 2);
		ExpandTemplate(basepath + "/content/" + page + ".html", STRIP_WHITESPACE, d, &output);
		r.append(output);
		delete d;
		
	} else {
		//File doesn't exists. 404.
		r.append(END_HEADER);
		r.append(HTML_404);
	}
	return r;
	*/
	return "";
}

void Gallery::createWorker() {
	//Create a LUA worker on the current thread/task.
	LuaParam luaparams[] = {{"requests", &requests}, {"sessions", &sessions}};
	//This call will not return until the lua worker is finished.
	runScript(SYSTEM_SCRIPT_PROCESS, (LuaParam*)luaparams, 2);

	/*
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
	char* cookie_str = FCGX_GetParam("HTTP_COOKIE", request->envp);
	//Get/set the session id.
	
	string sessionid = getCookieValue(cookie_str, "sessionid");
	SessionStore* store = NULL;
	int createstore = 0;
	if(!sessionid.empty()) {
		store = session.get_session(sessionid);
	}
	//Create a new session.
	if(store == NULL) {
		char* host = FCGX_GetParam("HTTP_HOST", request->envp);
		char* user_agent = FCGX_GetParam("HTTP_USER_AGENT", request->envp);
		store = session.new_session(host, user_agent);
		createstore = 1;
	}

	string final;
	if(strcmp(method, "GET") == 0) {
		if(strstr(uri, "/api") == uri) {
			//Create an unordered map containing ?key=var pairs.
			RequestVars v;
			parseRequestVars(uri + 4, v);
			final = processVars(v, *store, createstore);
		} else {
			final = getPage(uri, *store, createstore);
		}
	} else if(strcmp(method, "POST") == 0) {
		//Read data from the input stream. (allocated using CONTENT_LENGTH)
		char* strlength = FCGX_GetParam("HTTP_CONTENT_LENGTH", request->envp);
		if(strlength == NULL)
			return;
		int len = atoi(strlength);
		char* postdata = new char[len + 1];
		FCGX_GetStr(postdata, len, request->in);
		//End the string.
		postdata[len] = '\0';

		RequestVars v;
		parseRequestVars(postdata, v);

		final = processVars(v, *store, createstore);
		delete[] postdata;
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
	*/
}


Response Gallery::processVars(const char* uri, SessionStore& session, int publishSession) {
	/*
	RequestVars vars;
	//parseRequestVars(uri, vars);
	string t = vars["t"];

	Response r = JSON_HEADER;
	if(publishSession) {
		time_t t; time(&t); add_days(t, 1);
		r.append(GenCookie("sessionid", session.sessionid, &t));
	}

	unordered_map<string, GallFunc>::const_iterator miter = functionMap.find(t);
	int hasfunc = miter != functionMap.end();
	if((hasfunc && (params->get("user").empty() || params->get("pass").empty())) || 
		(hasfunc && !session.get("auth").empty()) || (hasfunc && t == "login")) {
		GallFunc f = miter->second;

		r.append(END_HEADER);
		(this->*f)(vars, r, session);
		return r;
	} else {
		Serializer s;
		
		r.append(END_HEADER);
		if(hasfunc) {
			s.append("msg", "NO_AUTH");
			r.append(s.get(RESPONSE_TYPE_MESSAGE));
		} else {
			r.append("{}");
		}
		return r;
	}
	*/
	return "";
}

int Gallery::genThumb(const char* file, double shortmax, double longmax) {
	string storepath = database.select_single(SELECT_SYSTEM("store_path"));
	string thumbspath = database.select_single(SELECT_SYSTEM("thumbs_path"));
	string imagepath = basepath + '/' + storepath + '/' + file;
	string thumbpath = basepath + '/' + thumbspath + '/' + file;
	
	//Check if thumb already exists.
	if(FileSystem::Open(thumbpath))
		return ERROR_SUCCESS;

	Image image(imagepath);
	int err = image.GetLastError();
	if(image.GetLastError() != ERROR_SUCCESS){
		return err;
	}
	//Calculate correct size (keeping aspect ratio) to shrink image to.
	double wRatio = 1;
	double hRatio = 1;
	double width = image.getWidth();
	double height = image.getHeight();
	if(width >= height) {
		if(width > longmax || height > shortmax) {
			wRatio = longmax / width;
			hRatio = shortmax / height;
		}
	}
	else {
		if(height > longmax || width > shortmax) {
			wRatio = shortmax / width;
			hRatio = longmax / height;
		}
	}

	double ratio = min(wRatio, hRatio);
	double newWidth = width * ratio;
	double newHeight = height * ratio;


    image.resize(newWidth, newHeight);
	image.save(thumbpath);

	if(!FileSystem::Open(thumbpath)) {
		logger->printf("An error occured generating %s.", thumbpath.c_str());
	}

	return ERROR_SUCCESS;
}

int Gallery::hasAlbums() {
	string albumsStr = database.select_single(SELECT_ALBUM_COUNT, NULL, "0");
	int nAlbums = stoi(albumsStr);
	if(nAlbums == 0) {
		return 0;
	}
	return 1;
}

void Gallery::refresh_templates() {
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
void Gallery::refresh_scripts() {
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
			luaL_loadfile(L, (pluginpath + s).c_str())
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

int Gallery::LuaWriter(lua_State* L, const void* p, size_t sz, void* ud) {
	LuaChunk* b = (LuaChunk*)ud;
	b->bytecode.append((const char*)p, sz);
	return 0;
}

void Gallery::addFile(const string& file, int nGenThumbs, const string& thumbspath, const string& path, const string& date, const string& albumID) {
	string thumbID;
	//Generate thumb.
	if(nGenThumbs) {
		FileSystem::MakePath(basepath + '/' + thumbspath + '/' + path + '/' + file);
		if(genThumb((path + '/' + file).c_str(), 200, 200) == ERROR_SUCCESS) {
			QueryRow params;
			params.push_back(path + '/' + file);
			int nThumbID = database.exec(INSERT_THUMB, &params);
			if(nThumbID > 0) {
				thumbID = to_string(nThumbID);
			}
		}
	}

	//Insert file 
	QueryRow params;
	params.push_back(file);
	params.push_back(file);
	params.push_back(date);
	int fileID;
	if(!thumbID.empty()) {
		params.push_back(thumbID);
		fileID = database.exec(INSERT_FILE, &params);
	} else {
		fileID = database.exec(INSERT_FILE_NO_THUMB, &params);
	}
	//Add entry into albumFiles
	QueryRow fparams;
	fparams.push_back(albumID);
	fparams.push_back(to_string(fileID));
	database.exec(INSERT_ALBUM_FILE, &fparams);
}

//Gallery specific functionality
int Gallery::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	string dupAlbumStr = database.select_single(SELECT_DUPLICATE_ALBUM_COUNT, &params);
	return stoi(dupAlbumStr);
}