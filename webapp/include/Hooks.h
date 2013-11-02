#ifndef HOOKS_H
#define HOOKS_H
#include "Platform.h"
#include "Session.h"
#include "Server.h"
#include "Webapp.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>

APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, const char*);

//Get a string stored in the session.
APIEXPORT int GetSessionValue(SessionStore*, webapp_str_t*, webapp_str_t* out);
APIEXPORT int SetSessionValue(SessionStore*, webapp_str_t* key, webapp_str_t* val);


APIEXPORT SessionStore* GetSession(Sessions*, const char*);
APIEXPORT SessionStore* NewSession(Sessions*, const char*, const char*);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, const char* key, const char* value);

APIEXPORT int GetSessionID(SessionStore*, webapp_str_t* out);
APIEXPORT void FinishRequest(Request*);
APIEXPORT void QueueProcess(Webapp*, webapp_str_t* func, webapp_str_t* vars);
APIEXPORT Process* GetNextProcess(Webapp*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, const char*);
APIEXPORT void RenderTemplate(Webapp*, ctemplate::TemplateDictionary*, const char*, std::vector<std::string*>*, webapp_str_t* out);
APIEXPORT void WriteData(asio::ip::tcp::socket*, webapp_str_t* data);

//Webapp stuff
APIEXPORT void GetParameter(Webapp*, int, webapp_str_t* out);

APIEXPORT Request* GetNextRequest(RequestQueue* requests);
APIEXPORT void ClearCache(Webapp* app, RequestQueue* requests);

//Database
APIEXPORT int ConnectDatabase(Database*, int database_type, const char* host, const char* username, const char* password, const char* database);
APIEXPORT long long ExecString(Database*, webapp_str_t* in);
APIEXPORT void SelectQuery(Database*, Query*, int desc);
APIEXPORT Query* CreateQuery(webapp_str_t* in);
APIEXPORT void SetQuery(Query*, webapp_str_t* in);
APIEXPORT void DestroyQuery(Query*);
APIEXPORT void BindParameter(Query* q, webapp_str_t* in);
APIEXPORT void GetCell(Query* q, unsigned int column, webapp_str_t* out);
APIEXPORT void GetColumnName(Query* q, unsigned int column, webapp_str_t* out);
#endif