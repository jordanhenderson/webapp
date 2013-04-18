// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"
#include "fcgiapp.h"

Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
	this->params = params;
}

tstring Gallery::getIndex() {
	tstring data = HTML_HEADER;
	File* f = FileSystem::Open("gallery/templates/index.html", "rb");
	data.append(FileSystem::Read(f));
	FileSystem::Close(f);
	return data;
}

tstring Gallery::process(char** request) {

	char* method = FCGX_GetParam("REQUEST_METHOD", request);
	char* uri = FCGX_GetParam("REQUEST_URI", request);
	if(strcmp(method, "GET") == 0) {
		if(strcmp(uri, "/") == 0) {
			return getIndex();
		} else {
			//Return the file if it exists. Else return 404.
			tstring fileuri = params->get("basepath") + tstring(uri);
			tstring data = HTML_HEADER;
			if(FileSystem::Exists(fileuri)) {
				File* f = FileSystem::Open(fileuri.c_str(), "rb");
				data.append(FileSystem::Read(f));
				FileSystem::Close(f);
			} else {
				data.append(HTML_404);
				
			}
			return data;
		}
	}

	return "";
}

