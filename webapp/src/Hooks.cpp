/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Platform.h"
extern "C" {
	#define _TINYDIR_FUNC APIEXPORT
	#include <tinydir.h>
}

#include "Webapp.h"
#include "Session.h"
#include "Image.h"
#include "Hooks.h"
#include "Database.h"
#include "FileSystem.h"

using namespace ctemplate;
using namespace std;
using namespace std::placeholders;
using namespace asio;
using namespace asio::ip;

int Template_ShowGlobalSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowTemplateGlobalSection(*section);
	return 0;
}

int Template_ShowSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowSection(*section);
	return 0;
}

int Template_SetGlobalValue(TemplateDictionary* dict, webapp_str_t* key,
							webapp_str_t* value) {
	if (dict == NULL || key == NULL || value == NULL) return 1;
	dict->SetTemplateGlobalValue(*key, *value);
	return 0;
}

TemplateDictionary* Template_Get(Webapp* app, Request* request) {
	if(app == NULL) return NULL;
	TemplateDictionary* dict = app->GetTemplate();
	if(dict != NULL) request->dicts.push_back(dict);
	return dict;
}

void Template_ReloadAll() {
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);
}

void Template_Load(webapp_str_t* page) {
	if(page == NULL) return;
	LoadTemplate(*page, STRIP_WHITESPACE);
}

void Template_Render(ctemplate::TemplateCache* cache,
					ctemplate::TemplateDictionary* dict, webapp_str_t* page,
					Request* request, webapp_str_t* out) {
	if (cache == NULL || dict == NULL || page == NULL ||
		request == NULL || out == NULL) return;

	string* output = new string;
	string pagestr = *page;
	cache->ExpandNoLoad("content/" + pagestr, STRIP_WHITESPACE, dict, NULL,
						  output);

	out->data = (char*)output->c_str();
	out->len = output->length();

	request->strings.push_back(output);
}

//Cleaned up by backend.
int GetSessionValue(SessionStore* session, webapp_str_t* key,
	webapp_str_t* out) {
	if(session == NULL || key == NULL) 
		return 0;

	const string* s = &session->get(*key);
	if (out != NULL) {
		//Write to out
		out->data = (char*)s->c_str();
		out->len = s->length();
	}

	return !s->empty();
}

int SetSessionValue(SessionStore* session, webapp_str_t* key,
	webapp_str_t* val) {
	if (session == NULL || key == NULL || val == NULL)
		return 0;
	session->store(*key, *val);
	return 1;
}

SessionStore* GetSession(Sessions* sessions, webapp_str_t* sessionid) {
	if (sessions == NULL || sessionid == NULL) return NULL;
	return sessions->get_session(*sessionid);
}

SessionStore* NewSession(Sessions* sessions, Request* request) {
	if (sessions == NULL || request == NULL) return NULL;
	return sessions->new_session(request);
}

void DestroySession(SessionStore* session) {
	session->destroy();
}

void SetParamInt(Webapp* app, unsigned int key, int value) {
	if(app == NULL) return;
	app->SetParamInt(key, value);
}

int GetParamInt(Webapp* app, unsigned int key) {
	if(app == NULL) return 0;
	return app->GetParamInt(key);
}

Request* GetNextRequest(LockedQueue<Request*>* requests) {
	if (requests == NULL) return NULL;
	return requests->dequeue();
}

void ClearCache(Webapp* app, LockedQueue<Request*>* requests) {
	if (app == NULL || requests == NULL) return;
	//Abort this lua vm.
	requests->aborted = 1;
	//Cleanup when this task completes.
	requests->cleanupTask = 1;
}

void QueueProcess(LockedQueue<Process*>* background_queue, webapp_str_t* func,
				  webapp_str_t* vars) {
	if (func == NULL || vars == NULL || background_queue == NULL 
		|| background_queue->aborted) return;
	Process* p = new Process(func, vars);
	background_queue->enqueue(p);
}

Process* GetNextProcess(LockedQueue<Process*>* background_queue) {
	if (background_queue == NULL) return NULL;
	return background_queue->dequeue();
}

void FinishProcess(Process* process) {
	delete process;
}

