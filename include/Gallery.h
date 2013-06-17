#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#include "Database.h"
#include "rapidjson.h"
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

#define HTTP_NO_AUTH "Status: 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Gallery\"\r\n\r\n"
#define RESPONSE_TYPE_DATA 0
#define RESPONSE_TYPE_TABLE 1
#define RESPONSE_TYPE_MESSAGE 2
#define RESPONSE_TYPE_FULL_MESSAGE 3
#define GETSPATH(x) basepath + PATHSEP + storepath + PATHSEP + x
#define GALLERYMAP(m) 	m["addAlbum"] = &Gallery::addAlbum; \
	m["getAlbums"] = &Gallery::getAlbums; \
	m["delAlbums"] = &Gallery::delAlbums; \
	m["addBulkAlbums"] = &Gallery::addBulkAlbums;
#define GETCHK(s) s.empty() ? 0 : 1
typedef std::unordered_map<std::string, std::string> RequestVars;
typedef std::string Response;
typedef int(Gallery::*GallFunc)(RequestVars&, Response&);
class Logging;

class Gallery : public ServerHandler, Internal {
private:
	std::shared_ptr<Logging> logger;
	std::shared_ptr<Parameters> params;
	std::string getPage(const char* page);
	std::string loadFile(const char* file);
	Database* database;
	RequestVars parseRequestVariables(char* vars, RequestVars& v);
	std::string processVars(RequestVars&);
	std::string user;
	std::string pass;
	std::string basepath;
	std::string storepath;
	std::string dbpath;
	std::string thumbspath;
	int auth;
public:
	Gallery::Gallery(std::shared_ptr<Parameters>& params, std::shared_ptr<Logging>& logger);
	Gallery::~Gallery();
	void process(FCGX_Request* request);
	int getDuplicateAlbums(const char* name, const char* path);
	std::vector<std::string> getRandomFileIds();
	std::vector<std::string> getSetIds();
	std::string getFilename(int);
	int genThumb(const char* file, double shortmax, double longmax);
	int getDuplicates( std::string& name, std::string& path );

	//Main response functions.
	int getAlbums(RequestVars& vars, Response&);
	int addAlbum(RequestVars& vars, Response&);
	int addBulkAlbums(RequestVars& vars, Response&);
	int delAlbums(RequestVars& vars, Response&);
};

#endif