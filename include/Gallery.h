#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "Server.h"
#define HTML_HEADER "Content-type: text/html\r\n\r\n"
class Logging;
class Gallery : public ServerHandler, Internal {
	Logging* logger;
	Parameters* params;
public:
	Gallery::Gallery(Parameters* params, Logging* logger);
	tstring process(char** request);
};

#endif