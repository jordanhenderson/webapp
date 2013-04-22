// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"
#include "fcgiapp.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "sqlite3.h"

using namespace rapidjson;
using namespace std;
Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
	this->params = params;
	this->filecache = new Parameters();
}

string Gallery::getIndex() {
	return loadFile("/templates/index.html");
}

string Gallery::loadFile(char* uri) {
	std::string fileuri = params->get("basepath") + uri;
	std::string data;

		if(FileSystem::Exists(fileuri.c_str())) {
			data = HTML_HEADER;

			if(endsWith(uri, ".css"))
				data = CSS_HEADER;
			else if(endsWith(uri, ".js"))
				data = JS_HEADER;
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

string Gallery::response(char* data, int type, int close) {
	Document document;
	document.SetObject();

	document.AddMember("type", type, document.GetAllocator());
	document.AddMember("data", data, document.GetAllocator());
	document.AddMember("close", close, document.GetAllocator());
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	document.Accept(writer);
	return JSON_HEADER + std::string(buffer.GetString(), buffer.Size());
}

void Gallery::process(FCGX_Request* request) {
	char* method = FCGX_GetParam("REQUEST_METHOD", request->envp);
	char* uri = FCGX_GetParam("REQUEST_URI", request->envp);
	std::string final;
	if(strcmp(method, "GET") == 0) {
		if(strcmp(uri, "/") == 0) {
			final = getIndex();
		} else if(strcmp(uri, "/load") == 0) {
			final = response(NO_ALBUMS_LINK, RESPONSE_TYPE_FULLMSG);
			
		} else {
			//Return the file if it exists. Else return 404.
			final = loadFile(uri);
		}
	} else if(strcmp(method, "POST") == 0) {
		final = "BLAH";
	}

	FCGX_PutStr(final.c_str(), final.length(), request->out);
}

