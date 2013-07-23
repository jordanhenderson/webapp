#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#include "Database.h"
#include "rapidjson.h"
#include "Session.h"
#include "Serializer.h"
#include "document.h"

#define TEMPLATE_VIDEO "video.html"
#define TEMPLATE_IMAGE "image.html"
#define TEMPLATE_FLASH "flash.html"
#define TEMPLATE_TEXT "text.html"
#define DEFAULT_THUMB "default.png"
#define ALBUM_SET 0
#define ALBUM_RANDOM 1
#define SLIDESHOW_SET_PR 1
#define SLIDESHOW_RAND_PR 9

#define HTTP_NO_AUTH "Status: 401 Unauthorized\r\n\r\nUnauthorised access."
#define RESPONSE_TYPE_DATA 0
#define RESPONSE_TYPE_MESSAGE 1
#define RESPONSE_TYPE_FULL_MESSAGE 2

//QUERY DEFINTIONS
#define SELECT_RANDOM_ALBUMIDS "SELECT id FROM albums WHERE type = " XSTR(ALBUM_RANDOM) ";"
#define SELECT_SET_ALBUMIDS "SELECT id FROM albums WHERE type = " XSTR(ALBUM_SET) ";"
#define SELECT_ENABLED_FILES "SELECT id FROM files WHERE albumid = ? AND enabled = 1;"
#define SELECT_DUPLICATE_ALBUM_COUNT "SELECT COUNT(*) FROM albums WHERE name = ? OR path = ?;"
#define SELECT_FILENAME "SELECT filename FROM files WHERE id = ?;"
#define SELECT_ALBUM_FILE "SELECT id, fileid FROM albumfiles WHERE albumid = ?;"
#define SELECT_FILE_THUMBID "SELECT thumbid FROM files WHERE id = ?;"
#define SELECT_ALBUM_PATH_THUMB "SELECT path, thumbid FROM albums WHERE id = ?;"
#define SELECT_ALBUM_COUNT "SELECT COUNT(*) FROM albums;"

#define SELECT_SYSTEM(var) "SELECT value FROM system WHERE name=\"" var "\""
#define INSERT_ALBUM "INSERT INTO albums (name, added, lastedited, path, type, recursive) VALUES (?,?,?,?,?,?);"
#define INSERT_FILE "INSERT INTO files (name, path, added, thumbid) VALUES (?,?,?,?);"
#define INSERT_FILE_NO_THUMB "INSERT INTO files (name, path, added) VALUES (?,?,?);"
#define INSERT_ALBUM_FILE "INSERT INTO albumfiles (albumid, fileid) VALUES (?,?);"
#define INSERT_THUMB "INSERT INTO thumbs(path) VALUES (?);"
#define UPDATE_THUMB "UPDATE %s SET thumbid = ? WHERE id = ?;"

#define DELETE_ALBUM "DELETE FROM albums WHERE id = ?;"

#define SELECT_FILE_DETAILS "SELECT f.id AS id, al.id as aid, f.name, f.rating as rating, f.views as views, \
(SELECT (" SELECT_SYSTEM("store_path") ") || " XSTR(PSEP) " || al.path || " XSTR(PSEP) " || f.path) AS path, \
	COALESCE(\
		(SELECT (" SELECT_SYSTEM("thumbs_path") ") || " XSTR(PSEP) " || th.path FROM thumbs th JOIN files ON files.thumbid = th.id AND files.id = f.id), \
		(SELECT (" SELECT_SYSTEM("thumbs_path") ") || " XSTR(PSEP) " || (" SELECT_SYSTEM("default_thumb") "))) \
AS thumb FROM files f JOIN albumfiles alf ON f.id=alf.fileID JOIN albums al ON al.id=alf.albumid WHERE 1 "

#define CONDITION_FILE_ENABLED " AND f.enabled = 1 "

#define TOGGLE_FILES "UPDATE files SET enabled = 1 - enabled WHERE id IN (SELECT f.id FROM files f JOIN albumfiles alf ON alf.fileid = f.id JOIN albums al ON alf.albumid = al.id WHERE 1 "

#define ORDER_DEFAULT " ORDER BY "

#define CONDITION_SEARCH " AND f.id IN (SELECT f.id FROM files f WHERE f.name LIKE ?) "

#define CONDITION_FILEID " AND f.id = ? "

#define CONDITION_ALBUM " AND al.id = ? "

