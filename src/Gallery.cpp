// gallery.cpp : Defines the entry point for the console application.
//

#include "Logging.h"
#include "Gallery.h"
#include "Image.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "decode.h"
#include <sha.h>

using namespace rapidjson;
using namespace std;
using namespace base64;


Gallery::Gallery(Parameters* params) {
	this->params = params;
	dbpath = params->get("dbpath");
	
	basepath = params->get("basepath");
	this->database = new Database((basepath + PATHSEP + dbpath).c_str());

	session = new Session();

	//Check authentication
	user = params->get("user");
	pass = params->get("pass");
	if(!user.empty() && !pass.empty()) {
		auth = 1;
	} else
		auth = 0;

	//Create function map.
	GALLERYMAP(m);
	
}

Gallery::~Gallery() {
	delete database;
	delete session;
}

//Generate a Set-Cookie header provided name, value and date.
string Gallery::genCookie(const string& name, const string& value, time_t* date) {
	if(date == NULL) return string_format("Set-Cookie: %s=%s\r\n", name.c_str(), value.c_str());
	else {
		string date_str = date_format("%a, %d-%b-%Y %H:%M:%S GMT", 29, date, 1);
		return string_format("Set-Cookie: %s=%s; Expires=%s\r\n",name.c_str(), value.c_str(), date_str.c_str());
	}
}

string Gallery::getPage(const char* page) {
	char pageuri[255];
	snprintf(pageuri, 255, "%s" PSEP "content" PSEP "%s.html", basepath.c_str(), page);
	
	//Template parsing
	if(FileSystem::Exists(pageuri)) {
		string data = HTML_HEADER END_HEADER;
		File f;
		FileSystem::Open(pageuri, "rb", &f);
		FileData filedata;

		FileSystem::Read(&f,&filedata);
		data.append(filedata.data, filedata.size);
		FileSystem::Close(&f);

		return data;
	} else {
		return loadFile(page);
	}

}

string Gallery::loadFile(const string& uri) {
	string fileuri = basepath + uri;
	string data;

		if(FileSystem::Exists(fileuri)) {
			data = HTML_HEADER END_HEADER;

			if(endsWith(uri, ".css"))
				data = CSS_HEADER END_HEADER;
			else if(endsWith(uri, ".js"))
				data = JS_HEADER END_HEADER;
			File f;
			FileSystem::Open(fileuri.c_str(), "rb", &f);
			FileData filedata;
			FileSystem::Read(&f, &filedata);
			data.append(filedata.data, filedata.size);
			FileSystem::Close(&f);
		} else {
			data.append(HTML_404);
		}
	
	return data;
}

int Gallery::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	
	Query query = database->select(SELECT_DUPLICATE_ALBUM_COUNT, &params);
	return stoi((*query.response)[0].at(0));
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

CookieVars Gallery::parseCookies(const char* cookies){
	CookieVars v;
	if(cookies == NULL)
		return v;
	int c = 0, i;
	string key;
	string val;
	for(i = 0; cookies[i] != '\0'; i++) {
		if(cookies[i] == '=') {
			key = string(cookies + c, i-c);
			c = i;
		} else if(cookies[i] == ';') {
			val = string(cookies + c + 1, i-c-1);
			v.insert(make_pair(key, val));
			c = i;
		}
	}
	if(c < i) {
		val = string(cookies + c + 1, i-c-1);
		v.insert(make_pair(key,val));
	}
	return v;
}


