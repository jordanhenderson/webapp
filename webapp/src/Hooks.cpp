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
#include "Database.h"
#include "FileSystem.h"
#include "Hooks.h"

using namespace ctemplate;
using namespace std;
using namespace std::placeholders;
using namespace asio;
using namespace asio::ip;

void Template_ShowGlobalSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return;
	dict->ShowTemplateGlobalSection(*section);
}

void Template_ShowSection(TemplateDictionary* dict, webapp_str_t* section) {
	if(dict == NULL || section == NULL) return;
	dict->ShowSection(*section);
}

void Template_SetGlobalValue(TemplateDictionary* dict, webapp_str_t* key,
							webapp_str_t* value) {
	if (dict == NULL || key == NULL || value == NULL) return;
	dict->SetTemplateGlobalValue(*key, *value);
}

void Template_SetValue(TemplateDictionary* dict, webapp_str_t* key,
							webapp_str_t* value) {
	if (dict == NULL || key == NULL || value == NULL) return;
	dict->SetValue(*key, *value);
}

void Template_SetIntValue(TemplateDictionary* dict, webapp_str_t* key,
							long value) {
	if (dict == NULL || key == NULL) return;
	dict->SetIntValue(*key, value);
}

TemplateDictionary* Template_Get(RequestQueue* worker, webapp_str_t* name) {
	if(worker == NULL) return NULL;
	TemplateDictionary* base = worker->baseTemplate;
	if(name == NULL) return base;
	auto tmpl = &worker->templates;
	auto dict = tmpl->find(*name);
	if(dict == tmpl->end()) return base;
	return (*dict).second;
}

void Template_Clear(TemplateDictionary* dict) {
	dict->Clear();
}

void Template_Include(Webapp* app, webapp_str_t* name, webapp_str_t* file) {
	app->templates.insert({*name, *file});
	LoadTemplate(*file, STRIP_WHITESPACE);
}

void Template_Load(webapp_str_t* page) {
	if(page == NULL) return;
	LoadTemplate(*page, STRIP_WHITESPACE);
}

void Template_Render(RequestQueue* worker, webapp_str_t* page,
					Request* request, webapp_str_t* out) {
	if (out == NULL || page == NULL) return;
	
	webapp_str_t dir = "content/";
	string* output = worker->RenderTemplate(dir + page);

	out->data = (char*)output->c_str();
	out->len = output->length();

	request->strings.push_back(output);
}

int GetSessionValue(Session* session, webapp_str_t* key, webapp_str_t* out) {
	if(session == NULL || key == NULL) 
		return 0;

    webapp_str_t* val = session->get(*key);
	if (out != NULL) {
		//Write to out
        out->data = val->data;
        out->len = val->len;
	}

    return !(val->len == 0);
}

int SetSessionValue(Session* session, webapp_str_t* key,
	webapp_str_t* val) {
	if (session == NULL || key == NULL || val == NULL)
		return 0;
    session->put(*key, *val);
	return 1;
}

int GetSessionID(Session* session, webapp_str_t* out) {
    if(session == NULL || out == NULL) return 0;
    out->data = session->session_id.data;
    out->len = session->session_id.len;
    return 1;
}

Session* GetSession(RequestQueue* worker, Request* request) {
    if (worker == NULL || request == NULL) return NULL;
    return worker->_sessions.get_session(request);
}

Session* NewSession(RequestQueue* worker, Request* request) {
    if (worker == NULL || request == NULL) return NULL;
    return worker->_sessions.new_session(request);
}

void DestroySession(Session* session) {
	delete session;
}

void SetParamInt(Webapp* app, unsigned int key, int value) {
	if(app == NULL) return;
	app->SetParamInt(key, value);
}

int GetParamInt(Webapp* app, unsigned int key) {
	if(app == NULL) return 0;
	return app->GetParamInt(key);
}

Request* GetNextRequest(RequestQueue* worker) {
	return worker->dequeue();
}

void ClearCache(RequestQueue* worker) {
	//Abort this lua vm.
	worker->aborted = 1;
	//Cleanup when this task completes.
	worker->cleanupTask = 1;
}

//Shuts the entire server down.
void Shutdown(RequestQueue* worker) {
	//Abort this lua vm.
	worker->aborted = 1;
	worker->cleanupTask = 1;
	//Shutdown when this task completes.
	worker->shutdown = 1;
}

void QueueProcess(BackgroundQueue* worker, webapp_str_t* func,
				  webapp_str_t* vars) {
	if (func == NULL || vars == NULL || worker->aborted) return;
	Process* p = new Process(func, vars);
	worker->enqueue(p);
}

Process* GetNextProcess(BackgroundQueue* worker) {
	return worker->dequeue();
}