#define INC_FILE_VIEWS "UPDATE files SET views = views + 1 WHERE id = ?"
#define INC_ALBUM_VIEWS "UPDATE albums SET views = views + 1 WHERE id = ?"

#define CONDITION_FILE_GROUPED " AND al.type = " XSTR(ALBUM_RANDOM) " OR al.type = " XSTR(ALBUM_SET) \
" AND f.id IN (SELECT fileid FROM albumfiles WHERE albumid=al.id ORDER BY id DESC LIMIT 1) "


#define SELECT_ALBUM_DETAILS "SELECT al.id AS id, name, added, lastedited, type, rating, recursive, views, al.path AS path, \
	COALESCE(\
		(SELECT (" SELECT_SYSTEM("thumbs_path") ") || " XSTR(PSEP) " || th.path FROM thumbs th JOIN albums ON albums.thumbid = th.id AND albums.id = al.id),\
		(SELECT (" SELECT_SYSTEM("thumbs_path") ") || " XSTR(PSEP) " || th.path FROM thumbs th JOIN files f ON f.thumbid = th.id JOIN albumfiles alf ON alf.fileid=f.id AND alf.id IN \
			(SELECT id FROM albumfiles WHERE albumid=al.id ORDER BY id ASC LIMIT 1) JOIN albums ON albums.id = al.id),\
		(SELECT (" SELECT_SYSTEM("thumbs_path") ") || " XSTR(PSEP) " || (" SELECT_SYSTEM("default_thumb") "))) AS thumb FROM albums al WHERE 1 "

#define SELECT_ALBUM_PATH "SELECT path, recursive FROM albums WHERE id = ?;"

#define DELETE_MISSING_FILES "DELETE FROM files f JOIN albumfiles alf ON alf.fileid = f.id WHERE alf.albumid = ? AND albumpath NOT IN ("

#define PRAGMA_FOREIGN "PRAGMA foreign_keys = ON;"

#define SELECT_PATHS_FROM_ALBUM "SELECT path FROM files JOIN albumfiles ON albumfiles.fileid = files.id WHERE albumid = ?;"

#define DEFAULT_PAGE_LIMIT 32


#define CONDITION_N_FILE(op) " AND f.id = (SELECT fileid FROM albumfiles WHERE id = (SELECT alf.id FROM albumfiles alf JOIN files f ON f.id = fileid WHERE fileid " op " ? AND f.enabled = 1 LIMIT 1))"

#define SELECT_ALBUM_ID_WITH_FILE "SELECT al.id FROM albums al JOIN albumfiles alf ON alf.albumid = al.id JOIN files f ON alf.fileid = f.id WHERE f.id = ?"

/* DBSPEC
CREATE TABLE "albumfiles" ("id" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , "albumid" INTEGER NOT NULL REFERENCES albums(id) ON DELETE CASCADE, "fileid" INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE )
CREATE TABLE "albums" ("id" INTEGER PRIMARY KEY  NOT NULL ,"name" TEXT,"added" DATETIME,"lastedited" DATETIME,"path" TEXT NOT NULL ,"type" INTEGER NOT NULL ,"thumbid" INTEGER REFERENCES thumbs(id) ON DELETE SET NULL,"rating" INTEGER NOT NULL  DEFAULT (0) ,"recursive" INTEGER NOT NULL , "views" INTEGER DEFAULT 0)
CREATE TABLE "files" ("id" INTEGER PRIMARY KEY  NOT NULL,"name" TEXT,"path" TEXT,"added" TEXT,"enabled" INTEGER NOT NULL  DEFAULT (1) ,"thumbid" INTEGER REFERENCES thumbs(id) ON DELETE SET NULL,"rating" INTEGER DEFAULT(0), "views" INTEGER DEFAULT 0, "lastedited" TEXT)
CREATE TABLE "system" ("id" INTEGER PRIMARY KEY  AUTOINCREMENT  NOT NULL , "name" TEXT NOT NULL , "value" TEXT)
CREATE TABLE "thumbs" ("id" INTEGER PRIMARY KEY  NOT NULL , "path" TEXT)
CREATE TRIGGER "file_cleanup" AFTER DELETE ON "albums" BEGIN DELETE FROM files WHERE id NOT IN (SELECT fileid FROM albumfiles); END
CREATE TRIGGER "thumbs_trigger_files" AFTER DELETE ON "files" BEGIN DELETE FROM thumbs WHERE id NOT IN (SELECT thumbid FROM files f WHERE thumbid NOT NULL UNION SELECT thumbid FROM albums WHERE thumbid NOT NULL); END
CREATE TRIGGER "thumbs_trigger_albums" AFTER DELETE ON "albums" BEGIN DELETE FROM thumbs WHERE id NOT IN (SELECT thumbid FROM files f WHERE thumbid NOT NULL UNION SELECT thumbid FROM albums WHERE thumbid NOT NULL); END
*/


