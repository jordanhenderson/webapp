#ifndef HOOKS_H
#define HOOKS_H
#include "Session.h"
#include <ctemplate/template.h>
#include <tbb/concurrent_queue.h>
#if defined _WIN32 || defined __CYGWIN__
    #ifdef __GNUC__
      #define APIEXPORT __attribute__ ((dllexport))
    #else
      #define APIEXPORT __declspec(dllexport)
    #endif
#else
  #if __GNUC__ >= 4
    #define APIEXPORT __attribute__ ((visibility ("default")))
  #endif
#endif

typedef struct { const char* data; int len; } script_t;

APIEXPORT int GetScript(Webapp*, const char*, script_t* out);
APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT const char* GetSessionValue(SessionStore*, const char*);
APIEXPORT SessionStore* GetSession(Sessions*, const char*);
APIEXPORT SessionStore* NewSession(Sessions*, const char*, const char*);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, const char* key, const char* value);
//APIEXPORT const char* Request_GetParam(const char*, FCGX_Request*);
APIEXPORT Request* GetNextRequest(tbb::concurrent_bounded_queue<Request*>* requests);
APIEXPORT size_t StringLen(const char*);
APIEXPORT const char* GenCookie(const char*, const char*, int, std::vector<void*>*);
APIEXPORT const char* GetSessionID(SessionStore*);
APIEXPORT std::vector<void*>* StartRequestHandler(Request*);
APIEXPORT void FinishRequestHandler(std::vector<void*>*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, const char*);
APIEXPORT const char* RenderTemplate(Webapp*, ctemplate::TemplateDictionary*, const char*, std::vector<void*>*);
#endif