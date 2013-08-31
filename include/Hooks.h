#ifndef HOOKS_H
#define HOOKS_H

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define APIEXPORT __declspec(dllexport)
#else
#define APIEXPORT
#endif

APIEXPORT int derp(Gallery* g);

#endif