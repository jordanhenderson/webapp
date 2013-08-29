#include "PluginHooks.h"
#include "Platform.h"
#include "Gallery.h"
int Obj_method(lua_State *L) {
	Gallery* g = (Gallery*)lua_touserdata(L, 1);
	RequestVars v;
	Response r;
	RamSession s;
	g->clearCache(v, r, s);
	printf("Derp %s", r.c_str());
	return 0;
}

int luaopen_ioCore(lua_State* L) {
	static const luaL_Reg Obj_lib[] = {
		{ "method", &Obj_method },
		{ NULL, NULL }
	};
	luaL_newlib(L, Obj_lib);
	return 1;
}