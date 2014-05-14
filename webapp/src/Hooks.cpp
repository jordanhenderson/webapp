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
void CleanupString(webapp_str_t* str) {
	delete str;
}

/* Template */
void Template_ShowGlobalSection(TemplateDictionary* dict, webapp_str_t* section)
{
	if(dict == NULL || section == NULL) return;
	dict->ShowTemplateGlobalSection(*section);
}

void Template_ShowSection(TemplateDictionary* dict, webapp_str_t* section)
{
	if(dict == NULL || section == NULL) return;
	dict->ShowSection(*section);
}

void Template_SetGlobalValue(TemplateDictionary* dict, webapp_str_t* key,
							 webapp_str_t* value)
{
	if (dict == NULL || key == NULL || value == NULL) return;
	dict->SetTemplateGlobalValue(*key, *value);
}

void Template_SetValue(TemplateDictionary* dict, webapp_str_t* key,
					   webapp_str_t* value)
{
	if (dict == NULL || key == NULL || value == NULL) return;
	dict->SetValue(*key, *value);
}

void Template_SetIntValue(TemplateDictionary* dict, webapp_str_t* key,
						  long value)
{
	if (dict == NULL || key == NULL) return;
	dict->SetIntValue(*key, value);
}

TemplateDictionary* Template_Get(RequestBase* worker, webapp_str_t* name)
{
	if(worker == NULL) return NULL;
	TemplateDictionary* base = worker->baseTemplate;
	if(name == NULL) return base;
	auto tmpl = &worker->templates;
	auto dict = tmpl->find(*name);
	if(dict == tmpl->end()) return base;
	return (*dict).second;
}

void Template_Clear(TemplateDictionary* dict)
{
	dict->Clear();
}

void Template_Include(webapp_str_t* name, webapp_str_t* file)
{
	if(name == NULL || file == NULL) return;
	app->templates.insert({*name, *file});
	LoadTemplate(*file, STRIP_WHITESPACE);
}

void Template_Load(webapp_str_t* page)
{
	if(page == NULL) return;
	LoadTemplate(*page, STRIP_WHITESPACE);
}

webapp_str_t* Template_Render(RequestBase* worker, webapp_str_t* page)
{
	if (page == NULL) return NULL;
	webapp_str_t dir = "content/";
	return worker->RenderTemplate(dir + page);
}

/* Session */
webapp_str_t* GetSessionValue(Session* session, webapp_str_t* key)
{
	if(session == NULL || key == NULL) return NULL;
	return session->get(*key);
}

int SetSessionValue(Session* session, webapp_str_t* key,
					webapp_str_t* val)
{
	if (session == NULL || key == NULL || val == NULL)
		return 0;
	session->put(*key, *val);
	return 1;
}

webapp_str_t* GetSessionID(Session* session)
{
	if(session == NULL) return NULL;
	return &session->session_id;
}

Session* GetCookieSession(RequestBase* worker, webapp_str_t* cookies)
{
	if(worker == NULL) return NULL;
	return worker->_sessions.get_cookie_session(cookies);
}

Session* GetSession(RequestBase* worker, webapp_str_t* session_id)
{
	if (worker == NULL) return NULL;
	return worker->_sessions.get_session(session_id);
}

Session* NewSession(RequestBase* worker, webapp_str_t* primary, 
					webapp_str_t* secondary)
{
	if (worker == NULL) return NULL;
	return worker->_sessions.new_session(primary, secondary);
}

void DestroySession(Session* session)
{
	delete session;
}

Session* GetRawSession(RequestBase* worker)
{
	if(worker == NULL) return NULL;
	return worker->_sessions.get_raw_session();
}

/* Script API */
webapp_str_t* CompileScript(const char* file)
{
	return app->CompileScript(file);
}

/* Parameter Store */
void SetParamInt(unsigned int key, int value)
{
	app->SetParamInt(key, value);
}

int GetParamInt(unsigned int key)
{
	return app->GetParamInt(key);
}

/* Worker Handling */
void ClearCache(RequestBase* worker)
{
	//Abort this lua vm.
	worker->aborted = 1;
	//Cleanup when this task completes.
	worker->cleanupTask = 1;
}

void Shutdown(RequestBase* worker)
{
	//Abort this lua vm.
	worker->aborted = 1;
	worker->cleanupTask = 1;
	//Shutdown when this task completes.
	worker->shutdown = 1;
}

/* Requests */
Request* GetNextRequest(RequestBase* worker)
{
	return worker->dequeue();
}

void QueueRequest(RequestBase* worker, Request* r) {
	worker->enqueue(r);
}

void FinishRequest(Request* r)
{
	r->reset();
	r->s.socket.async_read_some(null_buffers(), bind(
								  &Webapp::process_header_async,
								  app, r, _1, _2));
}