void Gallery::process(FCGX_Request* request) {
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
	char* cookie_str = FCGX_GetParam("HTTP_COOKIE", request->envp);
	//Get/set the session id.
	
	CookieVars cookies = parseCookies(cookie_str);
	CookieVars::iterator it = cookies.find("sessionid");
	SessionStore* store = NULL;
	int createstore = 0;
	if(it != cookies.end()) {
		store = session->get_session(it->second);
	}
	//Create a new session.
	if(store == NULL) {
		char* host = FCGX_GetParam("HTTP_HOST", request->envp);
		char* user_agent = FCGX_GetParam("HTTP_USER_AGENT", request->envp);
		store = session->new_session(host, user_agent);
		createstore = 1;
	}

	string final;
	if(strcmp(method, "GET") == 0) {
		if(strstr(uri, "/api") == uri) {
			//Create an unordered map containing ?key=var pairs.
			RequestVars v;
			parseRequestVars(uri + 4, v);
			final = processVars(v, *store, createstore);
		} else if(uri[0] == PATHSEP && uri[1] == '\0') {
			final = getPage("index");
		} else {
			final = getPage(uri);
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
		for(auto outer = v.begin(); outer!= v.end(); ++outer) {
			v[outer->first] = replaceAll(v[outer->first], "%2F", "/");
			v[outer->first] = replaceAll(v[outer->first], "%0D%0A", "\n");
		}
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

	map<string, GallFunc>::const_iterator miter = m.find(t);
	int hasfunc = miter != m.end();
	if((hasfunc && !auth) || (hasfunc && !session.get("auth").empty()) || (hasfunc && t == "login")) {
		GallFunc f = miter->second;
		r.append(END_HEADER);
		(this->*f)(vars, r, session);

		return r;
	} else {
		r.append(END_HEADER "{}");
		return r;
	}
}

int Gallery::genThumb(const char* file, double shortmax, double longmax) {
	string storepath = database->select(SELECT_SYSTEM("store_path")).response->at(0).at(0);
	string thumbspath = database->select(SELECT_SYSTEM("thumbs_path")).response->at(0).at(0);
	string imagepath = basepath + PATHSEP + storepath + PATHSEP + file;
	
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
	image.save(basepath + PATHSEP + thumbspath + PATHSEP + file);

	return ERROR_SUCCESS;
}

int Gallery::hasAlbums() {
	Query query = database->select(SELECT_ALBUM_COUNT);
	int nAlbums = stoi(((*query.response)[0][0]));
	if(nAlbums == 0) {
		return 0;
	}
	return 1;
}

//Public API functions. 
int Gallery::search(RequestVars& vars, Response& r, SessionStore& s) {
	string query = SELECT_FILE_DETAILS;
	QueryRow params;
	
	if(vars["t"] == "search") {
		query.append(CONDITION_SEARCH);
		params.push_back("%" + vars["q"] + "%");
	}

	Query q(query, &params);
	return getData(q, vars, r, s);

}

int Gallery::login(RequestVars& vars, Response& r, SessionStore& store) {
	string user = vars["user"];
	string pass = vars["pass"];
	Serializer s;
	if(!user.empty() && !pass.empty()) {
		if(user == this->user && pass == this->pass) {
			store.store("auth", "TRUE");
			s.append("msg", "LOGIN_SUCCESS", 1);

		}
	}
	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	return 0;
}

int Gallery::addBulkAlbums(RequestVars& vars, Response& r, SessionStore& s) {
	vector<string> paths;
	tokenize(vars["paths"],paths,"\n");
	for(string path: paths) {
		vars["path"] = vars["name"] = ref(path);
		if(addAlbum(vars, r, s) == 1) {
			//An error has occured!
			return 1;
		}
	}
	return 0;
}

int Gallery::addAlbum(RequestVars& vars, Response& r, SessionStore&) {

	string type = vars["type"];
	if(!is_number(type))
		return 1;
	int nRecurse = GETCHK(vars["recursive"]);
	int nGenThumbs = GETCHK(vars["genthumbs"]);
	string name = vars["name"];
	string path = vars["path"];
	int addStatus = 0;
	Serializer serializer;
	unordered_map<string, string> map;
	if(!name.empty() && !path.empty()) {
		//_addAlbum
		int nDuplicates = getDuplicates(name, path);
		if(nDuplicates > 0) {
			map["msg"] = "DUPLICATE_ALBUM";
			map["close"] = "0";
			addStatus = 2;
		} else {
			//Thread the adding code using a sneaky lambda. TODO: Add cleanup of thread.
			thread aa([this, name, path, type, nRecurse, nGenThumbs]() {
				string storepath = database->select(SELECT_SYSTEM("store_path")).response->at(0).at(0);
				string thumbspath = database->select(SELECT_SYSTEM("thumbs_path")).response->at(0).at(0);
				//Add the album.
				QueryRow params;
				//get the date.
				string date = date_format("%Y%m%d",8);

				params.push_back(name);
				params.push_back(date);
				params.push_back(date);
				params.push_back(path);
				params.push_back(type);
				params.push_back(to_string(nRecurse));

				int albumID = database->exec(INSERT_ALBUM, &params);
				vector<string> files = FileSystem::GetFiles(basepath + PATHSEP + storepath + PATHSEP + path, "", nRecurse);
				int albumThumbID = -1;
				if(files.size() > 0) {
					for(int i = 0; i < files.size(); i++) {
						string thumbID;
						//Generate thumb.
						if(nGenThumbs) {
							FileSystem::MakePath(basepath + PATHSEP + thumbspath + PATHSEP + path + PATHSEP + files[i]);
							logger->printf("Adding file: %s (%d)", files[i].c_str(), i);
							if(genThumb((path + PATHSEP + files[i]).c_str(), 200, 200) == ERROR_SUCCESS) {
								QueryRow params;
								params.push_back(path + PATHSEP + files[i]);
								int nThumbID = database->exec(INSERT_THUMB, &params);
								if(nThumbID > 0) {
									thumbID = to_string(nThumbID);
								}
							}
						}
						//Insert file 
						QueryRow params;
						params.push_back(files[i]);
						params.push_back(files[i]);
						params.push_back(date);
						params.push_back(thumbID);

						int fileID = database->exec(INSERT_FILE, &params);
						//Add entry into albumFiles
						QueryRow fparams;
						fparams.push_back(to_string(albumID));
						fparams.push_back(to_string(fileID));
						database->exec(INSERT_ALBUM_FILE, &fparams);

					}
				}
			});
			aa.detach();
			map["msg"] = "ADDED_SUCCESS";
			map["close"] = "1";

		}
	} else {
		map["msg"] = "FAILED";
		map["close"] = "1";
		addStatus = 1;
	}
	serializer.append(map);
	r.append(serializer.get(RESPONSE_TYPE_MESSAGE));
	return addStatus;
}

int Gallery::delAlbums(RequestVars& vars, Response& r, SessionStore&) {
	Serializer serializer;
	unordered_map<string, string> map;


	int delThumbs = GETCHK(vars["delthumbs"]);
	int delFiles = GETCHK(vars["delfiles"]);
	vector<string> albums;
	tokenize(vars["a"],albums,",");
	for(string album: albums) {
		QueryRow params;

		//Delete files.
		params.push_back(album);
		Query query = database->select(SELECT_ALBUM_FILE, &params);
		for(vector<string> row: (*query.response)) {
			//Delete thumbs first.
			QueryRow params;
			params.push_back(row.at(1));
			Query query = database->select(SELECT_FILE_THUMBID, &params);
			
			string thumbid = query.response->at(0).at(0);
			if(!thumbid.empty()) {
				//Delete the thumb entry.
				QueryRow params;
				params.push_back(thumbid);
				database->exec(DELETE_THUMB, &params);
			}
			
			database->exec(DELETE_FILE, &params);

			//Now delete albumfiles.
			params.clear();
			params.push_back(row.at(0));
			database->exec(DELETE_ALBUM_FILE, &params);
		}

		//Finally, delete the album thumb, then the album.

		params.clear();
		params.push_back(album);
		Query delquery = database->select(SELECT_ALBUM_PATH_THUMB, &params);
			if(delquery.response->size() > 0) {

			string path = delquery.response->at(0).at(0);
			string thumbid = delquery.response->at(0).at(1);
			if(!thumbid.empty()) {
				//Delete the thumb entry.
				QueryRow params;
				params.push_back(thumbid);
				database->exec(DELETE_THUMB, &params);
			}
			//Delete the album.
			database->exec(DELETE_ALBUM, &params);
			string storepath = database->select(SELECT_SYSTEM("store_path")).response->at(0).at(0);
			string thumbspath = database->select(SELECT_SYSTEM("thumbs_path")).response->at(0).at(0);
			if(delFiles) {
				//Delete the albums' files.
				FileSystem::DeletePath(basepath + PATHSEP + storepath + PATHSEP + path);
			}
			if(delThumbs) {
				FileSystem::DeletePath(basepath + PATHSEP + thumbspath + PATHSEP + path);
			}
		}

	}

	map["msg"] = "DELETE_SUCCESS";
	map["close"] = "1";
	serializer.append(map);
	r.append(serializer.get(RESPONSE_TYPE_MESSAGE));
	return 0;
}

int Gallery::getData(Query& query, RequestVars& vars, Response& r, SessionStore&) {
	Serializer serializer;
	
	if(!hasAlbums()) {
		serializer.append("msg", "NO_ALBUMS", 1);
		r.append(serializer.get(RESPONSE_TYPE_FULL_MESSAGE));
		return 0;
	}

	string limit = vars["limit"].empty() ? XSTR(DEFAULT_PAGE_LIMIT) : vars["limit"];
	query.params->push_back(limit);
	query.dbq->append(SELECT_DETAILS_END);
	
	query.description = new QueryRow();
	query.response = new QueryResponse();
	database->select(&query);
	serializer.append(query);

	r.append(serializer.get(RESPONSE_TYPE_DATA));
	return 0;
}

int Gallery::getFiles(RequestVars& vars, Response& r, SessionStore&s) {
	string query = SELECT_FILE_DETAILS;
	QueryRow params;
	
	string album = vars["album"];
	string id = vars["id"];
	if(!album.empty() && id.empty()) {
		query.append(CONDITION_ALBUM);
		params.push_back(album);
		database->exec(INC_ALBUM_VIEWS, &params);
	}
	else if(!id.empty()) {
		query.append(CONDITION_FILEID);
		params.push_back(id);
		//Increment views.
		database->exec(INC_FILE_VIEWS, &params);
	} else {
		query.append(CONDITION_FILE_GROUPED);
	}
	

	Query q(query, &params);
	return getData(q, vars, r, s);
	
}

int Gallery::getAlbums(RequestVars& vars, Response& r, SessionStore&s) {
	string query = SELECT_ALBUM_DETAILS;
	string id = vars["id"];
	QueryRow params;
	if(!id.empty()) {
		query.append(CONDITION_ALBUM);
		params.push_back(id);
	}
	Query q(query, &params);
	return getData(q, vars, r, s);
}

int Gallery::setThumb(RequestVars& vars, Response& r, SessionStore&) {
	string type = vars["type"]+'s';
	string id = vars["id"];
	string path = vars["path"];
	const char* target;
	QueryRow params;
	params.push_back(path);
	int thumbID = database->exec(INSERT_THUMB, &params);

	params.clear();
	params.push_back(to_string(thumbID));
	params.push_back(id);
	//Update the album/file thumb ID.
	char thumbquery[255];
	snprintf(thumbquery, 255, UPDATE_THUMB, thumbID);
	database->exec(thumbquery, &params);

	return 0;
}