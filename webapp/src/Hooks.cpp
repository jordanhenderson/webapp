/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Hooks.h"

using namespace ctemplate;
using namespace std;

int Template_ShowGlobalSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowTemplateGlobalSection(TemplateString(section->data, section->len));
	return 0;
}

int Template_ShowSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return 1;
	dict->ShowSection(TemplateString(section->data, section->len));
	return 0;
}

int Template_SetGlobalValue(TemplateDictionary* dict, webapp_str_t* key,
							webapp_str_t* value) {
	if (dict == NULL || key == NULL || value == NULL) return 1;
	dict->SetTemplateGlobalValue(TemplateString(key->data, key->len),
								 TemplateString(value->data, value->len));
	return 0;
}

TemplateDictionary* GetTemplate(Webapp* app) {
	if(app != NULL) return app->GetTemplate();
	else return NULL;
}

void RenderTemplate(Webapp* app, ctemplate::TemplateDictionary* dict, 
	webapp_str_t* page, Request* request, webapp_str_t* out) {
	if (app == NULL || dict == NULL || page == NULL || 
		request == NULL || out == NULL) return;

	string* output = new string;
	string pagestr(page->data, page->len);
	ExpandTemplate("content/" + pagestr, STRIP_WHITESPACE, dict, output);

	//Clean up the template dictionary.
	delete dict;
	
	out->data = output->c_str();
	out->len = output->length();

	request->strings.push_back(output);
}

//Cleaned up by backend.
int GetSessionValue(SessionStore* session, webapp_str_t* key, 
	webapp_str_t* out) {
	if(session == NULL || key == NULL) 
		return 0;

	const string* s = &session->get(string(key->data, key->len));
	if (out != NULL) {
		//Write to out
		out->data = s->c_str();
		out->len = s->length();
	}

	return !s->empty();
}

int SetSessionValue(SessionStore* session, webapp_str_t* key,
	webapp_str_t* val) {
	if (session == NULL || key == NULL || val == NULL)
		return 0;
	session->store(string(key->data, key->len), string(val->data, val->len));
	return 1;
}

SessionStore* GetSession(Sessions* sessions, webapp_str_t* sessionid) {
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
	Process* p = new Process();
	p->func = webapp_strdup(func);
	p->vars = webapp_strdup(vars);
	background_queue->enqueue(p);
}

Process* GetNextProcess(LockedQueue<Process*>* background_queue) {
	if (background_queue == NULL) return NULL;
	return background_queue->dequeue();
}

int GetSessionID(SessionStore* session, webapp_str_t* out) {
	if(session == NULL || out == NULL) return 0;
	out->data = session->sessionid.c_str();
	out->len = session->sessionid.length();
	return 1;
}

void FinishRequest(Request* request) {
	if(request == NULL) return;
	delete request;
}

void WriteData(asio::ip::tcp::socket* socket, webapp_str_t* data) {
	if (socket == NULL || data == NULL || data->data == NULL) return;
	*(unsigned short*)data->data = htons((unsigned short)data->len - sizeof(unsigned short));
	try {
		asio::write(*socket, asio::buffer(data->data, data->len));
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

Database* GetDatabase(Webapp* app, size_t index) {
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
	if (db == NULL || query == NULL || query->data == NULL) return -1;
	return db->exec(string(query->data, query->len));
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
	else q = new Query(db, string(in->data, in->len), desc);

	r->queries.push_back(q);
	return q;
}

void SetQuery(Query* q, webapp_str_t* in) {
	if (in == NULL || in->data == NULL || q == NULL
			|| q->status != DATABASE_QUERY_INIT) return;
	if (q->dbq != NULL) delete q->dbq;
	q->dbq = new string(in->data, in->len);
}

void BindParameter(Query* q, webapp_str_t* param) {
	if (q == NULL || param == NULL || param->data == NULL) return;
	q->params->push_back(string(param->data, param->len));
}

unsigned long long GetWebappTime() {
	time_t current_time = time(0);
	return current_time * 1000000;
}

//Image API 
Image* LoadImage(webapp_str_t* filename) {
	if(filename == NULL) return NULL;
	return new Image(string(filename->data, filename->len));
}

void ResizeImage(Image* img, int width, int height) {
	if(img == NULL) return;
	img->resize(width, height);
}

void SaveImage(Image* img, webapp_str_t* out, int destroy) {
	if(img == NULL || out == NULL) return;
	img->save(string(out->data, out->len));
	if(destroy) delete img;
}

void DestroyImage(Image* img) {
	if(img == NULL) return;
	delete img;
}

//File API
File* OpenFile(webapp_str_t* filename, webapp_str_t* mode) {
	if(filename == NULL || mode == NULL) return NULL;
	File* f = new File(string(filename->data, filename->len),
		string(mode->data, mode->len));
	return f;
}

void CloseFile(File* f) {
	if(f == NULL) return;
	f->Close();
	delete f;
}

void ReadFile(File* f, webapp_str_t* out) {
	if(f == NULL || out == NULL) return;
	out->data = f->Read();
	out->len = f->Size();
}

void WriteFile(File* f, webapp_str_t* buf) {
	if(f == NULL || buf == NULL) return;
	f->Write(string(buf->data, buf->len));
}

void CleanupFile(File* f) {
	if(f == NULL) return;
	f->Cleanup();
}

long long FileSize(File* f) {
	if(f == NULL) return 0;
	return f->Size();
}
