/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef WEBAPP_H
#define WEBAPP_H

#include <asio.hpp>
#include "Platform.h"
#include "CPlatform.h"
#include "FileSystem.h"
#include "Database.h"
#include "Session.h"
#include "Schema.h"
#include <ctemplate/template.h>
#include "readerwriterqueue.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lpeg.h"
#include "cjson.h"
}

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
	void notify() {cv.notify_one();}
};

struct Process {
	webapp_str_t* func = NULL;
	webapp_str_t* vars = NULL;
};

template<typename T>
class LockedQueue : public TaskQueue {
private:
	moodycamel::ReaderWriterQueue<T> queue;
public:
	//Each operation (enqueue, dequeue) must be done single threaded.
	//We need MP (Multi-Producer), so lock production.
	void enqueue(T i) { cv_mutex.lock(); queue.enqueue(i); cv_mutex.unlock(); cv.notify_one();  }
	T dequeue() { 
		T r;
		{
			std::unique_lock<std::mutex> lk(cv_mutex);
			while (!queue.try_dequeue(r) && !aborted) cv.wait(lk);
		}
		if(aborted) return NULL;
		return r; 
	}
    LockedQueue() : queue(WEBAPP_DEFAULT_QUEUESIZE) {}
};


class Webapp;
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
	void handleCleanup();
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
	void runWorker(LuaParam* params, int nArgs, script_t script);
	void compileScript(const char* filename, script_t output);
	std::array<webapp_str_t, WEBAPP_SCRIPTS> scripts;
	//(content) template filename vector
	std::vector<std::string> contentList;
	std::vector<std::string> serverTemplateList;
	
	//Keep track of dynamic databases
	std::unordered_map<size_t, Database*> databases;
	std::atomic<size_t> db_count{0};

	//IPC api
	asio::ip::tcp::acceptor* acceptor;
	void accept_message();
	void accept_message_async(asio::ip::tcp::socket*, const asio::error_code&);
	void process_message_async(Request *r, const asio::error_code&, std::size_t);
	void process_request(Request* r, size_t len);
	void process_request_async(Request* r, const asio::error_code&, std::size_t);
	asio::io_service& svc;

	
	
	ctemplate::TemplateDictionary cleanTemplate;
	unsigned int nWorkers = WEBAPP_NUM_THREADS - 1;
	unsigned int node_counter = 0;
	friend class WebappTask;
public:
	Webapp(asio::io_service& io_svc);
	~Webapp();
	void  Start() { svc.run(); };
	ctemplate::TemplateDictionary* getTemplate(const std::string& page);
	Database* CreateDatabase();
	Database* GetDatabase(size_t index);
	void DestroyDatabase(Database*);
	void refresh_templates();
	void reload_all();
	
	std::vector<TaskBase*> workers;
	std::vector<Sessions*> sessions;
	std::vector<LockedQueue<Request*>*> request_queues;
	
	unsigned int aborted = 0;
	unsigned int background_queue_enabled = 1;
	
	unsigned int port = WEBAPP_PORT_DEFAULT;
	std::mutex cleanupLock;
};

#endif //WEBAPP_H
