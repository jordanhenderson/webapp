/* Copyright (C) Jordan Henderson - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
*/

#include "Webapp.h"
#include "Session.h"

using namespace std;
using namespace ctemplate;
void TaskQueue::FinishTask() 
{
	//Ensure no other thread is cleaning.
	cleanupTask = 0;

	//Abort the worker (aborts any new requests).
	aborted = 1;

	//Notify any blocked threads to process the next request.
	while(!finished) {
		cv.notify_one();
		//Sleep to allow thread to finish, then check again.
		std::this_thread::
			sleep_for(std::chrono::milliseconds(100));
	}
	//Prevent any new requests from being queued.
	cv_mutex.lock();
}

void TaskQueue::RestartTask()
{
	aborted = 0;
	cv_mutex.unlock();
}

void TaskQueue::notify()
{
	cv.notify_one();
}

/**
 * Execute a worker task.
 * Worker tasks process queues of recieved requests (each single threaded)
 * The LUA VM must only block after handling each request.
 * Performs cleanup when finished.
*/
void WebappTask::Start()
{
	_worker = std::thread([this] {
		while (!IsAborted())
		{
			Execute();
			Cleanup();
		}
	});
}

void WebappTask::Stop()
{
	if(_worker.joinable()) _worker.join();
}

void BackgroundQueue::Execute()
{
	finished = 0;
	LuaParam _v[] = { { "bgworker", this },
		{ "app", _handler }
	};
	_handler->RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_QUEUE);
	//Set the finished flag to signify this thread is waiting.
	finished = 1;
}

void BackgroundQueue::Cleanup()
{
	TaskBase::Cleanup();
}

int BackgroundQueue::IsAborted()
{
	return TaskBase::IsAborted();
}

RequestBase::RequestBase(Webapp* handler) :
		TaskBase(handler), _sessions(_handler->db) {}

void RequestBase::Cleanup()
{
	_sessions.CleanupSessions();
	if(_cache != NULL) delete _cache;
	_cache = NULL;
	if(baseTemplate != NULL) delete baseTemplate;
	baseTemplate = NULL;

	templates.clear();
	TaskBase::Cleanup();
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

RequestQueue::~RequestQueue()
{
	if(_cache != NULL) delete _cache;
	if(baseTemplate != NULL) delete baseTemplate;
}

void RequestQueue::Execute()
{
	baseTemplate = new TemplateDictionary("");
	for(auto tmpl: _handler->templates) {
		TemplateDictionary* dict = baseTemplate->AddIncludeDictionary(tmpl.first);
		dict->SetFilename(tmpl.second);
		templates.insert({tmpl.first, dict});
	}
	if(_handler->GetParamInt(WEBAPP_PARAM_TPLCACHE)) {
		_cache = mutable_default_template_cache()->Clone();
		_cache->Freeze();
	} else {
		_cache = NULL;
	}
	
	finished = 0;
	LuaParam _v[] = { { "worker", (RequestBase*)this },
		{ "app", _handler }
	};
	_handler->RunScript(_v, sizeof(_v) / sizeof(LuaParam), SCRIPT_REQUEST);
	
	//Set the finished flag to signify this thread is waiting.
	finished = 1;
}

void RequestQueue::Cleanup()
{
	RequestBase::Cleanup();
}

int RequestQueue::IsAborted()
{
	return TaskBase::IsAborted();
}
