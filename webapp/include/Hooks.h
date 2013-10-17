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

APIEXPORT int Template_ShowGlobalSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT int Template_ShowSection(ctemplate::TemplateDictionary*, const char*);
APIEXPORT const char* GetSessionValue(SessionStore*, const char*);
APIEXPORT SessionStore* GetSession(Sessions*, const char*);
APIEXPORT SessionStore* NewSession(Sessions*, const char*, const char*);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, const char* key, const char* value);
APIEXPORT Request* GetNextRequest(tbb::concurrent_bounded_queue<Request*>* requests);
APIEXPORT int GetSessionID(SessionStore*, webapp_str_t* out);
APIEXPORT std::vector<std::string*>* StartRequestHandler();
APIEXPORT void FinishRequest(Request*, std::vector<std::string*>*);
APIEXPORT ctemplate::TemplateDictionary* GetTemplate(Webapp*, const char*);
APIEXPORT void RenderTemplate(Webapp*, ctemplate::TemplateDictionary*, const char*, std::vector<std::string*>*, webapp_str_t* out);
APIEXPORT void WriteData(asio::ip::tcp::socket*, char*, int);
#endif