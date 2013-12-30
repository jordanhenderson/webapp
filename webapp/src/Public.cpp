/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */
/*
int getData(Webapp* webapp, void* vars, webapp_str_t* out, SessionStore* store) {
	/*
	Serializer serializer;

	if (!hasAlbums()) {
		serializer.append("msg", "NO_ALBUMS", 1);
		r.append(serializer.get(RESPONSE_TYPE_MESSAGE));
		return 0;
	}

	string limit = vars["limit"].empty() ? XSTR(DEFAULT_PAGE_LIMIT) : vars["limit"];
	int nLimit = stoi(limit);
	if (nLimit > MAX_PAGE_LIMIT) {
		limit = XSTR(DEFAULT_PAGE_LIMIT);
		nLimit = DEFAULT_PAGE_LIMIT;
	}

	string col = vars["by"].empty() ? "id" : vars["by"];

	int nPage = vars["page"].empty() ? 0 : stoi(vars["page"]);
	int page = nPage * nLimit;

	if (col == "rand") page = 0;

	query.params->push_back(to_string(page));
	query.params->push_back(limit);

	string order = (vars["order"] == "asc") ? "ASC" : "DESC";
	//TODO validate col

	if (col == "rand") {
		query.dbq->append(ORDER_DEFAULT DB_FUNC_RANDOM);
	}
	else {
		query.dbq->append(ORDER_DEFAULT + col + " " + order);
	}
	query.dbq->append(" LIMIT ?,? ");
	query.description = new QueryRow();
	query.response = new QueryResponse();
	database.select(&query);
	serializer.append(query);

	r.append(serializer.get(RESPONSE_TYPE_DATA));
	*/
	//return 0;
//}

/*
extern "C" {
	APIEXPORT int getAlbums(Webapp* webapp, void* vars, webapp_str_t* out, SessionStore* store) {
		string query = SELECT_ALBUM_DETAILS;

		string id = "0";
		QueryRow params;
		if (!id.empty()) {
			query.append(CONDITION_ALBUMID);
			params.push_back(id);
		}
		Query q(query, &params);
		return getData(webapp, vars, out, store);
	}
};
*/

