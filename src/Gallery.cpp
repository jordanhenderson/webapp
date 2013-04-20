// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"
#include "fcgiapp.h"

Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
	this->params = params;
	this->filecache = new Parameters();
}

tstring Gallery::getIndex() {
	return loadFile("/templates/index.html");
}

tstring Gallery::loadFile(char* uri) {
	tstring fileuri = params->get("basepath") + uri;
	tstring data = HTML_HEADER;
	
	if(endsWith(uri, ".css"))
		data = CSS_HEADER;
	else if(endsWith(uri, ".js"))
		data = JS_HEADER;

		if(FileSystem::Exists(fileuri)) {
			File* f = FileSystem::Open(fileuri.c_str(), "rb");
			FileData* filedata = FileSystem::Read(f);
			data.append(filedata->data, filedata->size);
			FileSystem::Close(f);
			delete filedata;
			delete f;
		} else {
			
			data.append(HTML_404);
		}
	
	return data;
}

void Gallery::process(FCGX_Request* request) {
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
	tstring final;
	if(strcmp(method, "GET") == 0) {
		if(strcmp(uri, "/") == 0) {
			final = getIndex();
		} else if(strcmp(uri, "/load") == 0) {
			tstring data = JSON_HEADER;
			final = data;
		} else {
			//Return the file if it exists. Else return 404.
			final = loadFile(uri);
		}
	} else if(strcmp(method, "POST") == 0) {
		final = "BLAH";
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
}

