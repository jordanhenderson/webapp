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
#define RESPONSE_TYPE_TABLE 1
#define RESPONSE_TYPE_MESSAGE 2
#define RESPONSE_TYPE_FULL_MESSAGE 3

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
#define SELECT_ALBUM_DETAILS "SELECT id, name, added, lastedited, path, type, rating, recursive, thumbid AS thumb FROM albums LIMIT ?;"
#define SELECT_FIRST_FILE "SELECT fileid FROM albumfiles WHERE albumid = ? ORDER BY id ASC LIMIT 1;"
#define SELECT_FILE_PATH "SELECT path FROM files WHERE id = ?;"
#define SELECT_THUMB_PATH "SELECT path FROM thumbs WHERE id = ?;"

#define INSERT_ALBUM "INSERT INTO albums (name, added, lastedited, path, type, recursive) VALUES (?,?,?,?,?,?);"
#define INSERT_FILE "INSERT INTO files (name, path, added) VALUES (?,?,?);"
#define INSERT_ALBUM_FILE "INSERT INTO albumfiles (albumid, fileid) VALUES (?,?);"
#define INSERT_THUMB "INSERT INTO thumbs(path) VALUES (?);"
#define UPDATE_THUMB "UPDATE %s SET thumbid = ? WHERE id = ?;"

#define DELETE_THUMB "DELETE FROM thumbs WHERE id = ?;"
#define DELETE_FILE "DELETE FROM files WHERE id = ?;"
#define DELETE_ALBUM_FILE "DELETE FROM albumfiles WHERE id = ?;"
#define DELETE_ALBUM "DELETE FROM albums WHERE id = ?;"

#define DEFAULT_PAGE_LIMIT 25



#define GETSPATH(x) basepath + PATHSEP + storepath + PATHSEP + x
#define GALLERYMAP(m) 	m["addAlbum"] = &Gallery::addAlbum; \
	m["getAlbums"] = &Gallery::getAlbums; \
	m["delAlbums"] = &Gallery::delAlbums; \
	m["addBulkAlbums"] = &Gallery::addBulkAlbums; \
	m["login"] = &Gallery::login; \
	m["setThumb"] = &Gallery::setThumb;
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
	std::string storepath;
	std::string dbpath;
	std::string thumbspath;
	int auth;

	std::vector<std::string> getRandomFileIds();
	std::vector<std::string> getSetIds();
	std::string getFilename(int);
	int genThumb(const char* file, double shortmax, double longmax);
	int getDuplicates( std::string& name, std::string& path );
	std::string genCookie(const std::string& name, const std::string& value, time_t* date=NULL);
	CookieVars parseCookies(const char* cookies);
	std::map<std::string, GallFunc> m;
	void getData(Query&, Serializer&, int thumbrow);
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
};

#endif