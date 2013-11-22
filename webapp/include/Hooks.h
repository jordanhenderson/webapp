#ifndef HOOKS_H
#define HOOKS_H
#include "Platform.h"
#include "Session.h"
#include "Server.h"
#include "Webapp.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>

APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, webapp_str_t* section);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, webapp_str_t* section);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, webapp_str_t* key, webapp_str_t* value);

//Get a string stored in the session.
APIEXPORT int GetSessionValue(SessionStore*, webapp_str_t*, webapp_str_t* out);
APIEXPORT int SetSessionValue(SessionStore*, webapp_str_t* key, webapp_str_t* val);


APIEXPORT SessionStore* GetSession(Sessions*, webapp_str_t* sessionid);
APIEXPORT SessionStore* NewSession(Sessions*, Request*);
APIEXPORT void DestroySession(SessionStore*);

APIEXPORT int GetSessionID(SessionStore*, webapp_str_t* out);
APIEXPORT void FinishRequest(Request*);
APIEXPORT void QueueProcess(Webapp*, webapp_str_t* func, webapp_str_t* vars);
APIEXPORT Process* GetNextProcess(Webapp*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, webapp_str_t*);
APIEXPORT void RenderTemplate(Webapp*, ctemplate::TemplateDictionary*, webapp_str_t*, Request*, webapp_str_t* out);
APIEXPORT void WriteData(asio::ip::tcp::socket*, webapp_str_t* data);

//Webapp stuff
APIEXPORT void GetParameter(Webapp*, int, webapp_str_t* out);

APIEXPORT Request* GetNextRequest(RequestQueue* requests);
APIEXPORT void ClearCache(Webapp* app, RequestQueue* requests);
APIEXPORT void DisableBackgroundQueue(Webapp* app);

//Database
APIEXPORT int ConnectDatabase(Database*, int database_type, const char* host, const char* username, const char* password, const char* database);
APIEXPORT long long ExecString(Database*, webapp_str_t* in);
APIEXPORT int SelectQuery(Database*, Query*);
APIEXPORT Query* CreateQuery(webapp_str_t* in, Request* request, int desc);
APIEXPORT void SetQuery(Query*, webapp_str_t* in);
APIEXPORT void AppendQuery(Query*, webapp_str_t* in);
APIEXPORT void BindParameter(Query* q, webapp_str_t* in);
APIEXPORT void GetCell(Query* q, unsigned int column, webapp_str_t* out);
APIEXPORT void GetColumnName(Query* q, unsigned int column, webapp_str_t* out);
#endif