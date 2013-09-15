#include "Logging.h"
#include "Gallery.h"
#include "Image.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include <sha.h>

using namespace std;
using namespace ctemplate;

void Gallery::runScript(const char* filename, LuaParam* params, int nArgs) {
	for(LuaChunk* c: loadedScripts) {
		if(c->filename == filename) {
			runScript(c, params, nArgs);
		}
	}
}

//Run scripts based on their index (system scripts; see Schema.h)
void Gallery::runScript(int index, LuaParam* params, int nArgs) {
	if(index < systemScripts.size()) {
		LuaChunk* script = systemScripts.at(index);
		if(script != NULL) runScript(script, params, nArgs);
	}
}

//Run script given a LuaChunk. Called by public runScript methods.
void Gallery::runScript(LuaChunk* c, LuaParam* params, int nArgs) {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luaL_loadbuffer(L, c->bytecode.c_str(), c->bytecode.length(), c->filename.c_str());
	if(params != NULL) {
		for(int i = 0; i < nArgs; i++) {
			LuaParam* p = params + i;
			lua_pushlightuserdata(L, p->d);
			lua_setglobal(L, p->p);
		}
	}
	lua_pcall(L, 0, 0, 0);
}

Gallery::Gallery(Parameters* params) {
	shutdown_handler = 0;
	this->params = params;
	dbpath = params->get("dbpath");
	
	basepath = params->get("basepath");
	database = new Database(DATABASE_TYPE_SQLITE, (basepath + '/' + dbpath).c_str());

	//Enable pragma foreign keys.
	database->exec(PRAGMA_FOREIGN);

	//Create the schema. Running this even when tables exist prevent issues later on.
	string schema = CREATE_DATABASE;
	vector<string> schema_queries;
	tokenize(schema, schema_queries, ";");
	for(string s : schema_queries) {
		database->exec(s);
	}
	
	//Set the 'current thread id' to this thread. Essentially ensures no thread will be started (yet).

	currentID = std::this_thread::get_id();

	//Create/start process queue thread. (CONSUMER)
	thread_process_queue = new thread([this]() {
		while(!shutdown_handler) {
			{
				unique_lock<mutex> lk(mutex_queue_add);
				//Wait for a signal from log functions (pushers)
				while(processQueue.empty() && !shutdown_handler) 
					cv_queue_add.wait(lk);
			}
			if(shutdown_handler) return;
				
			while(!processQueue.empty()) {
				thread* t = processQueue.front();
				{
					lock_guard<mutex> lk(mutex_thread_start);
					currentID = t->get_id();
				}
				//Notify all threads that a new thread is ready to process.
				//Only the thread with the correct ID will actually be started.
				cv_thread_start.notify_all(); 
				//Join the started thread.
				t->join();
				delete t;
				processQueue.pop();
			}
		}
	});

	thread_process_queue->detach();

	contentTemplates = new TemplateDictionary("");

	refresh_templates();

	//Create function map.
	APIMAP(functionMap);

	systemScripts.reserve(SYSTEM_SCRIPT_COUNT);

	refresh_scripts();
	LuaParam luaparams[] = {{"hooks", &functionMap}};
	runScript(SYSTEM_SCRIPT_INIT, (LuaParam*)params, 1 );
}

//PRODUCER
void Gallery::process_thread(std::thread* t) {
	bool empty;
	{
		lock_guard<mutex> lk(mutex_queue_add);
		empty = processQueue.empty();
		processQueue.push(t);
	}

	if(empty) {
		cv_queue_add.notify_one();
	}
}

Gallery::~Gallery() {
	//Clean up client template files.
	for(TemplateData data: clientTemplateFiles) {
		delete(data.data);
	}

	delete contentTemplates;


	shutdown_handler = 1;
	cv_queue_add.notify_all();
	if(thread_process_queue->joinable())
		thread_process_queue->join();
	delete thread_process_queue;
	delete database;
}

//Generate a Set-Cookie header provided name, value and date.
string Gallery::genCookie(const string& name, const string& value, time_t* date) {
	if(date == NULL) return string_format("Set-Cookie: %s=%s\r\n", name.c_str(), value.c_str());
	else {
		string date_str = date_format("%a, %d-%b-%Y %H:%M:%S GMT", 29, date, 1);
		return string_format("Set-Cookie: %s=%s; Expires=%s\r\n",name.c_str(), value.c_str(), date_str.c_str());
	}
}

