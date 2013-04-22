#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#define HTML_HEADER "Content-type: text/html\r\n\r\n"
#define CSS_HEADER "Content-type: text/css; charset=UTF-8\r\n\r\n"
#define JSON_HEADER "Content-type: application/json\r\n\r\n"
#define JS_HEADER "Content-type: application/javascript\r\n\r\n"
#define HTML_404 "Status: 404 Not Found\r\n\r\nThe page you requested cannot be found (404)."

#define TEMPLATE_VIDEO "video.html"
#define TEMPLATE_IMAGE "image.html"
#define TEMPLATE_FLASH "flash.html"
#define TEMPLATE_TEXT "text.html"
#define STORE_PATH "store"
#define THUMBS_PATH "thumbs"
#define DEFAULT_THUMB "default.png"
#define ALBUM_SET 0
#define ALBUM_RANDOM 1
#define SLIDESHOW_SET_PR 1
#define SLIDESHOW_RAND_PR 9
#define NO_ALBUMS_LINK "<h1>You do not have any albums. <a href='../manage/'>Add some!</a></h1>"
#define NO_ALBUMS_POPUP "<h1>You do not have any albums. <a href='#' class='popup' data-form='addalbum_f'>Add some!</a></h1>"
#define DUPLICATE_ALBUM "An album with the specified name or path already exists. Please try again."
#define ALBUM_ADDED_SUCCESS "Album added successfully!"
#define ALBUMS_ADDED_SUCCESS "Albums added successfully!"
#define MISSING_FIELD "Please ensure all required fields are entered correctly."
#define ALBUM_NOT_EXISTS "The album path specified does not exist. Please try again."
#define THUMB_EXTENSIONS (".png", ".gif", ".jpg", ".jpeg")
#define RESPONSE_TYPE_FULLMSG 0
#define RESPONSE_TYPE_SMLMSG 1
#define RESPONSE_TYPE_DATA 2
#define RESPONSE_TYPE_TABLE 3




class Logging;
class Gallery : public ServerHandler, Internal {
	Logging* logger;
	Parameters* params;
	Parameters* filecache;
	std::string getIndex();
	std::string loadFile(char* file);
	std::string response(char* data, int type, int close=0);
public:
	Gallery::Gallery(Parameters* params, Logging* logger);
	void process(FCGX_Request* request);
};

#endif