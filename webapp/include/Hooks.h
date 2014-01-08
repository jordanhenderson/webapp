/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef HOOKS_H
#define HOOKS_H
#include "Platform.h"
#include "Session.h"
#include "Webapp.h"

APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, webapp_str_t* section);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, webapp_str_t* section);
APIEXPORT int Template_SetGlobalValue(ctemplate::TemplateDictionary* dict, webapp_str_t* key, webapp_str_t* value);

//Get a string stored in the session.
APIEXPORT int GetSessionValue(SessionStore*, webapp_str_t*, webapp_str_t* out);
APIEXPORT int SetSessionValue(SessionStore*, webapp_str_t* key, webapp_str_t* val);
APIEXPORT SessionStore* GetSession(Sessions*, webapp_str_t* sessionid);
APIEXPORT SessionStore* NewSession(Sessions*, Request*);
APIEXPORT void DestroySession(SessionStore*);

APIEXPORT int GetSessionID(SessionStore*, webapp_str_t* out);
APIEXPORT void FinishRequest(Request*);
APIEXPORT void QueueProcess(LockedQueue<Process*>*, webapp_str_t* funtion, webapp_str_t* vars);
APIEXPORT Process* GetNextProcess(LockedQueue<Process*>*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, webapp_str_t* out);
APIEXPORT void RenderTemplate(Webapp*, ctemplate::TemplateDictionary* tmpl, webapp_str_t* page, Request*, webapp_str_t* out);
APIEXPORT void WriteData(asio::ip::tcp::socket*, webapp_str_t* data);

//Webapp stuff
APIEXPORT void SetParamInt(Webapp*, unsigned int key, unsigned int value);
APIEXPORT Request* GetNextRequest(LockedQueue<Request*>*);
APIEXPORT void ClearCache(Webapp*, LockedQueue<Request*>*);
APIEXPORT void DisableBackgroundQueue(Webapp*);
APIEXPORT unsigned long long GetWebappTime();

//Database
APIEXPORT Database* CreateDatabase(Webapp*);
APIEXPORT void DestroyDatabase(Webapp*, Database*);
APIEXPORT Database* GetDatabase(Webapp*, size_t index);
APIEXPORT int ConnectDatabase(Database*, int database_type, const char* host, const char* username, const char* password, const char* database);
APIEXPORT long long ExecString(Database*, webapp_str_t* in);
APIEXPORT int SelectQuery(Query*);
APIEXPORT Query* CreateQuery(webapp_str_t*, Request*, Database*, int desc);
APIEXPORT void SetQuery(Query*, webapp_str_t*);
APIEXPORT void BindParameter(Query* q, webapp_str_t* in);
APIEXPORT void GetCell(Query* q, unsigned int column, webapp_str_t* out);
APIEXPORT void GetColumnName(Query* q, unsigned int column, webapp_str_t* out);

//Image API
APIEXPORT Image* LoadImage(webapp_str_t* filename);
APIEXPORT void ResizeImage(Image* img, int width, int height);
APIEXPORT void SaveImage(Image* img, webapp_str_t* out, int destroy);
APIEXPORT void DestroyImage(Image* img); 

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
#endif