Response Gallery::getPage(const string& uri, SessionStore& session, int publishSession) {
	Response r;
	string page;
	if(publishSession) {
		time_t t; time(&t); add_days(t, 1);
		r = genCookie("sessionid", session.sessionid, &t);
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
		TemplateDictionary* d = contentTemplates->MakeCopy("");
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

}

void parseRequestVars(char* vars, RequestVars& varmap) {
	char* key = NULL;
	char* val = NULL;
	int i;
	//Set key to be vars, in case ? is not present.
	key = vars;
	for(i = 0; vars[i] != '\0'; i++) {
		if(vars[i] == '?') {
			key = vars + i + 1;
		}
		if(key != NULL && vars[i] == '=') {
			//Terminate the string at the = for key.
			*(vars + i) = '\0';
			val = vars + i + 1;
		}
		if(val != NULL && vars[i] == '&') {
			//Terminate the string at the & for val.
			*(vars + i) = '\0';
			//Copy the key and val into our unordered map.
			varmap[key] = val;
			key = vars + i + 1;
			val = NULL;
		}
	}
	if(vars[i] == '\0' && key != NULL && val != NULL) {
		//Copy the key and val into our unordered map.
		varmap[key] = val;
	}
	return;
}

std::string Gallery::getCookieValue(const char* cookies, const char* key){
	if(cookies == NULL || key == NULL)
		return EMPTY;

	int len = strlen(key);
	int i;
	for(i = 0; cookies[i] != '\0'; i++) {
		if(cookies[i] == '=' && i >= len) {
			const char* k = cookies + i - len;

			if(strncmp(k, key, len) == 0) {
				//Find the end character.
				int c;
				for (c = i + 1; cookies[c] != '\0' && cookies[c] != ';'; c++);
				return string(k+len+1, c - i - 1);
			}
		}
	}
	return EMPTY;
}


void Gallery::process(FCGX_Request* request) {
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
}


Response Gallery::processVars(RequestVars& vars, SessionStore& session, int publishSession) {
	string t = vars["t"];

	Response r = JSON_HEADER;
	if(publishSession) {
		time_t t; time(&t); add_days(t, 1);
		r.append(genCookie("sessionid", session.sessionid, &t));
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
}

int Gallery::genThumb(const char* file, double shortmax, double longmax) {
	string storepath = database->select(SELECT_SYSTEM("store_path"));
	string thumbspath = database->select(SELECT_SYSTEM("thumbs_path"));
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
	string albumsStr = database->select(SELECT_ALBUM_COUNT);
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
	for(TemplateData data: clientTemplateFiles) {
		delete data.data;
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
	for(LuaChunk* b: loadedScripts) {
		delete b;
	}

	loadedScripts.clear();

	string basepath = this->basepath + '/';
	string pluginpath = basepath + "plugins/";
	const char* system_script_names[] = SYSTEM_SCRIPT_FILENAMES;
	for(string s: FileSystem::GetFiles(pluginpath, "", 1)) {
		LuaChunk* b = new LuaChunk(s);
		lua_State* L = luaL_newstate();
		if(L != NULL) {
			luaL_openlibs(L);
			luaL_loadfile(L, (pluginpath + s).c_str());
			lua_dump(L, LuaWriter, b);
			lua_close(L);
			//Bytecode loaded. push back for later.
			loadedScripts.push_back(b);
		} else delete b;

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
			int nThumbID = database->exec(INSERT_THUMB, &params);
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
		fileID = database->exec(INSERT_FILE, &params);
	} else {
		fileID = database->exec(INSERT_FILE_NO_THUMB, &params);
	}
	//Add entry into albumFiles
	QueryRow fparams;
	fparams.push_back(albumID);
	fparams.push_back(to_string(fileID));
	database->exec(INSERT_ALBUM_FILE, &fparams);
}

//Gallery specific functionality
int Gallery::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	string dupAlbumStr = database->select(SELECT_DUPLICATE_ALBUM_COUNT, &params);
	return stoi(dupAlbumStr);
}