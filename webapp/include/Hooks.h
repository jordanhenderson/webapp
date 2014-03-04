/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef HOOKS_H
#define HOOKS_H

#include "Platform.h"

struct webapp_str_t;
struct SessionStore;
struct Request;
class Sessions;
class Webapp;
class Process;
class Database;
class Query;
class Image;
class File;
template<typename T> class LockedQueue;

extern "C" {
APIEXPORT void Template_ShowGlobalSection(ctemplate::TemplateDictionary*,
										 webapp_str_t* section);
APIEXPORT void Template_ShowSection(ctemplate::TemplateDictionary*,
								   webapp_str_t* section);
APIEXPORT void Template_SetGlobalValue(ctemplate::TemplateDictionary* dict,
									  webapp_str_t* key, webapp_str_t* value);
APIEXPORT void Template_SetIntValue(ctemplate::TemplateDictionary* dict,
									  webapp_str_t* key, long value);
APIEXPORT void Template_SetValue(ctemplate::TemplateDictionary* dict,
									  webapp_str_t* key, webapp_str_t* value);
APIEXPORT ctemplate::TemplateDictionary* Template_Get(RequestQueue*, webapp_str_t* name);
APIEXPORT void Template_Render(RequestQueue* worker, webapp_str_t* page,
					Request* request, webapp_str_t* out);
APIEXPORT void Template_Clear(ctemplate::TemplateDictionary* dict);
APIEXPORT void Template_Load(webapp_str_t* page);
APIEXPORT void Template_Include(Webapp* app, webapp_str_t* name, webapp_str_t* file);

//Get a string stored in the session.
APIEXPORT int GetSessionValue(Session*, webapp_str_t*, webapp_str_t* out);
APIEXPORT int SetSessionValue(Session*, webapp_str_t* key, webapp_str_t* val);
APIEXPORT Session* GetSession(RequestQueue*, Request*);
APIEXPORT Session* NewSession(RequestQueue*, Request*);
APIEXPORT void DestroySession(Session*);
APIEXPORT int GetSessionID(Session*, webapp_str_t* out);

APIEXPORT void FinishRequest(Request*);
APIEXPORT void QueueProcess(BackgroundQueue*, webapp_str_t* funtion,
							webapp_str_t* vars);
APIEXPORT Process* GetNextProcess(BackgroundQueue*);
APIEXPORT void FinishProcess(Process*);
APIEXPORT void WriteData(Request*, webapp_str_t* data);
APIEXPORT void WriteHeader(Request*, uint32_t n_bytes, 
	webapp_str_t* content_type, webapp_str_t* cookies, int8_t cache);

//Webapp stuff
APIEXPORT void SetParamInt(Webapp*, unsigned int key, int value);
APIEXPORT int GetParamInt(Webapp*, unsigned int key);
APIEXPORT Request* GetNextRequest(RequestQueue*);
APIEXPORT void ClearCache(RequestQueue*);
APIEXPORT void Shutdown(RequestQueue*);

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

//Database
APIEXPORT Database* CreateDatabase(Webapp*);
APIEXPORT void DestroyDatabase(Webapp*, Database*);
APIEXPORT Database* GetDatabase(Webapp*, size_t index);
APIEXPORT int ConnectDatabase(Database*, int database_type, const char* host, 
	const char* username, const char* password, const char* database);
APIEXPORT uint64_t ExecString(Database*, webapp_str_t* in);
APIEXPORT int SelectQuery(Query*);
APIEXPORT Query* CreateQuery(webapp_str_t*, Request*, Database*, int desc);
APIEXPORT void SetQuery(Query*, webapp_str_t*);
APIEXPORT void BindParameter(Query* q, webapp_str_t* in);
APIEXPORT void GetCell(Query* q, unsigned int column, webapp_str_t* out);
APIEXPORT void GetColumnName(Query* q, unsigned int column, webapp_str_t* out);

//Image API
APIEXPORT Image* Image_Load(webapp_str_t* filename);
APIEXPORT void Image_Resize(Image* img, int width, int height);
APIEXPORT void Image_Save(Image* img, webapp_str_t* filename, int destroy);
APIEXPORT void Image_Destroy(Image* img); 

//File API
APIEXPORT File* File_Open(webapp_str_t* filename, webapp_str_t* mode);
APIEXPORT void File_Close(File*);
APIEXPORT uint16_t File_Read(File*, uint16_t n_bytes);
APIEXPORT void File_Write(File*, webapp_str_t* buf);
APIEXPORT uint64_t File_Size(File*);

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
