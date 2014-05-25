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

/* Helper methods */
void String_Destroy(webapp_str_t* str) {
	if(str != NULL) delete str;
}

/* Template */
void Template_ShowGlobalSection(TemplateDictionary* dict, webapp_str_t* section)
{
	if(dict == NULL) return;
	dict->ShowTemplateGlobalSection(*section);
}

void Template_ShowSection(TemplateDictionary* dict, webapp_str_t* section)
{
	if(dict == NULL) return;
	dict->ShowSection(*section);
}

void Template_SetGlobalValue(TemplateDictionary* dict, webapp_str_t* key,
							 webapp_str_t* value)
{
	if(dict == NULL) return;
	dict->SetTemplateGlobalValue(*key, *value);
}

void Template_SetValue(TemplateDictionary* dict, webapp_str_t* key,
					   webapp_str_t* value)
{
	if(dict == NULL) return;
	dict->SetValue(*key, *value);
}

void Template_SetIntValue(TemplateDictionary* dict, webapp_str_t* key,
						  long value)
{
	if(dict == NULL) return;
	dict->SetIntValue(*key, value);
}

TemplateDictionary* Template_Get(RequestBase* worker, webapp_str_t* name)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL) {
		TemplateDictionary* base = w->baseTemplate;
		if(name == NULL) return base;
		auto& tmpl = w->templates;
		auto dict = tmpl.find(*name);
		if(dict == tmpl.end()) return base;
		return dict->second;
	}
	return NULL;
}

void Template_Clear(TemplateDictionary* dict)
{
	if(dict == NULL) return;
	dict->Clear();
}

void Template_Include(webapp_str_t* name, webapp_str_t* file)
{
	app->templates.emplace(*name, *file);
	LoadTemplate(*file, STRIP_WHITESPACE);
}

void Template_Load(webapp_str_t* page)
{
	LoadTemplate(*page, STRIP_WHITESPACE);
}

webapp_str_t* Template_Render(RequestBase* worker, webapp_str_t* page)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL) 
	{
		webapp_str_t dir = "content/";
		return w->RenderTemplate(dir + page);
	}
	return NULL;
}

/* Session */
void Session_Init(RequestBase* worker, webapp_str_t* path)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL) {
		w->sessions.Init(*path);
	}
}

webapp_str_t* Session_GetValue(Session* session, webapp_str_t* key)
{
	if(session == NULL) return NULL;
	return session->get(*key);
}

void Session_SetValue(Session* session, webapp_str_t* key, 
					webapp_str_t* val)
{
	if(session == NULL) return;
	session->put(*key, *val);
}

Session* Session_GetFromCookies(RequestBase* worker, webapp_str_t* cookies)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL)
	{
		return w->sessions.get_cookie_session(cookies);
	}
	return NULL;

}

Session* Session_Get(RequestBase* worker, webapp_str_t* id)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL)
	{
		return w->sessions.get_session(*id);
	}
	return NULL;
}

Session* Session_New(RequestBase* worker, webapp_str_t* uid)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL)
	{
		return w->sessions.new_session(uid);
	}
	return NULL;
}

void Session_Destroy(Session* session)
{
	if(session == NULL) return;
	delete session;
}

Session* Session_GetRaw(RequestBase* worker)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL)
	{
		return w->sessions.get_raw_session();
	}
	return NULL;
}

/* Script API */
webapp_str_t* Script_Compile(RequestBase* worker,
							 const char* file)
{
	Worker* w = static_cast<Worker*>(worker);
	if(w != NULL)
	{
		return w->CompileScript(file);
	}
	return NULL;
}

/* Worker Handling */
void Worker_Create(WorkerInit* init)
{
	app->CreateWorker(*init);
}

void Worker_ClearCache(RequestBase* worker)
{
	worker->aborted = 1;
	worker->enqueue(NULL);
}

void Worker_Shutdown(RequestBase* worker)
{
	app->aborted = 1;
	worker->aborted = 1;
	worker->enqueue(NULL);
}

/* Requests */
Request* Request_GetNext(RequestBase* worker)
{
	return worker->dequeue();
}

void Request_Queue(RequestBase* worker, Request* r) {
	worker->reenqueue(r);
}

