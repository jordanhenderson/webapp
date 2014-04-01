/* Copyright (C) Jordan Henderson - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
*/

#include "Webapp.h"
#include "Session.h"

using namespace std;
using namespace ctemplate;

/**
 * Execute a worker task.
 * Worker tasks process queues of recieved requests (each single threaded)
 * The LUA VM must only block after handling each request.
 * Performs cleanup when finished.
*/
void WebappTask::Start()
{
	_worker = std::thread([this] {
		while(!IsAborted()) {
			Execute();
		}
	});
}

void WebappTask::Stop()
{
	if(_worker.joinable()) _worker.join();
}

RequestBase::RequestBase() {}

RequestBase::~RequestBase() {
	if(_cache != NULL) delete _cache;
	if(baseTemplate != NULL) delete baseTemplate;
	_sessions.CleanupSessions();
}

webapp_str_t* RequestBase::RenderTemplate(const webapp_str_t& page)
{
	webapp_str_t* output = new webapp_str_t();
	WebappStringEmitter wse(output);
	if(_cache)
		_cache->ExpandNoLoad(page, STRIP_WHITESPACE, baseTemplate, NULL, &wse);
	else {
		mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::LAZY_RELOAD);
		ExpandTemplate(page, STRIP_WHITESPACE, baseTemplate, &wse);
	}
	return output;
}

void RequestQueue::Execute()
{
	baseTemplate = new TemplateDictionary("");
	for(auto tmpl: app->templates) {
		TemplateDictionary* dict = baseTemplate->AddIncludeDictionary(tmpl.first);
		dict->SetFilename(tmpl.second);
		templates.insert({tmpl.first, dict});
	}
	if(app->GetParamInt(WEBAPP_PARAM_TPLCACHE)) {
		_cache = mutable_default_template_cache()->Clone();
		_cache->Freeze();
	} else {
		_cache = NULL;
	}
	
	finished = 0;
	LuaParam _v[] = { { "worker", (RequestBase*)this } };
	app->RunScript(_v, sizeof(_v) / sizeof(LuaParam), "plugins/core/process.lua");
	
	if(_cache != NULL) {
		delete _cache;
		_cache = NULL;
	}

	if(baseTemplate != NULL) {
		delete baseTemplate;
		baseTemplate = NULL;
	}
	//Set the finished flag to signify this thread is waiting.
	finished = 1;
	app->Cleanup(cleanupTask, shutdown);
}


int RequestQueue::IsAborted()
{
	return aborted;
}
