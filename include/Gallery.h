#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#define HTML_HEADER "Content-type: text/html\r\n\r\n"
#define CSS_HEADER "Content-type: text/css; charset=UTF-8\r\n\r\n"
#define JSON_HEADER "Content-type: application/json\r\n\r\n"
#define JS_HEADER "Content-type: application/javascript\r\n\r\n"
#define HTML_404 "The page you requested cannot be found (404)."
class Logging;
class Gallery : public ServerHandler, Internal {
	Logging* logger;
	Parameters* params;
	Parameters* filecache;
	tstring getIndex();
	tstring loadFile(char* file);
public:
	Gallery::Gallery(Parameters* params, Logging* logger);
	void process(FCGX_Request* request);
};

#endif