void Request_Finish(RequestBase* worker, Request* r)
{
	r->reset();
	worker->read_request(r, 0);
}

LuaSocket* Socket_Connect(RequestBase* worker, Request* r, 
					  webapp_str_t* addr, webapp_str_t* port) {

	worker->create_socket(r, *addr, *port);
}

void Socket_Destroy(LuaSocket* s) {
	s->socket.abort();
	delete s;
}

void Socket_Write(LuaSocket* s, RequestBase* worker,
				  webapp_str_t* buf)
{
	worker->start_write(s, buf);
}

webapp_str_t* Socket_Read(LuaSocket* s, RequestBase* worker, 
						Request* r, int bytes, int timeout)
{
	return worker->start_read(s, r, bytes, timeout);
}

int Socket_DataAvailable(LuaSocket* s)
{
	Socket& socket = s->socket;
	std::error_code ec;
	int bytes = socket.available(ec);
	if(ec) {
		return 0;
	} else {
		return bytes;
	}
}

/* Database */
Database* Database_Create()
{
	auto& databases = app->databases;
	size_t id = databases.size() + 1;
	return databases.emplace(id, id);
}

Database* Database_Get(size_t index)
{
	auto& databases = app->databases;
	return &databases[index];
}

void Database_Destroy(Database* db)
{
	auto& databases = app->databases;
	databases.erase(db->db_id);
}

int Database_Connect(Database* db, int database_type, const char* host,
					const char* username, const char* password,
					const char* database)
{
	return db->connect(database_type, host, username, password, database);
}

int64_t Database_Exec(Database* db, webapp_str_t* query)
{
	if(db == NULL) return 0;
	return db->exec(*query);
}

int Query_Select(Query* q)
{
	q->process();
	return q->status;
}

Query* Query_Create(Database* db, webapp_str_t* in)
{
	if(db == NULL) return NULL;
	Query* q = (in == NULL) ? 
		new Query(db) : new Query(db, *in);
	return q;
}

void Query_Destroy(Query* qry)
{
	delete qry;
}

void Query_Set(Query* q, webapp_str_t* in)
{
	if (q == NULL || q->status != DATABASE_QUERY_INIT) return;
	q->dbq = webapp_str_t(*in);
}

void Query_Bind(Query* q, webapp_str_t* param)
{
	if (q == NULL || param == NULL) return;
	q->params.push_back(*param);
}

/* Time */
void tm_to_webapp(struct tm* src, struct webapp_tm* output)
{
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

void webapp_to_tm(struct webapp_tm* src, struct tm* output)
{
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

void Time_Get(struct webapp_tm* output)
{
	time_t current_time = time(0);
	struct tm tmp_tm;
#ifdef _MSC_VER
	gmtime_s(&tmp_tm, &current_time);
#else
	gmtime_r(&current_time, &tmp_tm);
#endif
	tm_to_webapp(&tmp_tm, output);
}

void Time_Update(struct webapp_tm* output)
{
	struct tm tmp_tm;
	webapp_to_tm(output, &tmp_tm);
	mktime(&tmp_tm);
	tm_to_webapp(&tmp_tm, output);
}

/* Image */
Image* Image_Load(webapp_str_t* filename)
{
	return new Image(*filename);
}

void Image_Resize(Image* img, int width, int height)
{
	if(img == NULL) return;
	img->resize(width, height);
}

void Image_Save(Image* img, webapp_str_t* filename, int destroy)
{
	if(img == NULL) return;
	img->save(*filename);
	if(destroy) delete img;
}

void Image_Destroy(Image* img)
{
	if(img == NULL) return;
	delete img;
}

/* File */
File* File_Open(webapp_str_t* filename, webapp_str_t* mode)
{
	return new File(*filename, mode);
}

void File_Close(File* f)
{
	if(f == NULL) return;
	f->Close();
}

void File_Destroy(File* f)
{
	if(f == NULL) return;
	delete f;
}

int16_t File_Read(File* f, int16_t n_bytes)
{
	if(f == NULL) return 0;
	return f->Read(n_bytes);
}

void File_Write(File* f, webapp_str_t* buf)
{
	if(f == NULL) return;
	f->Write(*buf);
}

int64_t File_Size(File* f)
{
	if(f == NULL) return 0;
	return f->Size();
}
