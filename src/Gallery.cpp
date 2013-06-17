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

	this->database = new Database((basepath + PATHSEP + dbpath).c_str());

	//Check authentication
	user = params->get("username");
	pass = params->get("pass");
	if(!user.empty() && !pass.empty()) {
		auth = 1;
	} else
		auth = 0;
	
}

Gallery::~Gallery() {
	delete database;
}

	
string Gallery::getPage(const char* page) {
	const string pageuri = basepath +  PATHSEP + "templates" + PATHSEP + page + ".html";

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
	string fileuri = basepath + uri;
	string data;

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


int Gallery::getAlbums(RequestVars& vars, Response& r) {

	string format = vars["f"];
	Serializer serializer;
	if(format == "table") {
		unordered_map<string, string> tableData;
		tableData["THUMBS_PATH"] = thumbspath;
		tableData["thumb"] = "img";
		char* tabledesc[] = {"id", "name", "added", "lastedited", "path", "type", "rating", "recursive", "thumb"};
		vector<string> desc(tabledesc, end(tabledesc));


		
		serializer.append(tableData);
		serializer.append(desc);
		r = serializer.get(RESPONSE_TYPE_TABLE);
	} else {
		
		unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums;");
		int nAlbums = stoi(((*query->response)[0][0]));
		query = database->select("SELECT id, name, added, lastedited, path, type, rating, recursive, thumbid AS thumb FROM albums;", 1);
		
		for(vector<string> row: *query->response) {
			unordered_map<string, string> rowmap;
		
			try{
				if(row[8].empty()) {
					//AlbumID -> SELECT first albumfiles, use that file.
					QueryRow params;
					params.push_back(row[0]);
					unique_ptr<Query> query = database->select("SELECT fileid FROM albumfiles WHERE albumid = ? ORDER BY id ASC LIMIT 1;", &params);
					string sFileID = (*query->response).at(0).at(0);
					int fileID = stoi(sFileID);
					if(fileID > 0) {
						//Attempt to use the file referenced by albumfiles
						QueryRow params;
						params.push_back(sFileID);
						unique_ptr<Query> query = database->select("SELECT path FROM  files WHERE id = ?;", &params);
						row[8] = row[4] + PATHSEP + (*query->response).at(0).at(0);
					}
				} else {
					//AlbumID -> thumbID -> thumb
					QueryRow params;
					params.push_back(row[8]);
					unique_ptr<Query> query = database->select("SELECT path FROM  thumbs WHERE id = ?;", &params);
					row[8] = row[4] + PATHSEP + (*query->response).at(0).at(0);
				}
			} catch(...) {
				row[8] = DEFAULT_THUMB;
			}

			for(int i = 0; i < query->description->size(); i++) {
				rowmap[(*query->description)[i]] =  row[i];
			}
			serializer.append(rowmap);
		}
		
		r = serializer.get(RESPONSE_TYPE_DATA);
		
	}
	return 0;
}


int Gallery::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;", &params);
	return stoi((*query->response)[0].at(0));
}

int Gallery::addBulkAlbums(RequestVars& vars, Response& r) {
	vector<string> paths;
	tokenize(vars["paths"],paths,"\n");
	for(string path: paths) {
		vars["path"] = vars["name"] = ref(path);
		if(addAlbum(vars, r) == 1) {
			//An error has occured!
			return 1;
		}
	}
	return 0;
}

