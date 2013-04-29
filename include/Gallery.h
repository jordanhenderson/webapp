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

//TODO move these into client


#define DUPLICATE_ALBUM "An album with the specified name or path already exists. Please try again."
#define ALBUM_ADDED_SUCCESS "Album added successfully!"
#define ALBUMS_ADDED_SUCCESS "Albums added successfully!"
#define MISSING_FIELD "Please ensure all required fields are entered correctly."
#define ALBUM_NOT_EXISTS "The album path specified does not exist. Please try again."

#define RESPONSE_TYPE_DATA 0
#define RESPONSE_TYPE_TABLE 1



typedef std::unordered_map<std::string, std::string> RequestVars;
class Logging;
class Gallery : public ServerHandler, Internal {
private:
	std::shared_ptr<Logging> logger;
	std::shared_ptr<Parameters> params;
	std::string getPage(const char* page);
	std::string loadFile(const char* file);
	std::unique_ptr<Database> database;
	RequestVars parseRequestVariables(char* vars);
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
	std::string getAlbums();
	std::string getAlbumsTable();
	int getDuplicateAlbums(char* name, char* path);
	std::vector<std::string> getRandomFileIds();
	std::vector<std::string> getSetIds();
	std::string getFilename(int);
	int genThumb(char* file, int shortmax, int longmax);
};

#endif