int GetSessionID(SessionStore* session, webapp_str_t* out) {
	if(session == NULL || out == NULL) return 0;
	out->data = (char*)session->sessionid.c_str();
	out->len = session->sessionid.length();
	return 1;
}

void CleanupRequest(Request* r) {
	if(r->waiting == 0) {
		delete r;
	} else {
		r->socket->get_io_service().post(bind(CleanupRequest, r));
	}
}

void FinishRequest(Request* r) {
	if(r == NULL) return;
	r->shutdown = 1;
	r->socket->get_io_service().post(bind(CleanupRequest, r));
}

void WriteComplete(Request* r, webapp_str_t* buf, 
	const asio::error_code& error, size_t bytes_transferred) {
		r->waiting--;
		delete buf;
}

void WriteData(Request* request, webapp_str_t* data) {
	if(request == NULL || data == NULL || data->data == NULL) return;
	if(request->shutdown) return;
	
	uint16_t len = htons(data->len);
	webapp_str_t* s = new webapp_str_t(*data);
	request->waiting++;
	
	try {
		asio::async_write(*request->socket, asio::buffer(s->data, s->len),
			bind(&WriteComplete, request, s, _1, _2));
	}
	catch (system_error ec) {
		printf("Error writing to socket!");
	}

}

Database* CreateDatabase(Webapp* app) {
	if(app == NULL) return NULL;
	return app->CreateDatabase();
}

void DestroyDatabase(Webapp* app, Database* db) {
	if(app == NULL) return;
	return app->DestroyDatabase(db);
}

Database* GetDatabase(Webapp* app, long long index) {
	if(app == NULL) return NULL;
	return app->GetDatabase(index);
}

int ConnectDatabase(Database* db, int database_type, const char* host,
					const char* username, const char* password,
					const char* database) {
	if (db == NULL) return -1;
	return db->connect(database_type, host, username, password, database);
}

long long ExecString(Database* db, webapp_str_t* query) {
	if (db == NULL || query == NULL) return -1;
	return db->exec(*query);
}

int SelectQuery(Query* q) {
	if (q == NULL) return 0;
	q->process();
	return q->status;
}

Query* CreateQuery(webapp_str_t* in, Request* r, Database* db, int desc) {
	if (r == NULL) return NULL;
	Query* q = NULL;
	if (in == NULL) q = new Query(db, desc);
	else q = new Query(db, *in, desc);

	r->queries.push_back(q);
	return q;
}

void SetQuery(Query* q, webapp_str_t* in) {
	if (in == NULL || q == NULL
			|| q->status != DATABASE_QUERY_INIT) return;
	if (q->dbq != NULL) delete q->dbq;
	q->dbq = new string(*in);
}

void BindParameter(Query* q, webapp_str_t* param) {
	if (q == NULL || param == NULL) return;
	q->params->push_back(*param);
}

unsigned long long GetWebappTime() {
	time_t current_time = time(0);
	return current_time * 1000000;
}

//Image API 
Image* LoadImage(webapp_str_t* filename) {
	if(filename == NULL) return NULL;
	return new Image(*filename);
}

void ResizeImage(Image* img, int width, int height) {
	if(img == NULL) return;
	img->resize(width, height);
}

void SaveImage(Image* img, webapp_str_t* filename, int destroy) {
	if(img == NULL || filename == NULL) return;
	img->save(*filename);
	if(destroy) delete img;
}

void DestroyImage(Image* img) {
	if(img == NULL) return;
	delete img;
}

//File API
File* OpenFile(webapp_str_t* filename, webapp_str_t* mode) {
	if(filename == NULL || mode == NULL) return NULL;
	File* f = new File(*filename, mode);
	return f;
}

void CloseFile(File* f) {
	if(f == NULL) return;
	f->Close();
	delete f;
}

uint16_t ReadFile(File* f, uint16_t n_bytes) {
	if(f == NULL) return 0;
	return f->Read(n_bytes);
}

void WriteFile(File* f, webapp_str_t* buf) {
	if(f == NULL || buf == NULL) return;
	f->Write(*buf);
}

long long FileSize(File* f) {
	if(f == NULL) return 0;
	return f->Size();
}
