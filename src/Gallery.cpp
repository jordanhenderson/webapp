// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"
#include "Image.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "Serializer.h"
#include "decode.h"

using namespace rapidjson;
using namespace std;
using namespace base64;


Gallery::Gallery(shared_ptr<Parameters>& params, shared_ptr<Logging>& logger) {
	this->logger = logger;
	this->params = params;
	storepath = params->get("storepath");
	basepath = params->get("basepath");
	dbpath = params->get("dbpath");
	thumbspath = params->get("thumbspath");

	this->database = unique_ptr<Database>(new Database((basepath + "/" + dbpath).c_str()));

	//Check authentication
	user = params->get("username");
	pass = params->get("pass");
	if(!user.empty() && !pass.empty()) {
		auth = 1;
	} else
		auth = 0;

	this->genThumb("test/testgif2.gif", 300, 300);
	
}

Gallery::~Gallery() {
	
}

string Gallery::getPage(const char* page) {
	string pageuri = basepath +  "/templates/" + string(page) + ".html";

	if(FileSystem::Exists(pageuri)) {
		string data = HTML_HEADER;
		unique_ptr<File> f = FileSystem::Open(pageuri.c_str(), "rb");
		unique_ptr<FileData> filedata = FileSystem::Read(f);
		data.append(filedata->data, filedata->size);
		FileSystem::Close(f);

		return data;
	} else {
		return loadFile(page);
	}

}

string Gallery::loadFile(const char* uri) {
	std::string fileuri = basepath + uri;
	std::string data;

		if(FileSystem::Exists(fileuri)) {
			data = HTML_HEADER;

			if(endsWith(uri, ".css"))
				data = CSS_HEADER;
			else if(endsWith(uri, ".js"))
				data = JS_HEADER;
			unique_ptr<File> f = FileSystem::Open(fileuri.c_str(), "rb");
			unique_ptr<FileData> filedata = FileSystem::Read(f);
			data.append(filedata->data, filedata->size);
			FileSystem::Close(f);
		} else {
			data.append(HTML_404);
		}
	
	return data;
}

string Gallery::getAlbums() {
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums;");
	string final;
	
	int nAlbums = std::stoi(((*query->response)[0][0]));
	Serializer serializer;
	query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive, (SELECT thumbpath FROM thumbs WHERE id = albums.thumbid) AS thumb FROM albums;", 1);
	for(vector<string> row: *query->response) {
		//Create unique pointers for the map, store them in the maps vector. This way, the map (AKA string pointers) are retained until maps is deconstructed.
		unique_ptr<unordered_map<string, string>> rowmap = unique_ptr<unordered_map<string,string>>(new unordered_map<string,string>);
		
		if(!FileSystem::Exists(row[8])) {
			row[8] = DEFAULT_THUMB;
		}
		for(int i = 0; i < query->description->size(); i++) {
			(*rowmap)[(*query->description)[i]] =  row[i];
		}
		
		serializer.append(move(rowmap));

	}

	final = serializer.get(RESPONSE_TYPE_DATA);

	return final;
}

string Gallery::getAlbumsTable() {
	unique_ptr<Query> query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive, (SELECT thumbpath FROM thumbs WHERE id = albums.thumbid) AS thumb FROM albums;", 1);

	unordered_map<string, string> tableData;
	tableData["THUMBS_PATH"] = thumbspath;
	tableData["thumb"] = "img";
	
	Serializer serializer;
	serializer.append(tableData);
	serializer.append(*query->description);
	
	return serializer.get(RESPONSE_TYPE_TABLE);
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
				delete[] decoded_auth;
				return;
			}
		
			delete[] decoded_auth;

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
		delete[] postdata;
		
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
}

int Gallery::getDuplicateAlbums(char* name, char* path) {
	unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
	params->push_back(name);
	params->push_back(path);
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;", params, 1);
	return stoi((*query->response)[0][0]);
}

vector<string> Gallery::getRandomFileIds() {
	vector<string> s;
	unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
	params->push_back(XSTR(ALBUM_RANDOM));
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;", params);
	for(vector<string> row: *query->response) {
		unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
		params->push_back(row[0]);
		unique_ptr<Query> query = database->select("SELECT id FROM files WHERE albumid = ? AND enabled = 1;", params);
		for(vector<string> s_row: *query->response) {
			s.push_back(s_row[0]);
		}
	}
	return s;
}

vector<string> Gallery::getSetIds() {
	vector<string> s;
	unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
	params->push_back(XSTR(ALBUM_SET));
	unique_ptr<Query> query = database->select("SELECT id FROM albums WHERE type = ?;", params);
	for(vector<string> row: *query->response) {
		s.push_back(row[0]);
	}
	return s;
}

string Gallery::getFilename(int fileid) {
	unique_ptr<QueryRow> params = unique_ptr<QueryRow>(new QueryRow());
	params->push_back(to_string(fileid));
	unique_ptr<Query> query = database->select("SELECT filename FROM files WHERE id = ?;", params);
	string filename = (*query->response)[0][0];
	return filename;
}

string Gallery::processVars(RequestVars& vars) {
	if(vars["t"] == "albums") {
		if(vars["f"] == "table")
			return getAlbumsTable();
		return getAlbums();
	} else {
		return JSON_HEADER + string("{}");
	}
}


int Gallery::genThumb(char* file, double shortmax, double longmax) {
	string imagepath = basepath + "/" + storepath + "/" + file;
	Image image(imagepath);
	int err = image.GetLastError();
	if(image.GetLastError() != ERROR_SUCCESS){
		return -1;
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


   	image.resize(30, 80);
	//image.save(basepath + "/" + thumbspath + "/" + file);
	image.save(basepath + "/" + thumbspath + "/" + "out.gif");
	return 0;
}