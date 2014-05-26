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
struct Database;
struct Query;

class Worker;
class Sessions;
class Webapp;
class Image;
class File;

extern "C" {
/* Helper methods */
	APIEXPORT void String_Destroy(webapp_str_t*);

/* Session */
	APIEXPORT void Session_Init(Worker* worker, webapp_str_t* path);
	APIEXPORT webapp_str_t* 
				   Session_GetValue(Session*, webapp_str_t*);
	APIEXPORT void Session_SetValue(Session*, webapp_str_t* key, 
								  webapp_str_t* val);
    APIEXPORT Session*
				   Session_GetFromCookies(Worker*, webapp_str_t* cookies);
	APIEXPORT Session* 
				   Session_Get(Worker*, webapp_str_t* id);
	APIEXPORT Session* 
				   Session_New(Worker*, webapp_str_t* uid);
	APIEXPORT void 
				   Session_Destroy(Session*);
	APIEXPORT Session* 
				   Session_GetRaw(Worker*);

/* Script API */
	APIEXPORT webapp_str_t* 
				   Script_Compile(Worker* worker, const char* file);

/* Worker Handling */
	APIEXPORT void Worker_Create(WorkerInit* init);
	APIEXPORT void Worker_ClearCache(Worker*);
	APIEXPORT void Worker_Shutdown(Worker*);

/* Requests */
	APIEXPORT Request* 
				   Request_GetNext(Worker*);
	APIEXPORT void Request_Queue(Worker*, Request*);
	APIEXPORT void Request_Finish(Worker*, Request*);
	APIEXPORT void Socket_Write(LuaSocket*, Worker*, webapp_str_t* data);
	APIEXPORT webapp_str_t* 
				   Socket_Read(LuaSocket* socket, Worker* worker, 
							Request* r, int bytes, int timeout);
	APIEXPORT LuaSocket* 
				   Socket_Connect(Worker* worker, Request* r, 
								  webapp_str_t* addr, webapp_str_t* port);
	APIEXPORT void Socket_Destroy(LuaSocket* socket);
	APIEXPORT int  Socket_DataAvailable(LuaSocket* socket);
/* Database */
	APIEXPORT Database* 
				   Database_Create();
	APIEXPORT void Database_Destroy(Database*);
	APIEXPORT Database* 
				   Database_Get(size_t index);
	APIEXPORT int  Database_Connect(Database*, int database_type, 
									const char* host,
									const char* username, 
									const char* password, 
									const char* database);
	APIEXPORT int64_t 
				   Database_Exec(Database*, webapp_str_t* in);
	APIEXPORT int  Query_Select(Query*);
	APIEXPORT Query* 
				   Query_Create(Database*, webapp_str_t*);
	APIEXPORT void Query_Destroy(Query*);
	APIEXPORT void Query_Set(Query*, webapp_str_t*);
	APIEXPORT void Query_Bind(Query* q, webapp_str_t* in);
	
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

	APIEXPORT void Time_Get(struct webapp_tm*);
	APIEXPORT void Time_Update(struct webapp_tm*);

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
	APIEXPORT void File_Destroy(File*);
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
