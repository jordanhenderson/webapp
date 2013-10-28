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
	if (sessions == NULL || sessionid == NULL) return NULL;
	return sessions->get_session(sessionid);
}

//Cleaned up by backend.
SessionStore* NewSession(Sessions* sessions, const char* host, const char* user_agent) {
	if (sessions == NULL || host == NULL || user_agent == NULL) return NULL;
	return sessions->new_session(host, user_agent);
}

int Template_SetValue(TemplateDictionary* dict, const char* key, const char* value) {
	if (dict == NULL || key == NULL || value == NULL) return 1;
	dict->SetValue(key, value);
	return 0;
}

//Cleaned up by backend.
Request* GetNextRequest(tbb::concurrent_bounded_queue<Request*>* requests) {
	if (requests == NULL) return NULL;
	Request* request = NULL;
	requests->pop(request);
	return request;

}

void QueueProcess(Webapp* app, webapp_str_t* func, webapp_str_t* vars) {
	if (func == NULL || vars == NULL || app == NULL) return;
	Process* p = new Process();
	p->func = webapp_strdup(func);
	p->vars = webapp_strdup(vars);
	app->background_queue.push(p);

}

Process* GetNextProcess(Webapp* app) {
	if (app == NULL) return NULL;
	Process* p = NULL;
	app->background_queue.pop(p);
	return p;
}

int GetSessionID(SessionStore* session, webapp_str_t* out) {
	if(session == NULL || out == NULL) return 0;
	out->data = session->sessionid.c_str();
	out->len = session->sessionid.length();
	return 1;
}

void FinishRequest(Request* request) {
	if(request == NULL) return;


	for(std::vector<string*>::iterator it = request->handler->begin(); it != request->handler->end(); ++it) {
		free(*it);
	}

	delete request;
}

TemplateDictionary* GetTemplate(Webapp* gallery, const char* page) {
	if(gallery != NULL) return gallery->getTemplate(page);
	else return NULL;
}


void RenderTemplate(Webapp* gallery, ctemplate::TemplateDictionary* dict, const char* page, std::vector<string*>* handler, webapp_str_t* out) {
	if (gallery == NULL || dict == NULL || page == NULL || handler == NULL || out == NULL) return;
	string* output = new string;
	ExpandTemplate(gallery->basepath + "/content/" + page, STRIP_WHITESPACE, dict, output);

	//Clean up the template dictionary.
	delete dict;
	
	out->data = output->c_str();
	out->len = output->length();

	handler->push_back(output);

}

const char* GetParam(Webapp* gallery, const char* param) {
	if (gallery == NULL || param == NULL) return NULL;
	const string* val = &(gallery->params->get(param));
	if(val->empty()) return NULL;
	else return val->c_str();
}

void WriteData(asio::ip::tcp::socket* socket, char* data, int len) {
	if (socket == NULL || data == NULL) return;
	*(int*)data = htons(len - 4);
	try {
		asio::write(*socket, asio::buffer(data, len));
	}
	catch (system_error ec) {
		printf("Error writing to socket!");
	}
}

