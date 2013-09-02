#include "Platform.h"
#include "Gallery.h"

extern "C" {
#include "Hooks.h"
}
using namespace ctemplate;
APIEXPORT int Template_ShowSection(TemplateDictionary* dict, const char* section) {
	dict->ShowSection(section);
	return 0;
}