int Gallery::addAlbum(RequestVars& vars, Response& r) {

	string type = vars["type"];
	if(!is_number(type))
		return 1;
	int nRecurse = GETCHK(vars["recurse"]);
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
				//Add the album.
				QueryRow params;
				//get the date.
				time_t t;
				time(&t);
				char date[9];
				strftime(date, 9, "%Y%m%d", localtime(&t));
			
				params.push_back(name);
				params.push_back(date);
				params.push_back(date);
				params.push_back(path);
				params.push_back(type);
				params.push_back(to_string(nRecurse));

				int albumID = database->exec("INSERT INTO albums (name, added, lastedited, path, type, recursive) VALUES (?,?,?,?,?,?);", &params);
				vector<string> files = FileSystem::GetFiles(basepath + PATHSEP + storepath + PATHSEP + path, "", nRecurse);
				int albumThumbID = -1;
				if(files.size() > 0) {
					for(int i = 0; i < files.size(); i++) {
						//Generate thumb.
						if(nGenThumbs) {
							FileSystem::MakePath(basepath + PATHSEP + thumbspath + PATHSEP + path + PATHSEP + files[i]);
							genThumb((path + PATHSEP + files[i]).c_str(), 180, 180);
						}
						//Insert file 
						QueryRow params;
						params.push_back(files[i]);
						params.push_back(files[i]);
						params.push_back(date);
				
						int fileID = database->exec("INSERT INTO files (name, path, added) VALUES (?,?,?);", &params);
						//Add entry into albumFiles
						QueryRow fparams;
						fparams.push_back(to_string(albumID));
						fparams.push_back(to_string(fileID));
						database->exec("INSERT INTO albumfiles (albumid, fileid) VALUES (?,?);", &fparams);

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
	r = serializer.get(RESPONSE_TYPE_MESSAGE);
	return addStatus;
}

int Gallery::delAlbums(RequestVars& vars, Response& r) {
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
		unique_ptr<Query> query = database->select("SELECT id, fileid FROM albumfiles WHERE albumid = ?;", &params);
		for(vector<string> row: (*query->response)) {
			//Delete thumbs first.
			QueryRow params;
			params.push_back(row.at(1));
			unique_ptr<Query> query = database->select("SELECT thumbid FROM files WHERE id = ?;", &params);
			try {
			string thumbid = (*query->response).at(0).at(0);
			if(!thumbid.empty()) {
				//Delete the thumb entry.
				QueryRow params;
				params.push_back(thumbid);
				database->exec("DELETE FROM thumbs WHERE id = ?;", &params);
			}
			} catch(out_of_range ex) {
				//albumfiles exists, file does not!
			}
			database->exec("DELETE FROM files WHERE id = ?;", &params);

			//Now delete albumfiles.
			params.clear();
			params.push_back(row.at(0));
			database->exec("DELETE FROM albumfiles WHERE id = ?;", &params);
		}

		//Finally, delete the album thumb, then the album.

		params.clear();
		params.push_back(album);
		query = database->select("SELECT path, thumbid FROM albums WHERE id = ?;", &params);
		try {
			string path = (*query->response).at(0).at(0);
			string thumbid = (*query->response).at(0).at(1);
			if(!thumbid.empty()) {
				//Delete the thumb entry.
				QueryRow params;
				params.push_back(thumbid);
				database->exec("DELETE FROM thumbs WHERE id = ?;", &params);
			}
			//Delete the album.
			database->exec("DELETE FROM albums WHERE id = ?;", &params);
			if(delFiles) {
				//Delete the albums' files.
				FileSystem::DeletePath(basepath + PATHSEP + storepath + PATHSEP + path);
			}
			if(delThumbs) {
				FileSystem::DeletePath(basepath + PATHSEP + thumbspath + PATHSEP + path);
			}

		} catch(out_of_range ex) {
			//album not exists?
		}
	}

	map["msg"] = "DELETE_SUCCESS";
	map["close"] = "1";
	serializer.append(map);
	r = serializer.get(RESPONSE_TYPE_MESSAGE);
	return 0;
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


void Gallery::process(FCGX_Request* request) {
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);

	if(auth) {
		char* auth_header = FCGX_GetParam("HTTP_AUTHORIZATION", request->envp);
		if(auth_header != NULL && strstr(auth_header, "Basic ") == auth_header) {
			char* auth_details = auth_header + 6;
			int l = strlen(auth_details);
			//Decode the base64-encoded string. We need to initialise with 0's as the decoded string length is unknown (unless we check padding).
			char* decoded_auth = new char[l / 4 * 3 + 1]();
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

	string final;
	if(strcmp(method, "GET") == 0) {
		if(strstr(uri, "/api") == uri) {
			//Create an unordered map containing ?key=var pairs.
			RequestVars v;
			parseRequestVars(uri + 4, v);
			final = processVars(v);
		} else if(uri[0] == PATHSEP && uri[1] == '\0') {
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
		final = processVars(v);
		delete[] postdata;
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
	
}

int Gallery::getDuplicateAlbums(const char* name, const char* path) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;", &params, 1);
	return stoi((*query->response).at(0).at(0));
}

vector<string> Gallery::getRandomFileIds() {
	vector<string> s;
	QueryRow params;
	params.push_back(XSTR(ALBUM_RANDOM));
	unique_ptr<Query> query = database->select("SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;", &params);
	for(vector<string> row: *query->response) {
		QueryRow params;
		params.push_back(row.at(0));
		unique_ptr<Query> query = database->select("SELECT id FROM files WHERE albumid = ? AND enabled = 1;", &params);
		for(vector<string> s_row: *query->response) {
			s.push_back(s_row.at(0));
		}
	}
	return s;
}

vector<string> Gallery::getSetIds() {
	vector<string> s;
	QueryRow params;
	params.push_back(XSTR(ALBUM_SET));
	unique_ptr<Query> query = database->select("SELECT id FROM albums WHERE type = ?;", &params);
	for(vector<string> row: *query->response) {
		s.push_back(row[0]);
	}
	return s;
}

string Gallery::getFilename(int fileid) {
	QueryRow params;
	params.push_back(to_string(fileid));
	unique_ptr<Query> query = database->select("SELECT filename FROM files WHERE id = ?;", &params);
	string filename = (*query->response).at(0).at(0);
	return filename;
}


string Gallery::processVars(RequestVars& vars) {
	string t = vars["t"];
	map<string, GallFunc> m;
	GALLERYMAP(m);
	map<string, GallFunc>::const_iterator miter = m.find(t);
	if(miter != m.end()) {
		GallFunc f = miter->second;
		Response r;
		//Call the function (r passed as the response)
		(this->*f)(vars, r);
		return r;
	} else {
		return JSON_HEADER "{}";
	}
}

int Gallery::genThumb(const char* file, double shortmax, double longmax) {
	string imagepath = basepath + PATHSEP + storepath + PATHSEP + file;
	
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


   	image.resize(newWidth, newHeight);
	image.save(basepath + PATHSEP + thumbspath + PATHSEP + file);
	return 0;
}
