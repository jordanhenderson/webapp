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

const char* GetSessionValue(SessionStore* session, const char* key) {
	string s = session->get(key);
	if(s.empty()) return NULL;
	return s.c_str();
}

SessionStore* GetSession(Sessions* sessions, const char* sessionid) {
	return sessions->get_session(sessionid);
}

SessionStore* NewSession(Sessions* sessions, const char* host, const char* user_agent) {
	return sessions->new_session(host, user_agent);
}

int Template_SetValue(TemplateDictionary* dict, const char* key, const char* value) {
	dict->SetValue(key, value);
	return 0;
}

FCGX_Request* GetNextRequest(tbb::concurrent_bounded_queue<FCGX_Request*>* requests) {
	FCGX_Request* request = NULL;
	requests->pop(request);
	return request;
}

const char* GetCookieValue(const char* cookies, const char* key) {
	if(cookies == NULL || key == NULL)
		return NULL;
	const char* cookie = NULL;
	int len = strlen(key);
	for(int i = 0; cookies[i] != '\0'; i++) {
		if(cookies[i] == '=' && i >= len) {
			const char* k = cookies + i - len;

			if(strncmp(k, key, len) == 0) {
				//Find the end character.
				int c;
				for (c = i + 1; cookies[c] != '\0' && cookies[c] != ';'; c++);
				cookie = (const char*) malloc(c - i - 1);
				return cookie;
			}
		}
	}
	return NULL;
}

void Free(void* ptr) {
	free(ptr);
}

size_t StringLen(const char* str) {
	return strlen(str);
}

//Generate a Set-Cookie header provided name, value and date.
const char* GenCookie(const char* name, const char* value, time_t* date) {
	char* cookie = NULL;
	if(date == NULL) {
		int size = snprintf(NULL, 255, "Set-Cookie: %s=%s\r\n", name, value);
		cookie = (char*)malloc(size + 1);
		snprintf(cookie, 255, "Set-Cookie: %s=%s\r\n", name, value);
	}
	else {
		const char* date_str = date_format("%a, %d-%b-%Y %H:%M:%S GMT", 29, date, 1);
		int size = snprintf(NULL, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
		cookie = (char*)malloc(size + 1);
		snprintf(cookie, 255, "Set-Cookie: %s=%s; Expires=%s\r\n", name, value, date_str);
		free((void*)date_str);
	}
	return cookie;
}