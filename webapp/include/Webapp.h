#ifndef GALLERY_H
#define GALLERY_H

#include <asio.hpp>
#include "Platform.h"
#include "CPlatform.h"
#include "FileSystem.h"
#include "Database.h"
#include "Session.h"
#include <ctemplate/template.h>
#include "readerwriterqueue.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}


#include "Schema.h"

class Logging;
class Webapp;
//LuaParams are temporary containers to hold stack variables for lua scripts.
struct LuaParam {
	const char* name;
	void* ptr;
};

struct TemplateData {
	std::string name;
	FileData* data;
};

struct Request {
	asio::ip::tcp::socket* socket = NULL;
	std::vector<char>* v = NULL;
	std::vector<char>* headers = NULL;
	int amount_to_recieve = 0;
	int length = 0;
	int method = 0;
	webapp_str_t uri;
	webapp_str_t host;
	webapp_str_t user_agent;
	webapp_str_t cookies;
	webapp_str_t request_body;
	std::vector<std::string*> handler;
	webapp_str_t* input_chain[STRING_VARS];
	std::vector<Query*> queries;
	~Request() {
		if (socket != NULL) delete socket;
		if (v != NULL) delete v;
		if (headers != NULL) delete headers;

    }

};

class TaskQueue {
public:
	unsigned int cleanupTask = 0;
	std::atomic<unsigned int> aborted{0};
	unsigned int finished = 0;
	std::condition_variable cv;
	std::mutex cv_mutex; //Mutex to allow ready cv
};

class RequestQueue : public TaskQueue {
public:
	moodycamel::ReaderWriterQueue<Request*> requests;
    RequestQueue() : requests(WEBAPP_DEFAULT_QUEUESIZE) {}
};

struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};

template<typename T>
class LockedQueue : public TaskQueue {
private:
	moodycamel::ReaderWriterQueue<T> process_queue;
public:
	//Each operation (enqueue, dequeue) must be done single threaded.
	//We need MP (Multi-Producer), so lock production.
	void notify() {cv.notify_one();}
	void enqueue(T i) { cv_mutex.lock(); process_queue.enqueue(i); cv_mutex.unlock(); cv.notify_one();  }
	T dequeue() { 
		T r;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			while (!process_queue.try_dequeue(r) && !aborted) cv.wait(lk);
		}
		if(aborted) return NULL;
		return r; 
	}
    LockedQueue() : process_queue(WEBAPP_DEFAULT_QUEUESIZE) {}
};


class TaskBase {
public:
    TaskBase(Webapp* handler, TaskQueue* q) : _handler(handler), _q(q) {}
	void start() {
		_worker = std::thread([this]{execute();});
	}
	void join() {
		_worker.join();
	}
	TaskQueue* _q = NULL;
	virtual void execute() = 0;
    static void start_thread(TaskBase* base) { base->execute(); }
protected:
	Webapp* _handler = NULL;
	
private:
	std::thread _worker;
	
};

class WebappTask : public TaskBase {
private:
	unsigned int _id;
	unsigned int _bg;
	Sessions* _sessions;
	void cleanup(TaskQueue*);
public:
	WebappTask(Webapp* handler, Sessions* sessions, TaskQueue* queue, int background_task=0) 
		: TaskBase(handler, queue), _sessions(sessions), _bg(background_task) {
			start();
        }
	void execute();
	friend class Webapp;
};

typedef enum {SCRIPT_INIT, SCRIPT_QUEUE, SCRIPT_REQUEST, SCRIPT_HANDLERS} script_t;

class Webapp {
private:
	unsigned int nError = 0;
	void runWorker(LuaParam* params, int nArgs, script_t script);
	void compileScript(const char* filename, script_t output);
	std::array<webapp_str_t, WEBAPP_SCRIPTS> scripts;
	//(content) template filename vector
	std::vector<std::string> contentList;

    std::vector<std::string> serverTemplateList;

	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();
	asio::io_service& svc;
	
	void processRequest(Request* r, size_t len);
	
    ctemplate::TemplateDictionary cleanTemplate;
	unsigned int nWorkers;
	friend class WebappTask;
public:
	inline unsigned int GetLastError() { return nError; };
	std::vector<TaskBase*> workers;
	Webapp(asio::io_service& io_svc);
	~Webapp();
	ctemplate::TemplateDictionary* getTemplate(const std::string& page);
	void refresh_templates();
	
	std::vector<Sessions*> sessions;
	std::vector<Database*> databases;
	unsigned int aborted = 0;

	unsigned int background_queue_enabled = 1;
	std::vector<RequestQueue*> requests;
	int node_counter = 0;
	unsigned int port = WEBAPP_PORT_DEFAULT;
	std::mutex cleanupLock;
};

#endif
