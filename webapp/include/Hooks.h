/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef HOOKS_H
#define HOOKS_H

#include "Platform.h"

struct webapp_str_t;
struct Socket;
struct Request;
struct Session;
struct RequestBase;

class Webapp;
class Database;
struct Query;
class Image;
class File;

extern "C" {
/* Template */
	APIEXPORT void Template_ShowGlobalSection(ctemplate::TemplateDictionary*,
			webapp_str_t* section);
	APIEXPORT void Template_ShowSection(ctemplate::TemplateDictionary*,
										webapp_str_t* section);
	APIEXPORT void Template_SetGlobalValue(ctemplate::TemplateDictionary* dict,
										   webapp_str_t* key, webapp_str_t* value);
	APIEXPORT void Template_SetValue(ctemplate::TemplateDictionary* dict,
									 webapp_str_t* key, webapp_str_t* value);
	APIEXPORT void Template_SetIntValue(ctemplate::TemplateDictionary* dict, 
										webapp_str_t* key, long value);
	APIEXPORT ctemplate::TemplateDictionary* 
				   Template_Get(RequestBase*, webapp_str_t* name);
	APIEXPORT void Template_Clear(ctemplate::TemplateDictionary* dict);
	APIEXPORT void Template_Include(webapp_str_t* name, webapp_str_t* file);
	APIEXPORT void Template_Load(webapp_str_t* page);
	APIEXPORT webapp_str_t* 
				   Template_Render(RequestBase* worker, 
						webapp_str_t* page, Request* request);

/* Session */
	APIEXPORT webapp_str_t* 
				   GetSessionValue(Session*, webapp_str_t*);
	APIEXPORT int  SetSessionValue(Session*, webapp_str_t* key, 
								  webapp_str_t* val);
	APIEXPORT webapp_str_t* 
				   GetSessionID(Session*);
    APIEXPORT Session*
                   GetCookieSession(RequestBase*, Request*,
                                    webapp_str_t* cookies);
	APIEXPORT Session* 
                   GetSession(RequestBase*, Request*,
                              webapp_str_t* session_id);
	APIEXPORT Session* 
                   NewSession(RequestBase*, Request*,
                              webapp_str_t* primary,
                              webapp_str_t* secondary);
	APIEXPORT void 
				   DestroySession(Session*);
	APIEXPORT Session* 
				   GetRawSession(RequestBase*, Request*);

/* Script API */
	APIEXPORT webapp_str_t* CompileScript(const char* file);

/* Parameter Store */
	APIEXPORT void SetParamInt(unsigned int key, int value);
	APIEXPORT int  GetParamInt(unsigned int key);

/* Worker Handling */
	APIEXPORT void ClearCache(RequestBase*);
	APIEXPORT void Shutdown(RequestBase*);

/* Requests */
	APIEXPORT Request* 
				   GetNextRequest(RequestBase*);
	APIEXPORT void QueueRequest(RequestBase*, Request*);
	APIEXPORT void FinishRequest(Request*);
	APIEXPORT void WriteData(LuaSocket*, webapp_str_t* data);
	APIEXPORT webapp_str_t* ReadData(LuaSocket* socket, RequestBase* worker, 
						   Request* r, int bytes, int timeout);
	APIEXPORT LuaSocket* ConnectSocket(RequestBase* worker, Request* r, 
					  webapp_str_t* addr, webapp_str_t* port);
	APIEXPORT void DestroySocket(LuaSocket* socket);
	APIEXPORT int SocketAvailable(LuaSocket* socket);
/* Database */
	APIEXPORT Database* 
				   CreateDatabase();
	APIEXPORT void DestroyDatabase(Database*);
	APIEXPORT Database* 
				   GetDatabase(size_t index);
	APIEXPORT int  ConnectDatabase(Database*, int database_type, const char* host,
								  const char* username, 
								  const char* password, 
								  const char* database);
	APIEXPORT int64_t 
				   ExecString(Database*, webapp_str_t* in);
	APIEXPORT int  SelectQuery(Query*);
	APIEXPORT Query* 
				   CreateQuery(webapp_str_t*, Request*, Database*, 
							   int desc);
	APIEXPORT void SetQuery(Query*, webapp_str_t*);
	APIEXPORT void BindParameter(Query* q, webapp_str_t* in);
	
/* Time */
	struct webapp_tm {
		int tm_sec;
		int tm_min;
		int tm_hour;
		int tm_mday;
		int tm_mon;
		int tm_year;
		int tm_wday;
		int tm_yday;
		int tm_isdst;
	};

	APIEXPORT void GetTime(struct webapp_tm*);
	APIEXPORT void UpdateTime(struct webapp_tm*);

/* Image */
	APIEXPORT Image* 
				   Image_Load(webapp_str_t* filename);
	APIEXPORT void Image_Resize(Image* img, int width, int height);
	APIEXPORT void Image_Save(Image* img, webapp_str_t* filename, int destroy);
	APIEXPORT void Image_Destroy(Image* img);

/* File */
	APIEXPORT File* 
				   File_Open(webapp_str_t* filename, webapp_str_t* mode);
	APIEXPORT void File_Close(File*);
	APIEXPORT int16_t 
				   File_Read(File*, int16_t n_bytes);
	APIEXPORT void File_Write(File*, webapp_str_t* buf);
	APIEXPORT int64_t 
				   File_Size(File*);

/* SHA Hash */
//Force SHA-function inclusion from openssl.
	FORCE_UNDEFINED_SYMBOL(SHA256_Init)
	FORCE_UNDEFINED_SYMBOL(SHA256_Update)
	FORCE_UNDEFINED_SYMBOL(SHA256_Final)
	FORCE_UNDEFINED_SYMBOL(SHA512_Init)
	FORCE_UNDEFINED_SYMBOL(SHA512_Update)
	FORCE_UNDEFINED_SYMBOL(SHA512_Final)
	FORCE_UNDEFINED_SYMBOL(SHA384_Init)
	FORCE_UNDEFINED_SYMBOL(SHA384_Update)
	FORCE_UNDEFINED_SYMBOL(SHA384_Final)
}
#endif //HOOKS_H
