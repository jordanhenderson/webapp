#ifndef GALLERY_H
#define GALLERY_H
#include "Platform.h"
#include "Parameters.h"
#include "HttpServer.h"
class Logging;
class Gallery : public ServerHandler, Internal {
	Logging* logger;
	Parameters* params;
	int port;
	tstring host;
public:
	Gallery::Gallery(Parameters* params);
	void setLog(tstring file);
	inline Logging* getLogger();
	void process();
};

#endif