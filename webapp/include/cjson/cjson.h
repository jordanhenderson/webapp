#include <lua.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define snprintf _snprintf
#endif

#define ENABLE_CJSON_GLOBAL 1
int luaopen_cjson(lua_State *l);