#define GETSPATH(x) basepath + PATHSEP + storepath + PATHSEP + x
#define GALLERYMAP(m) 	m["addAlbum"] = &Gallery::addAlbum; \
	m["getAlbums"] = &Gallery::getAlbums; \
	m["delAlbums"] = &Gallery::delAlbums; \
	m["addBulkAlbums"] = &Gallery::addBulkAlbums; \
	m["login"] = &Gallery::login; \
	m["setThumb"] = &Gallery::setThumb; \
	m["getFiles"] = &Gallery::getFiles; \
	m["search"] = &Gallery::search; \
	m["refreshAlbums"] = &Gallery::refreshAlbums; \
	m["disableFiles"] = &Gallery::disableFiles;
#define GETCHK(s) s.empty() ? 0 : 1
typedef std::unordered_map<std::string, std::string> RequestVars;
typedef std::unordered_map<std::string, std::string> CookieVars;
typedef std::string Response;
typedef int(Gallery::*GallFunc)(RequestVars&, Response&, SessionStore&);
class Logging;

class Gallery : public ServerHandler, Internal {
private:
	
	Parameters* params;
	std::string getPage(const char* page);
	std::string loadFile(const std::string& file);
	Database* database;
	Session* session;
	RequestVars parseRequestVariables(char* vars, RequestVars& v);
	Response processVars(RequestVars&, SessionStore&, int publishSession=0);
	std::string user;
	std::string pass;
	std::string basepath;
	std::string dbpath;
	int auth;

	int genThumb(const char* file, double shortmax, double longmax);
	int getDuplicates( std::string& name, std::string& path );
	std::string genCookie(const std::string& name, const std::string& value, time_t* date=NULL);
	CookieVars parseCookies(const char* cookies);
	std::map<std::string, GallFunc> m;
	void createFieldMap(Query& q, rapidjson::Value& v);
	int hasAlbums();
	int getData(Query& query, RequestVars&, Response&, SessionStore&);
	template <typename T>
	void addFiles(T& files, int nGenThumbs, const std::string& path, const std::string& albumID) {
		std::string date = date_format("%Y%m%d",8);
		std::string storepath = database->select(SELECT_SYSTEM("store_path")).response->at(0).at(0);
		std::string thumbspath = database->select(SELECT_SYSTEM("thumbs_path")).response->at(0).at(0);
		if(files.size() > 0) {
			for (T::const_iterator it = files.begin(), end = files.end(); it != end; ++it) {

				string thumbID;
				//Generate thumb.
				if(nGenThumbs) {
					FileSystem::MakePath(basepath + PATHSEP + thumbspath + PATHSEP + path + PATHSEP + *it);
					if(genThumb((path + PATHSEP + *it).c_str(), 200, 200) == ERROR_SUCCESS) {
						QueryRow params;
						params.push_back(path + PATHSEP + *it);
						int nThumbID = database->exec(INSERT_THUMB, &params);
						if(nThumbID > 0) {
							thumbID = to_string(nThumbID);
						}
					}
				}
				//Insert file 
				QueryRow params;
				params.push_back(*it);
				params.push_back(*it);
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
		}
	}



public:
	Gallery::Gallery(Parameters* params);
	Gallery::~Gallery();
	void process(FCGX_Request* request);
	

	//Main response functions.
	int getAlbums(RequestVars&, Response&, SessionStore&);
	int addAlbum(RequestVars&, Response&, SessionStore&);
	int addBulkAlbums(RequestVars&, Response&, SessionStore&);
	int delAlbums(RequestVars&, Response&, SessionStore&);
	int login(RequestVars&, Response&, SessionStore&);
	int setThumb(RequestVars&, Response&, SessionStore&);
	int getFiles(RequestVars&, Response&, SessionStore&);
	int search(RequestVars&, Response&, SessionStore&);
	int refreshAlbums(RequestVars&, Response&, SessionStore&);
	int disableFiles(RequestVars&, Response&, SessionStore&);
};

#endif