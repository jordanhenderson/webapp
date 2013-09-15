#ifndef HOOKS_H
#define HOOKS_H
#include "Session.h"
#include <ctemplate/template.h>
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
APIEXPORT const char* Session_Get(SessionStore*, const char*);
APIEXPORT int Template_SetValue(ctemplate::TemplateDictionary* dict, const char* key, const char* value);
APIEXPORT void Register_Hook(std::map<std::string, GallFunc>* functionMap, const char* hook, void* lua_func);
#endif