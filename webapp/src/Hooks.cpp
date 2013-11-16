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
int GetSessionValue(SessionStore* session, webapp_str_t* key, webapp_str_t* out) {
	if(session == NULL || key == NULL) 
		return NULL;

	const string* s = &session->get(string(key->data, key->len));
	if (out != NULL) {
		//Write to out
		out->data = s->c_str();
		out->len = s->length();
	}

	return !s->empty();
}

int SetSessionValue(SessionStore* session, webapp_str_t* key, webapp_str_t* val) {
	if (session == NULL || key == NULL || val == NULL)
		return 0;
	session->store(string(key->data, key->len), string(val->data, val->len));
	return 1;
}

SessionStore* GetSession(Sessions* sessions, const char* sessionid) {
	if (sessions == NULL || sessionid == NULL) return NULL;
	return sessions->get_session(sessionid);
}

SessionStore* NewSession(Sessions* sessions, Request* request) {
	if (sessions == NULL || request == NULL) return NULL;
	return sessions->new_session(request);
}

void DestroySession(SessionStore* session) {
	session->destroy();
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

	for(std::vector<string*>::iterator it = request->handler.begin(); it != request->handler.end(); ++it) {
		delete *it;
	}

	for (std::vector<Query*>::iterator it = request->queries.begin(); it != request->queries.end(); ++it) {
		delete *it;
	}

	delete request;
}

TemplateDictionary* GetTemplate(Webapp* gallery, const char* page) {
	if(gallery != NULL) return gallery->getTemplate(page);
	else return NULL;
}


void RenderTemplate(Webapp* gallery, ctemplate::TemplateDictionary* dict, const char* page, Request* request, webapp_str_t* out) {
	if (gallery == NULL || dict == NULL || page == NULL || request == NULL || out == NULL) return;
	string* output = new string;
	ExpandTemplate(*gallery->basepath + "/content/" + page, STRIP_WHITESPACE, dict, output);

	//Clean up the template dictionary.
	delete dict;
	
	out->data = output->c_str();
	out->len = output->length();

	request->handler.push_back(output);

}

void WriteData(asio::ip::tcp::socket* socket, webapp_str_t* data) {
	if (socket == NULL || data == NULL || data->data == NULL) return;
	*(int*)data->data = htons(data->len - 4);
	try {
		asio::write(*socket, asio::buffer(data->data, data->len));
	}
	catch (system_error ec) {
		printf("Error writing to socket!");
	}
}

int ConnectDatabase(Database* db, int database_type, const char* host, const char* username, const char* password, const char* database) {
	if (db == NULL) return -1;

	return db->connect(database_type, host, username, password, database);
}

void AppendQuery(Query* q, webapp_str_t* in) {
	if (q == NULL || in == NULL || q->dbq == NULL || q->status != DATABASE_QUERY_INIT) return;
	q->dbq->append(string(in->data, in->len));
}

long long ExecString(Database* db, webapp_str_t* query) {
	if (db == NULL || query == NULL || query->data == NULL) return -1;
	return db->exec(string(query->data, query->len));
}

int SelectQuery(Database* db, Query* q) {
	if (db == NULL || q == NULL) return NULL;
	db->select(q);
	return q->status;
}

Query* CreateQuery(webapp_str_t* in, Request* r, int desc) {
	if (r == NULL) return NULL;
	Query* q = NULL;
	if (in == NULL) q = new Query(desc);
	else q = new Query(string(in->data, in->len), desc);

	r->queries.push_back(q);
	return q;
}

void SetQuery(Query* q, webapp_str_t* in) {
	if (in == NULL || in->data == NULL || q == NULL || q->status != DATABASE_QUERY_INIT) return;
	if (q->dbq != NULL) delete q->dbq;
	q->dbq = new string(in->data, in->len);
}


void ExecQuery(Database* db, Query* q) {
	if (q == NULL) return;
	db->select(q);
}

void BindParameter(Query* q, webapp_str_t* param) {
	if (q == NULL || param == NULL || param->data == NULL) return;
	q->params->push_back(string(param->data, param->len));
}

void GetParameter(Webapp* app, int param, webapp_str_t* out) {
	if (app == NULL) return;
	switch (param) {
	case WEBAPP_PARAM_BASEPATH:
		out->data = app->basepath->c_str();
		out->len = app->basepath->length();
		break;
	case WEBAPP_PARAM_DBPATH:
		out->data = app->dbpath->c_str();
		out->len = app->dbpath->length();
		break;
	default:
		out->data = NULL;
		out->len = 0;
		break;
	}
}
