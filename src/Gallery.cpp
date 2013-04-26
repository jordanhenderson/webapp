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
#include "decode.h"

using namespace rapidjson;
using namespace std;
using namespace base64;
Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
	this->params = params;
	this->filecache = new Parameters();
	this->database = new Database("gallery.sqlite");
	//Check authentication
	user = params->get("username");
	pass = params->get("pass");
	if(!user.empty() && !pass.empty()) {
		auth = 1;
	} else
		auth = 0;

}

Gallery::~Gallery() {
	delete filecache;
	delete database;
}

string Gallery::getPage(const char* page) {
	string pageuri = params->get("basepath") +  "/templates/" + string(page) + ".html";

	if(FileSystem::Exists(pageuri.c_str())) {
		string data = HTML_HEADER;
		File* f = FileSystem::Open(pageuri.c_str(), "rb");
		FileData* filedata = FileSystem::Read(f);
		data.append(filedata->data, filedata->size);
		FileSystem::Close(f);
		delete filedata;
		delete f;
		return data;
	} else {
		return loadFile(page);
	}

}

string Gallery::loadFile(const char* uri) {
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
	query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive, (SELECT thumbpath FROM thumbs WHERE id = albums.thumbid) AS thumb FROM albums;", 1);
	std::vector<unordered_map<string,string>*> maplist;
	for(vector<string> row: *query->response) {
		unordered_map<string, string>* rowmap = new unordered_map<string,string>();
		maplist.push_back(rowmap);
		if(!FileSystem::Exists(row[8].c_str())) {
			row[8] = DEFAULT_THUMB;
		}
		for(int i = 0; i < query->description->size(); i++) {
			(*rowmap)[(*query->description)[i]] =  row[i];
		}
		serializer->append(*rowmap);
	}


	final = serializer->get(RESPONSE_TYPE_DATA);
	for(unordered_map<string,string>* map : maplist) {
		delete map;
	}

	delete serializer;
	return final;
}

string Gallery::getAlbumsTable() {
	Query* query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive, (SELECT thumbpath FROM thumbs WHERE id = albums.thumbid) AS thumb FROM albums;", 1);

	unordered_map<string, string> tableData;
	tableData["THUMBS_PATH"] = THUMBS_PATH;
	tableData["thumb"] = "img";
	
	Serializer* serializer = new Serializer();
	serializer->append(tableData);
	serializer->append(*query->description);
	
	return serializer->get(RESPONSE_TYPE_TABLE);

	delete query;
	delete serializer;
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
			key = vars + i + 1;
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

	if(auth) {
		char* auth_header = FCGX_GetParam("HTTP_AUTHORIZATION", request->envp);
		if(auth_header != NULL && strstr(auth_header, "Basic ") == auth_header) {
			char* auth_details = auth_header + 6;
			int l = strlen(auth_details);
			//Decode the base64-encoded string.
			char* decoded_auth = new char[l + 1]();
			base64_decodestate decode_state;
			base64_init_decodestate(&decode_state);
			base64_decode_block(auth_details, l, decoded_auth, &decode_state);
			//Separate username and password.
			char* pass = strchr(decoded_auth, ':');
			*(pass) = '\0';
			pass++;
			if(pass != params->get("pass") || decoded_auth != params->get("username")) {
				FCGX_PutStr(HTTP_NO_AUTH, strlen(HTTP_NO_AUTH), request->out);
				return;
			}

		} else {
			FCGX_PutStr(HTTP_NO_AUTH, strlen(HTTP_NO_AUTH), request->out);
			return;
		}
	}

	std::string final;
	if(strcmp(method, "GET") == 0) {
		if(strstr(uri, "/api") == uri) {
			//Create an unordered map containing ?key=var pairs.
			RequestVars v = parseRequestVars(uri + 4);
			final = processVars(v);
		} else if(strcmp(uri, "/") == 0) {
			final = getPage("index");
		} else {
			//Return the file if it exists. Else return 404.
			final = getPage(uri);
		}
	} else if(strcmp(method, "POST") == 0) {
		//Read data from the input stream. (allocated using CONTENT_LENGTH)
		char* strlength = FCGX_GetParam("HTTP_CONTENT_LENGTH", request->envp);
		if(strlength == NULL)
			return;
		int len = atoi(strlength);
		char* postdata = new char[len + 1]();
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
		if(vars["f"] == "table")
			return getAlbumsTable();
		return getAlbums();
	} else {
		return JSON_HEADER + string("{}");
	}
}

