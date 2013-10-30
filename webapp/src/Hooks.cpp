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
int GetSessionValue(SessionStore* session, const char* key, webapp_str_t* out) {
	if(session == NULL || key == NULL) 
		return NULL;

	const string* s = &session->get(key);
	if (out != NULL) {
		//Write to out
		out->data = s->c_str();
		out->len = s->length();
	}

	return !s->empty();
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

Request* GetNextRequest(RequestQueue* requests) {
	if (requests == NULL) return NULL;
	Request* request = NULL;
	try {
		if (!requests->aborted)
			requests->requests.pop(request);
	}
	catch (tbb::user_abort ex) {
		//Do nothing...
	}
	return request;

}

//Clear the cache (aborts any waiting request handlers, locks connect mutex, clears cache, unlocks).
void ClearCache(Webapp* app, RequestQueue* requests) {
	if (app == NULL || requests == NULL) return;
	requests->lock.lock();
	requests->aborted = 1;
	requests->requests.abort();

	//Cleanup when this task completes.
	app->posttask = new (tbb::task::allocate_root()) CleanupTask(app, requests);

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

int ConnectDatabase(Database* db, int database_type, const char* host, const char* username, const char* password, const char* database) {
	if (db == NULL) return -1;

	return db->connect(database_type, host, username, password, database);
}

long long ExecQuery(Database* db, const char* query, int len) {
	if (db == NULL) return -1;
	return db->exec(string(query, len));
}

void GetParameter(Webapp* app, int param, webapp_str_t* out) {
	if (app == NULL) return;
	switch (param) {
	case WEBAPP_PARAM_BASEPATH:
		out->data = app->basepath.c_str();
		out->len = app->basepath.length();
		break;
	case WEBAPP_PARAM_DBPATH:
		out->data = app->dbpath.c_str();
		out->len = app->basepath.length();
		break;
	default:
		out->data = NULL;
		out->len = 0;
		break;
	}
}
