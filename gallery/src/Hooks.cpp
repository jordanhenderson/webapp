#include "Platform.h"
#include "Gallery.h"

extern "C" {
#include "Hooks.h"
}
using namespace ctemplate;
using namespace std;
int Template_ShowGlobalSection(TemplateDictionary* dict, const char* section) {
	dict->ShowTemplateGlobalSection(section);
	return 0;
}

int Template_ShowSection(TemplateDictionary* dict, const char* section) {
	dict->ShowSection(section);
	return 0;
}

const char* Session_Get(SessionStore* session, const char* key) {
	string s = session->get(key);
	if(s.empty()) return NULL;
	return s.c_str();
}

int Template_SetValue(TemplateDictionary* dict, const char* key, const char* value) {
	dict->SetValue(key, value);
	return 0;
}