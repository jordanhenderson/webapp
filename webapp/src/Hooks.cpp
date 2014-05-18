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
	delete str;
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
	
	TemplateDictionary* base = worker->baseTemplate;
	if(name == NULL) return base;
	auto tmpl = &worker->templates;
	auto dict = tmpl->find(*name);
	if(dict == tmpl->end()) return base;
	return (*dict).second;
}

void Template_Clear(TemplateDictionary* dict)
{
	if(dict == NULL) return;
	dict->Clear();
}

void Template_Include(webapp_str_t* name, webapp_str_t* file)
{
	app->templates.insert({*name, *file});
	LoadTemplate(*file, STRIP_WHITESPACE);
}

void Template_Load(webapp_str_t* page)
{
	LoadTemplate(*page, STRIP_WHITESPACE);
}

webapp_str_t* Template_Render(RequestBase* worker, webapp_str_t* page)
{
	RequestQueue* queue = static_cast<RequestQueue*>(worker);
	if(queue != NULL) 
	{
		webapp_str_t dir = "content/";
		return queue->RenderTemplate(dir + page);
	}
}

/* Session */
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
	RequestQueue* queue = static_cast<RequestQueue*>(worker);
	if(queue != NULL) 
	{
		return queue->_sessions.get_cookie_session(cookies);
	}
	return NULL;
}

Session* Session_Get(RequestBase* worker, webapp_str_t* id)
{
	RequestQueue* queue = static_cast<RequestQueue*>(worker);
	if(queue != NULL) 
	{
		return worker->_sessions.get_session(id);
	}
	return NULL;
}

Session* Session_New(RequestBase* worker, webapp_str_t* uid)
{
	RequestQueue* queue = static_cast<RequestQueue*>(worker);
	if(queue != NULL)
	{
		return worker->_sessions.new_session(uid);
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
	RequestQueue* queue = static_cast<RequestQueue*>(worker);
	if(queue != NULL)
	{
		return worker->_sessions.get_raw_session();
	}
	return NULL;
}

/* Script API */
webapp_str_t* Script_Compile(const char* file)
{
	return app->CompileScript(file);
}

/* Worker Handling */
void Worker_Create(WorkerInit* init)
{
	app->CreateWorker(*init);
}

void Worker_ClearCache(RequestBase* worker)
{

}

void Worker_Shutdown(RequestBase* worker)
{

}

/* Requests */
Request* Request_GetNext(RequestBase* worker)
{
	return worker->dequeue();
}

void Request_Queue(RequestBase* worker, Request* r) {
	worker->enqueue(r);
}

void Request_Finish(RequestBase* worker, Request* r)
{
	r->reset();
	worker->read_request(r, 0);
}

/* Socket API */
void ConnectHandler(RequestBase* worker, Request* r, LuaSocket* s, 
					const std::error_code& ec, 
					tcp::resolver::iterator it)
{
	Socket& socket = s->socket;
	if(!ec) {
		worker->enqueue(r);
	} else if (it != tcp::resolver::iterator()) {
		//Try next endpoint.
		socket.abort();
		tcp::endpoint ep = *it;
		socket.async_connect(ep, bind(&ConnectHandler, worker, r, s, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		socket.abort();
		worker->enqueue(r);
	}
}

void ResolveHandler(RequestBase* worker, Request* r, LuaSocket* s,
					const std::error_code& ec, 
					tcp::resolver::iterator it)
{
	Socket& socket = s->socket;
	if(!ec) {
		tcp::endpoint ep = *it;
		socket.async_connect(ep, bind(&ConnectHandler, worker, r, s, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		socket.abort();
		worker->enqueue(r);
	}
}

LuaSocket* Socket_Connect(RequestBase* worker, Request* r, 
					  webapp_str_t* addr, webapp_str_t* port) {
	tcp::resolver::query qry(tcp::v4(), *addr, *port);
	tcp::resolver& resolver = app->get_resolver();
	LuaSocket* s = app->create_socket();
	resolver.async_resolve(qry, bind(&ResolveHandler, worker, r, s, _1, _2));
	return s;
}

void Socket_Destroy(LuaSocket* s) {
	app->destroy_socket(s);
}

void WriteEvent(LuaSocket* s, webapp_str_t* buf,
				const std::error_code& error, size_t bytes_transferred)
{
	Socket& socket = s->socket;
	socket.waiting--;
	delete buf;
}

void ReadEvent(LuaSocket* s, RequestBase* worker, Request* r, 
	webapp_str_t* output, int timeout, const std::error_code& ec, size_t n_bytes)
{
	Socket& socket = s->socket;
	if(!ec) {
		socket.timer.cancel();
		try {
			uint16_t& n = socket.ctr;
			n += socket.read_some(buffer(output->data + n, output->len - n));
			if(n == output->len) {
				//read complete.
				worker->enqueue(r);
			} else {
				socket.timer.expires_from_now(chrono::seconds(timeout));
				socket.timer.async_wait(bind(&ReadEvent, s, worker, r,
											  output, timeout, _1, 0));
				socket.async_read_some(null_buffers(), bind(&ReadEvent, 
					s, worker, r, output, timeout, _1, 0));
			}
		} catch (...) {
			socket.abort();
			output->len = 0;
		}
	} else if(ec != asio::error::operation_aborted) {
		//Read failed/timeout.
		socket.abort();
		output->len = 0;
	}
}

void Socket_Write(LuaSocket* s, webapp_str_t* buf)
{
	Socket& socket = s->socket;
	//TODO: investigate leak here.
	webapp_str_t* tmp_buf = new webapp_str_t(*buf);
	socket.waiting++;
	
	//No try/catch statement needed; async_write always succeeds.
	//Errors handled in callback.
	async_write(socket, buffer(tmp_buf->data, tmp_buf->len),
					  bind(&WriteEvent, s, tmp_buf, _1, _2));
}

webapp_str_t* Socket_Read(LuaSocket* s, RequestBase* worker, 
						Request* r, int bytes, int timeout)
{
	//LuaSocket wraps the actual socket object.
	Socket& socket = s->socket;
	webapp_str_t* output = new webapp_str_t(bytes);
	socket.ctr = 0;
	
	socket.timer.expires_from_now(chrono::seconds(timeout));
	socket.timer.async_wait(bind(&ReadEvent, s, worker, r,
								  output, timeout, _1, 0));
	socket.async_read_some(null_buffers(), bind(&ReadEvent, s, 
							worker, r, output, timeout, _1, _2));
	return output;
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
	return app->CreateDatabase();
}

void Database_Destroy(Database* db)
{
	if(db == NULL) return;
	return app->DestroyDatabase(db);
}

Database* Database_Get(size_t index)
{
	return app->GetDatabase(index);
}

int Database_Connect(Database* db, int database_type, const char* host,
					const char* username, const char* password,
					const char* database)
{
	if(db == NULL) return 0;
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
