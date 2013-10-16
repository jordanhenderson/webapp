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
/*
webapp_str_t* GenCookie(const char* name, const char* value, int days, std::vector<void*>* handler) {
	char* cookie = NULL;
	webapp_str_t* str = (webapp_str_t*) malloc (sizeof(webapp_str_t));
	time_t t; time(&t); add_days(t, days);

	const char* date_str = date_format("%a, %d-%b-%Y %H:%M:%S GMT", 29, &t, 1);
	int size = snprintf(NULL, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
	cookie = (char*)malloc(size + 1);
	snprintf(cookie, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
	str->data = cookie;
	str->len = size;

	free((void*)date_str);
	handler->push_back(cookie);
	handler->push_back(str);
	return str;
}*/

//Cleaned up by backend.
int GetSessionID(SessionStore* session, webapp_str_t* out) {
	if(session == NULL || out == NULL) return 0;
	out->data = session->sessionid.c_str();
	out->len = session->sessionid.length();
	return 1;
}

std::vector<std::string*>* StartRequestHandler() {
	return new std::vector<std::string*>();
}

void FinishRequest(Request* request, std::vector<string*>* handler) {
	if(request == NULL || handler == NULL) return;
	delete request->socket;
	free(request);

	for(std::vector<string*>::iterator it = handler->begin(); it != handler->end(); ++it) {
		free(*it);
	}

	delete handler;
}

TemplateDictionary* GetTemplate(Webapp* gallery, const char* page) {
	if(gallery != NULL) return gallery->getTemplate(page);
	else return NULL;
}


void RenderTemplate(Webapp* gallery, ctemplate::TemplateDictionary* dict, const char* page, std::vector<string*>* handler, webapp_str_t* out) {
	string* output = new string;
	ExpandTemplate(gallery->basepath + "/content/" + page, STRIP_WHITESPACE, dict, output);

	//Clean up the template dictionary.
	delete dict;
	
	out->data = output->c_str();
	out->len = output->length();

	handler->push_back(output);

}

const char* GetParam(Webapp* gallery, const char* param) {
	const string* val = &(gallery->params->get(param));
	if(val->empty()) return NULL;
	else return val->c_str();
}

void writeHandler(const std::error_code& error,  std::size_t bytes_transferred) {

}

void WriteData(asio::ip::tcp::socket* socket, char* data, int len) {
	*(int*)data = htons(len - 4);
	asio::write(*socket, asio::buffer(data, len));
}
