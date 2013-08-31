#include "Platform.h"
#include "Gallery.h"

extern "C" {
#include "Hooks.h"
}

APIEXPORT int derp(Gallery* g) {
	RequestVars v;
	Response r;
	RamSession s;
	g->clearCache(v, r, s);
	printf("Derp %s", r.c_str());
	return 0;
}
