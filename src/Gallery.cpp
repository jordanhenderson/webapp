#include "Logging.h"
#include "Gallery.h"
#include "Image.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include <sha.h>

using namespace rapidjson;
using namespace std;

Gallery::Gallery(Parameters* params) {

	abort = 0;
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
	
	//Check authentication
	user = params->get("user");
	pass = params->get("pass");
	if(!user.empty() && !pass.empty()) {
		auth = 1;
	} else
		auth = 0;

	//Create/start process queue thread.
	
	thread_process_queue = new thread([this]() {
		while(!abort) {
			{
				unique_lock<mutex> lk(thread_process_mutex);
					//Wait for a signal from log functions (pushers)
				while(processQueue.empty() && !abort) 
					cv.wait(lk);
			}
			if(abort) return;
				
			while(!processQueue.empty()) {
				thread* t = processQueue.front();
				{
					std::lock_guard<std::mutex> lk(process_mutex);
					currentID = t->get_id();
				}
				cv_proc.notify_all();
				t->join();
				delete t;
				processQueue.pop();
			}
		}
		
	});
	serverTemplatesLock.lock();
	serverTemplates = new ctemplate::TemplateDictionary("");
	serverTemplatesLock.unlock();

	load_templates();
	thread_process_queue->detach();
	currentID = std::this_thread::get_id();
	//Create function map.
	GALLERYMAP(functionMap);
	
}

void Gallery::process_thread(std::thread* t) {
	bool empty;
	{
		lock_guard<mutex> lk(thread_process_mutex);
		empty = processQueue.empty();
		processQueue.push(t);
	}

	if(empty) {
		cv.notify_one();
	}
}

Gallery::~Gallery() {
	//Clean up client template files.
	for(FileData* data: clientTemplateFiles) {
		delete(data);
	}

	delete serverTemplates;


	abort = 1;
	cv.notify_all();
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

Response Gallery::getPage(const string& page, SessionStore& session, int publishSession) {
	Response r;

	if(publishSession) {
		time_t t; time(&t); add_days(t, 1);
		r = genCookie("sessionid", session.sessionid, &t);
	}

	//Template parsing
	if(contains(contentList, page + ".html")) {
		r = HTML_HEADER + r + END_HEADER;
		string output;
		serverTemplatesLock.lock();
		ctemplate::TemplateDictionary* d = serverTemplates->MakeCopy("");
		serverTemplatesLock.unlock();

		//Do additional template logic processing here (NOTE_LUA)
		if(!session.get("auth").empty() || !auth)
			d->ShowSection("LOGGED_IN");
		else
			d->ShowSection("NOT_LOGGED_IN");

		ctemplate::PerExpandData data;
		
		ctemplate::ExpandWithData(basepath + "/content/" + page + ".html", ctemplate::DO_NOT_STRIP, d, &data, &output);
		r.append(output);
		delete d;
		
	} else {
		//Plain file handling.
		string fileuri = basepath + page;
		File f;
		FileSystem::Open(fileuri.c_str(), "rb", &f);
		if(f.pszFile != NULL) {
			if(endsWith(fileuri, ".css"))
				r = CSS_HEADER + r;
			else if(endsWith(fileuri, ".js"))
				r = JS_HEADER + r;
			else
				r = HTML_HEADER + r;

			r.append(END_HEADER);

			FileData filedata;
			FileSystem::Read(&f, &filedata);
			r.append(filedata.data, filedata.size);
			FileSystem::Close(&f);
		} else {
			//File doesn't exists. 404.
			r.append(END_HEADER);
			r.append(HTML_404);
		}
	}
	return r;

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
int Gallery::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	Query dup_query(SELECT_DUPLICATE_ALBUM_COUNT);
	Query* query = database->select(&dup_query, &params);
	return stoi((*query->response)[0].at(0));
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
		} else if(uri[0] == '/' && (uri[1] == '\0' || uri[1] == '?')) {
			final = getPage("index", *store, createstore);
		} else {
			final = getPage(uri+1, *store, createstore);
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

	map<string, GallFunc>::const_iterator miter = functionMap.find(t);
	int hasfunc = miter != functionMap.end();
	if((hasfunc && !auth) || (hasfunc && !session.get("auth").empty()) || (hasfunc && t == "login")) {
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
	Query store_q(SELECT_SYSTEM("store_path"));
	Query thumbs_q(SELECT_SYSTEM("thumbs_path"));
	string storepath = database->select(&store_q)->response->at(0).at(0);
	string thumbspath = database->select(&thumbs_q)->response->at(0).at(0);
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
	Query album_count_q(SELECT_ALBUM_COUNT);
	Query* query = database->select(&album_count_q);
	int nAlbums = stoi((*query->response)[0][0]);
	if(nAlbums == 0) {
		return 0;
	}
	return 1;
}

void Gallery::load_templates() {
	//Force reload templates
	ctemplate::mutable_default_template_cache()->ReloadAllIfChanged(ctemplate::TemplateCache::IMMEDIATE_RELOAD);

	//Load content files (applicable templates)
	contentList.clear();
	string basepath = this->basepath + '/';
	string templatepath = basepath + "content/";
	contentList = FileSystem::GetFiles(templatepath, "", 0);
	for(string s : contentList) {
		//Preload templates.
		ctemplate::LoadTemplate(templatepath + s, ctemplate::DO_NOT_STRIP);
	}

	serverTemplatesLock.lock();
	delete serverTemplates;
	serverTemplates = new ctemplate::TemplateDictionary("");
	//Load server templates (See ctemplate Include Dictionaries)
	templatepath = basepath + "templates/server/";
	for(string s: FileSystem::GetFiles(templatepath, "", 0)) {
		size_t pos = s.find_last_of(".");
		ctemplate::TemplateDictionary* d = serverTemplates->AddIncludeDictionary(s.substr(0, pos));
		
		d->SetFilename(templatepath + s);
	}
	

	//Load client templates (inline insertion into content templates) - these are Handbrake (JS) templates.
	//First delete existing filedata. This should be optimised later.
	for(FileData* data: clientTemplateFiles) {
		delete data;
	}
	clientTemplateFiles.clear();

	templatepath = basepath + "templates/client/";
	for(string s: FileSystem::GetFiles(templatepath, "", 0)) {
		File f;
		FileData* data = new FileData();
		FileSystem::Open(templatepath + s, "rb", &f);
		string template_name = s.substr(0, s.find_last_of("."));
		FileSystem::Read(&f, data);
		std::transform(template_name.begin(), template_name.end(), template_name.begin(), ::toupper);
		serverTemplates->SetValueWithoutCopy("T_" + template_name, ctemplate::TemplateString(data->data,data->size));
	}
	serverTemplatesLock.unlock();
}