//Public API functions.
/*
int Webapp::disableFiles(RequestVars& vars, Response& r, SessionStore& s) {
	string query = TOGGLE_FILES;
	QueryRow params;

	string album = vars["album"];
	string id = vars["id"];
	if(!album.empty() && id.empty()) {
		query.append(CONDITION_ALBUMID ")");
		params.push_back(album);
	}
	else if(!id.empty()) {
		query.append(CONDITION_FILEID ")");
		params.push_back(id);
		//Increment views.
	} 

	database.exec(query, &params);
	r.append("{}");
	return 0;
}

int Webapp::updateFile(RequestVars& vars, Response& r, SessionStore &session) {
	string file = vars["id"];
	string field = vars["field"];
	string v = vars["v"];
	QueryRow params;
	if(file.empty() || field.empty() || v.empty()) {
		Serializer s;
		s.append("msg", "INVALID_PARAM", 1);
		r.append(s.get(RESPONSE_TYPE_MESSAGE));
		return 0;
	}

	string query = replaceAll(UPDATE_FILE, "%s", field);

	params.push_back(v);
	params.push_back(file);
	Query q(query, &params);
	database.exec(&q);

	RequestVars data_vars;
	return getAlbums(data_vars, r, session);
}

int Webapp::updateAlbum(RequestVars& vars, Response& r, SessionStore &session) {
	string album = vars["id"];
	string field = vars["field"];
	string v = vars["v"];
	QueryRow params;
	if(album.empty() || field.empty() || v.empty()) {
		Serializer s;
		s.append("msg", "INVALID_PARAM", 1);
		r.append(s.get(RESPONSE_TYPE_MESSAGE));
		return 0;
	}

	string query = replaceAll(UPDATE_ALBUM, "%s", field);

	params.push_back(v);
	params.push_back(album);
	Query q(query, &params);
	database.exec(&q);

	RequestVars data_vars;
	return getAlbums(data_vars, r, session);
}

int Webapp::getBoth(RequestVars& vars, Response& r, SessionStore& s) {
	QueryRow params;
	Query q(SELECT_BOTH_DETAILS, &params);
	return getData(q, vars, r, s);
}

int Webapp::clearCache(RequestVars& vars, Response& r, SessionStore& session) {
	handlerLock.lock();
	int ref = parent_task->decrement_ref_count(); //decrement the ref count, and then wait for all other tasks to complete.
	parent_task->wait_for_all();
	refresh_templates();
	parent_task->increment_ref_count(); //increment the ref count (to allow proper removal of this task)
	parent_task->increment_ref_count(); //increment the ref count (to allow proper removal of this task)
	handlerLock.unlock();
	
	Serializer s;
	s.append("msg", "CACHE_CLEARED", 1);
	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	
	return 1;
}

int Webapp::refreshAlbums(RequestVars& vars, Response& r, SessionStore& session) {
	int nGenThumbs = GETCHK(vars["genthumbs"]);
	vector<string> albums;
	tokenize(vars["a"],albums,",");

	thread* refresh_albums = new thread([this, albums, nGenThumbs]() {
		std::unique_lock<std::mutex> lk(mutex_thread_start);
		while(currentID != this_thread::get_id() && !shutdown_handler)
			cv_thread_start.wait(lk);
		string storepath = database.select_single(SELECT_SYSTEM("store_path"));
		for(string album: albums) {
			QueryRow params;
			params.push_back(album);

			Query q(SELECT_ALBUM_PATH);
			database.select(&q, &params);
			if(!q.response->empty()) {
				QueryRow params;
				params.push_back(album);
				string path = q.response->at(0).at(0);
				string recursive = q.response->at(0).at(1);
				//Delete nonexistent paths stored in database.
				string existingFiles = DELETE_MISSING_FILES;
				list<string> files = FileSystem::GetFilesAsList(basepath + '/' + storepath + '/' + path, "", stoi(recursive));
				if(!files.empty()) {
					for (std::list<string>::const_iterator it = files.begin(), end = files.end(); it != end; ++it) {
						existingFiles.append("\"" + *it + "\",");
					}
					existingFiles = existingFiles.substr(0, existingFiles.size()-1);
				}
				existingFiles.append(")");
				database.exec(existingFiles, &params);
				//Get a list of files in the album from db.
				Query q_album(SELECT_PATHS_FROM_ALBUM);
				database.select(&q_album, &params);

				for(int i = 0; i < q_album.response->size(); i++) {
					vector<string> row = q_album.response->at(i);
					//For every row, look for a match in files. Remove from files when match found.
					for (std::list<string>::iterator it = files.begin(), end = files.end(); it != end;) {
						if(row.at(0) == *it) {
							it = files.erase(it);
						} else {
							++it;
						}
					}
				}
				addFiles(files, nGenThumbs, path, album);
			}
		}
	});
	process_thread(refresh_albums);

	Serializer s;
	rapidjson::Value v;
	s.append("msg", "REFRESH_SUCCESS", 0, &v);
	s.append("success", 1, 1, &v);
	r.append(s.get(RESPONSE_TYPE_MESSAGE));

	return 0;
}

int Webapp::search(RequestVars& vars, Response& r, SessionStore& s) {
	string query;
	string domain = vars["w"];
	string search_str = "%" + url_decode(vars["q"]) + "%";
	QueryRow params;
	if(domain == "both") {
		query = SELECT_SEARCH;
		params.push_back(search_str);
		params.push_back(search_str);
	}
	else if(domain == "albums") {
		query = SELECT_ALBUM_DETAILS CONDITION_ALBUM_NAME;
		params.push_back(search_str);
	}
	else return 0;
	
	Query q(query, &params);
	return getData(q, vars, r, s);

}

int Webapp::login(RequestVars& vars, Response& r, SessionStore& store) {
	string user = vars["user"];
	string pass = vars["pass"];
	Serializer s;
	if(user == params->get("user") && pass == params->get("pass")) {
		store.store("auth", "TRUE");
		rapidjson::Value v;
		s.append("msg", "LOGIN_SUCCESS", 0, &v);
		s.append("reload", 1, 1, &v);
	} else {
		s.append("msg", "LOGIN_FAILED", 1);
	}
	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	return 0;
}

int Webapp::logout(RequestVars& vars, Response& r, SessionStore& store) {
	string user = vars["user"];
	string pass = vars["pass"];
	Serializer s;
	
	store.destroy();
	rapidjson::Value v;
	s.append("msg", "LOGOUT_SUCCESS", 0, &v);
	s.append("reload", 1, 1, &v);

	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	return 0;
}

int Webapp::addBulkAlbums(RequestVars& vars, Response& r, SessionStore& session) {
	//This function ignores duplicates.
	vector<string> paths;
	tokenize(url_decode(vars["paths"]),paths,"\n");
	Response tmp; 
	Serializer s;
	for(string path: paths) {
		vars["path"] = vars["name"] = ref(path);

		if(addAlbum(vars, tmp, session) == 1) {
			//An error has occured!
			s.append("msg", "ADD_BULK_FAILED");
			return 1;
		}
	}
	rapidjson::Value v;
	s.append("msg", "ADDED_SUCCESS", 0, &v);
	s.append("success", 1, 1, &v);
	r.append(s.get(RESPONSE_TYPE_MESSAGE));

	return 0;
}

int Webapp::addAlbum(RequestVars& vars, Response& r, SessionStore&) {
	string type = vars["type"];
	if(!is_number(type))
		return 1;
	int nRecurse = GETCHK(vars["recursive"]);
	int nGenThumbs = GETCHK(vars["genthumbs"]);
	string name = url_decode(vars["name"]);
	string path = replaceAll(url_decode(vars["path"]), "\\", "/");
	int addStatus = 0;
	Serializer s;
	if(!name.empty() && !path.empty()) {
		//_addAlbum
		int nDuplicates = getDuplicates(name, path);
		if(nDuplicates > 0) {
			s.append("msg", "DUPLICATE_ALBUM");
			addStatus = 2;
		} else {
			thread* add_album = new thread([this, name, path, type, nRecurse, nGenThumbs]() {
				std::unique_lock<std::mutex> lk(mutex_thread_start);
				while(currentID != this_thread::get_id() && !shutdown_handler)
					cv_thread_start.wait(lk);
				
				string storepath = database.select_single(SELECT_SYSTEM("store_path"));
				//Add the album.
				QueryRow params;
				//get the date.
				const char* date = date_format("%Y%m%d",8);

				params.push_back(name);
				params.push_back(date);
				params.push_back(date);
				params.push_back(path);
				params.push_back(type);
				params.push_back(to_string(nRecurse));

				free((void*)date);
				int albumID = database.exec(INSERT_ALBUM, &params);
				vector<string> files = FileSystem::GetFiles(basepath + '/' + storepath + '/' + path, "", nRecurse);

				addFiles(files, nGenThumbs, path, to_string(albumID));

			});
			process_thread(add_album);
			rapidjson::Value v;
			s.append("msg", "ADDED_SUCCESS", 0, &v);
			s.append("success", 1, 1, &v);
		}
	} else {
		s.append("msg", "INVALID_PARAM", 1);
		addStatus = 1;
	}
	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	return addStatus;
}

int Webapp::delAlbums(RequestVars& vars, Response& r, SessionStore&) {
	Serializer s;

	int delThumbs = GETCHK(vars["delthumbs"]);
	int delFiles = GETCHK(vars["delfiles"]);
	vector<string> albums;
	tokenize(vars["a"],albums,",");

	thread* del_albums = new thread([this, albums, delThumbs, delFiles]() {
		std::unique_lock<std::mutex> lk(mutex_thread_start);
		while(currentID != this_thread::get_id() && !shutdown_handler)
			cv_thread_start.wait(lk);

		string storepath = database.select_single(SELECT_SYSTEM("store_path"));
		string thumbspath = database.select_single(SELECT_SYSTEM("thumbs_path"));
		for(string album: albums) {
			QueryRow params;

			params.push_back(album);
			Query delquery(SELECT_ALBUM_PATH);
			database.select(&delquery, &params);
			if(delquery.response->size() > 0) {

				string path = delquery.response->at(0).at(0);

				//Delete the album.
				database.exec(DELETE_ALBUM, &params);

				if(delFiles) {
					//Delete the albums' files.
					FileSystem::DeletePath(basepath + '/' + storepath + '/' + path);
				}
				if(delThumbs) {
					//Wait for thumb thread to finish.
					FileSystem::DeletePath(basepath + '/' + thumbspath + '/' + path);
				}
			}
		}
	});
	process_thread(del_albums);
	rapidjson::Value v;
	s.append("msg", "DELETE_SUCCESS", 0, &v);
	s.append("success", 1, 1, &v);
	r.append(s.get(RESPONSE_TYPE_MESSAGE));
	return 0;
}

int Webapp::getData(Query& query, RequestVars& vars, Response& r, SessionStore&) {
	Serializer serializer;

	if(!hasAlbums()) {
		serializer.append("msg", "NO_ALBUMS", 1);
		r.append(serializer.get(RESPONSE_TYPE_MESSAGE));
		return 0;
	}

	string limit = vars["limit"].empty() ? XSTR(DEFAULT_PAGE_LIMIT) : vars["limit"];
	int nLimit = stoi(limit);
	if(nLimit > MAX_PAGE_LIMIT) {
		limit = XSTR(DEFAULT_PAGE_LIMIT);
		nLimit = DEFAULT_PAGE_LIMIT;
	}

	string col = vars["by"].empty() ? "id" : vars["by"];

	int nPage = vars["page"].empty() ? 0 : stoi(vars["page"]);
	int page = nPage * nLimit;

	if(col == "rand") page = 0;

	query.params->push_back(to_string(page));
	query.params->push_back(limit);

	string order = (vars["order"] == "asc") ? "ASC" : "DESC" ;
	//TODO validate col
	
	if(col == "rand") {
		query.dbq->append(ORDER_DEFAULT DB_FUNC_RANDOM);
	} else {
		query.dbq->append(ORDER_DEFAULT + col + " " + order);
	}
	query.dbq->append(" LIMIT ?,? ");
	query.description = new QueryRow();
	query.response = new QueryResponse();
	database.select(&query);
	serializer.append(query);

	r.append(serializer.get(RESPONSE_TYPE_DATA));

	return 0;
}

int Webapp::getFiles(RequestVars& vars, Response& r, SessionStore&s) {
	string query = SELECT_FILE_DETAILS;
	QueryRow params;

	string album = vars["album"];
	string id = vars["id"];
	if(vars["showall"] != "1" && id.empty()) 
		query.append(CONDITION_FILE_ENABLED);


	if(!album.empty() && id.empty()) {
		query.append(CONDITION_ALBUMID);
		params.push_back(album);
		database.exec(INC_ALBUM_VIEWS, &params);
	}
	else if(!id.empty()) {
		string f = vars["f"];
		if(!f.empty()) {
			if(f == "next") query.append(CONDITION_N_FILE(">", "MIN"));
			else 
				if(f == "prev") query.append(CONDITION_N_FILE("<", "MAX"));
			params.push_back(id);
			Query q(SELECT_ALBUM_ID_WITH_FILE);
			database.select(&q, &params);
			if(!q.response->empty()) 
				params.push_back(q.response->at(0).at(0));
		} else {
			query.append(CONDITION_FILEID);
			params.push_back(id);
		}

		//Increment views.
		QueryRow incParams;
		incParams.push_back(id);
		database.exec(INC_FILE_VIEWS, &incParams);
	} 


	Query q(query, &params);
	return getData(q, vars, r, s);

}

int Webapp::getAlbums(RequestVars& vars, Response& r, SessionStore&s) {
	string query = SELECT_ALBUM_DETAILS;

	string id = vars["id"];
	QueryRow params;
	if(!id.empty()) {
		query.append(CONDITION_ALBUMID);
		params.push_back(id);
	}
	Query q(query, &params);
	return getData(q, vars, r, s);
}

int Webapp::setThumb(RequestVars& vars, Response& r, SessionStore&) {
	string type = vars["type"]+'s';
	string id = vars["id"];
	string path = vars["path"];

	QueryRow params;
	params.push_back(path);
	string thumbID = to_string(database.exec(INSERT_THUMB, &params));

	params.clear();
	params.push_back(thumbID);
	params.push_back(id);
	//Update the album/file thumb ID.
	string thumbquery = replaceAll(UPDATE_THUMB, "%s", thumbID);
	database.exec(thumbquery, &params);

	return 0;
}


void Webapp::addFile(const string& file, int nGenThumbs, const string& thumbspath, const string& path, const string& date, const string& albumID) {
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
int Webapp::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	string dupAlbumStr = database.select_single(SELECT_DUPLICATE_ALBUM_COUNT, &params);
	return stoi(dupAlbumStr);
}


int Webapp::hasAlbums() {
	string albumsStr = database.select_single(SELECT_ALBUM_COUNT, NULL, "0");
	int nAlbums = stoi(albumsStr);
	if(nAlbums == 0) {
		return 0;
	}
	return 1;
}



int Webapp::genThumb(const char* file, double shortmax, double longmax) {
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
*/