/* Socket API */
void ConnectHandler(RequestBase* worker, Request* r, Socket* s, 
					const std::error_code& ec, 
					tcp::resolver::iterator it)
{
	if(!ec) {
		worker->enqueue(r);
	} else if (it != tcp::resolver::iterator()) {
		//Try next endpoint.
		s->abort();
		tcp::endpoint ep = *it;
		s->async_connect(ep, bind(&ConnectHandler, worker, r, s, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		s->abort();
		worker->enqueue(r);
	}
}

void ResolveHandler(RequestBase* worker, Request* r, Socket* s,
					const std::error_code& ec, 
					tcp::resolver::iterator it)
{
	if(!ec) {
		tcp::endpoint ep = *it;
		s->async_connect(ep, bind(&ConnectHandler, worker, r, s, _1, ++it));
	} else if(ec != asio::error::operation_aborted) {
		s->abort();
		worker->enqueue(r);
	}
}

LuaSocket* ConnectSocket(RequestBase* worker, Request* r, 
					  webapp_str_t* addr, webapp_str_t* port) {
	tcp::resolver::query qry(tcp::v4(), *addr, *port);
	tcp::resolver& resolver = app->get_resolver();
	LuaSocket* s = app->create_socket();
	Socket* socket = &s->socket;
	resolver.async_resolve(qry, bind(&ResolveHandler, worker, r, socket, _1, _2));
	return s;
}

void DestroySocket(LuaSocket* s) {
	app->destroy_socket(s);
}

void WriteComplete(Socket* s, webapp_str_t* buf,
				   const std::error_code& error, size_t bytes_transferred)
{
	if(!error) {
		s->waiting--;
	}
	delete buf;

}

void WriteData(LuaSocket* s, webapp_str_t* buf)
{
	Socket* socket = &s->socket;
	//TODO: investigate leak here.
	webapp_str_t* tmp_buf = new webapp_str_t(*buf);
	socket->waiting++;
	
	//No try/catch statement needed; async_write always succeeds.
	//Errors handled in callback.
	async_write(*socket, buffer(tmp_buf->data, tmp_buf->len),
					  bind(&WriteComplete, socket, tmp_buf, _1, _2));
}

void ReadEvent(Socket* socket, RequestBase* worker, Request* r, 
	webapp_str_t* output, int timeout, const std::error_code& ec, size_t n_bytes)
{
	Socket& s = *socket;
	if(!ec) {
		s.timer.cancel();
		try {
			uint16_t& n = socket->ctr;
			n += socket->read_some(buffer(output->data + n, output->len - n));
			if(n == output->len) {
				//read complete.
				worker->enqueue(r);
			} else {
				s.timer.expires_from_now(chrono::seconds(timeout));
				s.timer.async_wait(bind(&ReadEvent, socket, worker, r,
											  output, timeout, _1, 0));
				s.async_read_some(null_buffers(), bind(&ReadEvent, 
					socket, worker, r, output, timeout, _1, 0));
			}
		} catch (...) {
			s.abort();
			output->len = 0;
		}
	} else if(ec != asio::error::operation_aborted) {
		//Read failed/timeout.
		s.abort();
		output->len = 0;
	}
}

webapp_str_t* ReadData(LuaSocket* socket, RequestBase* worker, Request* r,
			  int bytes, int timeout)
{
	//LuaSocket wraps the actual socket object.
	Socket* s = &socket->socket;
	webapp_str_t* output = new webapp_str_t(bytes);
	s->ctr = 0;
	
	s->timer.expires_from_now(chrono::seconds(timeout));
	s->timer.async_wait(bind(&ReadEvent, s, worker, r,
								  output, timeout, _1, 0));
	s->async_read_some(null_buffers(), bind(&ReadEvent, s, 
							worker, r, output, timeout, _1, _2));
	return output;
}

int SocketAvailable(LuaSocket* s)
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
Database* CreateDatabase()
{
	return app->CreateDatabase();
}

void DestroyDatabase(Database* db)
{
	return app->DestroyDatabase(db);
}

Database* GetDatabase(size_t index)
{
	return app->GetDatabase(index);
}

int ConnectDatabase(Database* db, int database_type, const char* host,
					const char* username, const char* password,
					const char* database)
{
	if (db == NULL) return -1;
	return db->connect(database_type, host, username, password, database);
}

int64_t ExecString(Database* db, webapp_str_t* query)
{
	if (db == NULL || query == NULL) return -1;
	return db->exec(*query);
}

int SelectQuery(Query* q)
{
	if (q == NULL) return 0;
	q->process();
	return q->status;
}

Query* CreateQuery(webapp_str_t* in, Database* db, int desc)
{
	if (db == NULL) return NULL;
	Query* q = NULL;
	if (in == NULL) q = new Query(db, desc);
	else q = new Query(db, *in, desc);
	return q;
}

void DestroyQuery(webapp_str_t* qry)
{
	delete qry;
}

void SetQuery(Query* q, webapp_str_t* in)
{
	if (in == NULL || q == NULL
			|| q->status != DATABASE_QUERY_INIT) return;
	q->dbq = webapp_str_t(*in);
}

void BindParameter(Query* q, webapp_str_t* param)
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

void GetTime(struct webapp_tm* output)
{
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

void UpdateTime(struct webapp_tm* output)
{
	if(output == NULL) return;
	struct tm tmp_tm;
	webapp_to_tm(output, &tmp_tm);
	mktime(&tmp_tm);
	tm_to_webapp(&tmp_tm, output);
}

/* Image */
Image* Image_Load(webapp_str_t* filename)
{
	if(filename == NULL) return NULL;
	return new Image(*filename);
}

void Image_Resize(Image* img, int width, int height)
{
	if(img == NULL) return;
	img->resize(width, height);
}

void Image_Save(Image* img, webapp_str_t* filename, int destroy)
{
	if(img == NULL || filename == NULL) return;
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
	if(filename == NULL || mode == NULL) return NULL;
	File* f = new File(*filename, mode);
	return f;
}

void File_Close(File* f)
{
	if(f == NULL) return;
	f->Close();
	delete f;
}

int16_t File_Read(File* f, int16_t n_bytes)
{
	if(f == NULL) return 0;
	return f->Read(n_bytes);
}

void File_Write(File* f, webapp_str_t* buf)
{
	if(f == NULL || buf == NULL) return;
	f->Write(*buf);
}

int64_t File_Size(File* f)
{
	if(f == NULL) return 0;
	return f->Size();
}
