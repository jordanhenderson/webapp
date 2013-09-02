#ifndef HOOKS_H
#define HOOKS_H
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
APIEXPORT int Template_ShowSection(Gallery*, ctemplate::TemplateDictionary*);

#endif