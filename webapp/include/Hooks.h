#ifndef HOOKS_H
#define HOOKS_H
#include "Platform.h"
#include "Session.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>

APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT const char* GetSessionValue(SessionStore*, const char*);
APIEXPORT SessionStore* GetSession(Sessions*, const char*);
APIEXPORT SessionStore* NewSession(Sessions*, const char*, const char*);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, const char* key, const char* value);
APIEXPORT Request* GetNextRequest(tbb::concurrent_bounded_queue<Request*>* requests);
APIEXPORT int GetSessionID(SessionStore*, webapp_str_t* out);
APIEXPORT void FinishRequest(Request*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, const char*);
APIEXPORT void RenderTemplate(Webapp*, ctemplate::TemplateDictionary*, const char*, std::vector<std::string*>*, webapp_str_t* out);
APIEXPORT void WriteData(asio::ip::tcp::socket*, char*, int);

#endif