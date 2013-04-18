#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#define HTML_HEADER "Content-type: text/html\r\n\r\n"
#define HTML_404 "The page you requested cannot be found (404)."
class Logging;
class Gallery : public ServerHandler, Internal {
	Logging* logger;
	Parameters* params;
	tstring getIndex();
public:
	Gallery::Gallery(Parameters* params, Logging* logger);
	tstring process(char** request);
};

#endif