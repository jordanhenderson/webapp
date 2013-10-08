#include "Platform.h"
#include "Webapp.h"

extern "C" {
#include "Hooks.h"
}
using namespace ctemplate;
using namespace std;
int Template_ShowGlobalSection(TemplateDictionary* dict, const char* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowTemplateGlobalSection(section);
	return 0;
}

int Template_ShowSection(TemplateDictionary* dict, const char* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowSection(section);
	return 0;
}

//Cleaned up by backend.
const char* GetSessionValue(SessionStore* session, const char* key) {
	if(session == NULL || key == NULL) 
		return NULL;
	const string* s = &session->get(key);
	if(s->empty()) return NULL;
	return s->c_str();
}

//Cleaned up by backend.
SessionStore* GetSession(Sessions* sessions, const char* sessionid) {
	return sessions->get_session(sessionid);
}

//Cleaned up by backend.
SessionStore* NewSession(Sessions* sessions, const char* host, const char* user_agent) {
	return sessions->new_session(host, user_agent);
}

int Template_SetValue(TemplateDictionary* dict, const char* key, const char* value) {
	dict->SetValue(key, value);
	return 0;
}

//Cleaned up by backend.
Request* GetNextRequest(tbb::concurrent_bounded_queue<Request*>* requests) {
	Request* request = NULL;
	requests->pop(request);
	return request;

}


size_t StringLen(const char* str) {
	if(str == NULL) return NULL;
	return strlen(str);
}

//Generate a Set-Cookie header provided name, value and date.
//Cleaned up by request handler.
const char* GenCookie(const char* name, const char* value, int days, std::vector<void*>* handler) {
	char* cookie = NULL;
	time_t t; time(&t); add_days(t, days);

	const char* date_str = date_format("%a, %d-%b-%Y %H:%M:%S GMT", 29, &t, 1);
	int size = snprintf(NULL, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
	cookie = (char*)malloc(size + 1);
	snprintf(cookie, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
	free((void*)date_str);
	handler->push_back(cookie);
	return cookie;
}

//Cleaned up by backend.
const char* GetSessionID(SessionStore* session) {
	return session->sessionid.c_str();
}

std::vector<void*>* StartRequestHandler(Request* request) {
	std::vector<void*>* gc = new std::vector<void*>();
	gc->push_back(request);
	return gc;
}

void FinishRequestHandler(std::vector<void*>* handler) {
	Request* request = (Request*)handler->at(0);
	request->socket->close();
	free(request);

	for(std::vector<void*>::iterator it = handler->begin() + 1; it != handler->end(); ++it) {
		free(*it);
	}
	delete handler;
}

TemplateDictionary* GetTemplate(Webapp* gallery, const char* page) {
	if(gallery != NULL) return gallery->getTemplate(page);
	else return NULL;
}


const char* RenderTemplate(Webapp* gallery, ctemplate::TemplateDictionary* dict, const char* page, std::vector<void*>* handler) {
	string output;
	ExpandTemplate(gallery->basepath + "/content/" + page, STRIP_WHITESPACE, dict, &output);
	char* content = (char*) malloc (output.size() + 1);
	if(content != NULL) {
		memcpy(content, output.c_str(), output.size() + 1);
		handler->push_back(content);
	}
	delete dict;
	return content;
}

const char* GetParam(Webapp* gallery, const char* param) {
	const string* val = &(gallery->params->get(param));
	if(val->empty()) return NULL;
	else return val->c_str();
}

int GetScript(Webapp* gallery, const char* filename, script_t* out) {
	for(LuaChunk c: gallery->loadedScripts) {
		if(c.filename == filename) {
			out->data = c.bytecode.c_str();
			out->len = c.bytecode.size();
			return 1;
		}
	}
	return 0;
}

void writeHandler(const std::error_code& error,  std::size_t bytes_transferred) {

}

void WriteData(asio::ip::tcp::socket* socket, char* data, int len) {
	*(int*)data = htons(len - 4);
	asio::async_write(*socket, asio::buffer(data, len), writeHandler);
}
