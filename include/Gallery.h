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

#define SELECT_DETAILS_END " ORDER BY id DESC LIMIT ?;"

#define CONDITION_SEARCH " AND f.id IN (SELECT f.id FROM files f WHERE f.name LIKE ?) "

#define CONDITION_FILEID " AND f.id IN (?) "

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

#define DELETE_FILES "DELETE FROM albumfiles WHERE fileid IN (?); DELETE FROM files WHERE id IN (?);" 

#define PRAGMA_FOREIGN "PRAGMA foreign_keys = ON;"

#define DEFAULT_PAGE_LIMIT 32

#define GETSPATH(x) basepath + PATHSEP + storepath + PATHSEP + x
#define GALLERYMAP(m) 	m["addAlbum"] = &Gallery::addAlbum; \
	m["getAlbums"] = &Gallery::getAlbums; \
	m["delAlbums"] = &Gallery::delAlbums; \
	m["addBulkAlbums"] = &Gallery::addBulkAlbums; \
	m["login"] = &Gallery::login; \
	m["setThumb"] = &Gallery::setThumb; \
	m["getFiles"] = &Gallery::getFiles; \
	m["search"] = &Gallery::search; \
	m["refreshAlbums"] = &Gallery::refreshAlbums;
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
};

#endif