void FinishProcess(Process* process) {
	delete process;
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

void WriteComplete(std::atomic<int>* wt, webapp_str_t* buf, 
	const asio::error_code& error, size_t bytes_transferred) {
		(*wt)--;
		delete buf;
}

/**
 * WriteSocket must own _data (handles garbage collection).
*/
void WriteSocket(Request* r, webapp_str_t* s) {
	auto wt = &r->waiting;
	(*wt)++;
	
	try {
		asio::async_write(*r->socket, asio::buffer(s->data, s->len),
			bind(&WriteComplete, wt, s, _1, _2));
	}
	catch (asio::system_error ec) {
		printf("Error writing to socket!");
	}
}

void WriteData(Request* r, webapp_str_t* data) {
	if(r == NULL || data == NULL) return;
	if(r->shutdown) return;
	
	webapp_str_t* s = new webapp_str_t(*data);
	WriteSocket(r, s);
}

void WriteHeader(Request* r, uint32_t n_bytes, 
	webapp_str_t* content_type, webapp_str_t* cookies, int8_t cache) {
	if(r == NULL || content_type == NULL || cookies == NULL) return;
	if(r->shutdown) return;
	webapp_data_t<uint32_t> len(htonl(n_bytes));
	webapp_data_t<uint32_t> content_type_len = htonl(content_type->len);
	webapp_data_t<uint32_t> cookies_len = htonl(cookies->len);
	webapp_data_t<int8_t> cache_s = cache;
	
	webapp_str_t* s = new webapp_str_t(
		len + content_type_len + cookies_len + cache_s + content_type + cookies);
	WriteSocket(r, s);
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

uint64_t ExecString(Database* db, webapp_str_t* query) {
	if (db == NULL || query == NULL) return -1;
	return db->exec(*query);
}

int SelectQuery(Query* q) {
	if (q == NULL) return 0;
	q->process();
	return q->status;
}

Query* CreateQuery(webapp_str_t* in, Request* r, Database* db, int desc) {
	if (r == NULL || db == NULL) return NULL;
	Query* q = NULL;
	if (in == NULL) q = new Query(db, desc);
	else q = new Query(db, *in, desc);

	r->queries.push_back(q);
	return q;
}

void SetQuery(Query* q, webapp_str_t* in) {
	if (in == NULL || q == NULL
			|| q->status != DATABASE_QUERY_INIT) return;
	q->dbq = webapp_str_t(*in);
}

void BindParameter(Query* q, webapp_str_t* param) {
	if (q == NULL || param == NULL) return;
	q->params->push_back(*param);
}

void tm_to_webapp(struct tm* src, struct webapp_tm* output) {
    output->tm_sec = src->tm_sec;
    output->tm_min = src->tm_min;
    output->tm_hour = src->tm_hour;
    output->tm_mday = src->tm_mday;
    output->tm_mon = src->tm_mon;
    output->tm_year = src->tm_year;
    output->tm_wday = src->tm_wday;
    output->tm_yday = src->tm_yday;
    output->tm_isdst = src->tm_isdst;
}

void webapp_to_tm(struct webapp_tm* src, struct tm* output) {
    output->tm_sec = src->tm_sec;
    output->tm_min = src->tm_min;
    output->tm_hour = src->tm_hour;
    output->tm_mday = src->tm_mday;
    output->tm_mon = src->tm_mon;
    output->tm_year = src->tm_year;
    output->tm_wday = src->tm_wday;
    output->tm_yday = src->tm_yday;
    output->tm_isdst = src->tm_isdst;
}

void GetTime(struct webapp_tm* output) {
    if(output == NULL) return;
	time_t current_time = time(0);
    struct tm tmp_tm;
#ifdef _MSC_VER
    gmtime_s(&tmp_tm, &current_time);
#else
    gmtime_r(&current_time, &tmp_tm);
#endif
    tm_to_webapp(&tmp_tm, output);

}

void UpdateTime(struct webapp_tm* output) {
    if(output == NULL) return;
    struct tm tmp_tm;
    webapp_to_tm(output, &tmp_tm);
    mktime(&tmp_tm);
    tm_to_webapp(&tmp_tm, output);
}

//Image API 
Image* Image_Load(webapp_str_t* filename) {
	if(filename == NULL) return NULL;
	return new Image(*filename);
}

void Image_Resize(Image* img, int width, int height) {
	if(img == NULL) return;
	img->resize(width, height);
}

void Image_Save(Image* img, webapp_str_t* filename, int destroy) {
	if(img == NULL || filename == NULL) return;
	img->save(*filename);
	if(destroy) delete img;
}

void Image_Destroy(Image* img) {
	if(img == NULL) return;
	delete img;
}

//File API
File* File_Open(webapp_str_t* filename, webapp_str_t* mode) {
	if(filename == NULL || mode == NULL) return NULL;
	File* f = new File(*filename, mode);
	return f;
}

void File_Close(File* f) {
	if(f == NULL) return;
	f->Close();
	delete f;
}

uint16_t File_Read(File* f, uint16_t n_bytes) {
	if(f == NULL) return 0;
	return f->Read(n_bytes);
}

void File_Write(File* f, webapp_str_t* buf) {
	if(f == NULL || buf == NULL) return;
	f->Write(*buf);
}

uint64_t File_Size(File* f) {
	if(f == NULL) return 0;
	return f->Size();
}
