// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "Serializer.h"

using namespace rapidjson;
using namespace std;
Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
	this->params = params;
	this->filecache = new Parameters();
	this->database = new Database("gallery.sqlite");
}

Gallery::~Gallery() {
	delete filecache;
	delete database;
}

string Gallery::getPage(page_id id) {
	if(id == SLIDESHOW) 
		return loadFile("/templates/slideshow.html");
	else if(id == MANAGE) 
		return loadFile("/templates/manage.html");
	else 
		return loadFile("/templates/index.html");
}

string Gallery::loadFile(char* uri) {
	std::string fileuri = params->get("basepath") + uri;
	std::string data;

		if(FileSystem::Exists(fileuri.c_str())) {
			data = HTML_HEADER;

			if(endsWith(uri, ".css"))
				data = CSS_HEADER;
			else if(endsWith(uri, ".js"))
				data = JS_HEADER;
			File* f = FileSystem::Open(fileuri.c_str(), "rb");
			FileData* filedata = FileSystem::Read(f);
			data.append(filedata->data, filedata->size);
			FileSystem::Close(f);
			delete filedata;
			delete f;
		} else {
			data.append(HTML_404);
		}
	
	return data;
}

string Gallery::getAlbums() {
	Query* query = database->select("SELECT COUNT(*) FROM albums;");
	string final;
	
	int nAlbums = std::stoi(((*query->response)[0][0]));
	delete query;
	Serializer* serializer = new Serializer();
	query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive FROM albums;");
	serializer->append(*query->response);
	final = serializer->get(RESPONSE_TYPE_DATA);
	
	
	return final;
}

RequestVars parseRequestVars(char* vars) {
	char* key = NULL;
	char* val = NULL;
	std::unordered_map<std::string, std::string> varmap;
	int i;
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
			key = NULL;
			val = NULL;
		}
	}
	if(vars[i] == '\0' && key != NULL && val != NULL) {
		//Copy the key and val into our unordered map.
		varmap[key] = val;
	}
	return varmap;
}

void Gallery::process(FCGX_Request* request) {
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
	std::string final;
	if(strcmp(method, "GET") == 0) {
		if(strcmp(uri, "/") == 0) {
			final = getPage(INDEX);
		} else if(strcmp(uri, "/manage") == 0) {
			final = getPage(MANAGE);
		} else if(strcmp(uri, "/slideshow") == 0) {
			final = getPage(SLIDESHOW);
		} else if(strstr(uri, "/api") == uri) {
			//Create an unordered map containing ?key=var pairs.
			RequestVars v = parseRequestVars(uri + 4);
			final = processVars(v);
		} else {
			//Return the file if it exists. Else return 404.
			final = loadFile(uri);
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
		RequestVars v = parseRequestVars(postdata);
		final = processVars(v);
		
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
}

string Gallery::processVars(RequestVars& vars) {
	string a = vars["t"];
	if(vars["t"] == "albums") {
		return getAlbums();
	} else {
		return JSON_HEADER + string("{}");
	}
}
