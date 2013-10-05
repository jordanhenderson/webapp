#include <asio.hpp>
#include "Logging.h"
#include "Webapp.h"
#include "Image.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include <sha.h>
#include <tbb/task_scheduler_init.h>


using namespace std;
using namespace ctemplate;
using namespace asio;
using namespace asio::ip;

Webapp::Webapp(Parameters* params, asio::io_service& io_svc) :  contentTemplates(""),
										params(params),
										basepath(params->get("basepath")),
										dbpath(params->get("dbpath")), 
										svc(io_svc) {

	database.connect(DATABASE_TYPE_SQLITE, dbpath);
	//Enable pragma foreign keys.
	database.exec(PRAGMA_FOREIGN);

	//Create the schema. Running this even when tables exist prevent issues later on.
	string schema = CREATE_DATABASE;
	vector<string> schema_queries;
	tokenize(schema, schema_queries, ";");
	for(string s : schema_queries) {
		database.exec(s);
	}
	
	refresh_templates();

	//Create function map.
	APIMAP(functionMap);

	systemScripts.reserve(SYSTEM_SCRIPT_COUNT);

	queue_process_thread = std::thread([this]() {
		while(!shutdown_handler) {
			std::thread* nextThread;
			processQueue.pop(nextThread);
			currentID = nextThread->get_id();
			cv_thread_start.notify_all();
		}
	});

	queue_process_thread.detach();


	asio::io_service::work wrk = asio::io_service::work(svc);
	tcp::endpoint endpoint(tcp::v4(), 5000);
	acceptor = new tcp::acceptor(svc, endpoint, true);
	accept_message();
	svc.run();
}

void Webapp::accept_message() {
	ip::tcp::socket* s = new ip::tcp::socket(svc);
	acceptor->async_accept(*s, [this, s](const asio::error_code& error) {
		asio::streambuf buf;

		asio::write(*s, asio::buffer("HTTP/1.0 200 OK\r\nContent-type: text/html\r\nStatus: 404 Not Found\r\n\r\nThe page you requested cannot be found (404)."));
		while(1);
		accept_message();
	});
}

void Webapp::process_message(std::vector<char>& msg) {

}

//PRODUCER
void Webapp::process_thread(std::thread* t) {
	processQueue.push(t);
}

Webapp::~Webapp() {
	//Clean up client template files.
	for(TemplateData file: clientTemplateFiles) {
		delete(file.data);
	}

	shutdown_handler = 1;
	processQueue.abort();
}

void Webapp::createWorker() {
	//Create a LUA worker on the current thread/task.
	//LuaParam luaparams[] = {{"requests", &requests}, {"sessions", &sessions}, {"app", this}};
	//This call will not return until the lua worker is finished.
	//runScript(SYSTEM_SCRIPT_PROCESS, (LuaParam*)luaparams, 3);

}

TemplateDictionary* Webapp::getTemplate(const char* page) {
	if(contains(contentList, page)) {
		TemplateDictionary *d = contentTemplates.MakeCopy("");
		for(string data: serverTemplateFiles) {
			d->AddIncludeDictionary(data)->SetFilename(data);
		}
		for(TemplateData file: clientTemplateFiles) {
			d->SetValueWithoutCopy(file.name, TemplateString(file.data->data, file.data->size));
		}
		return d;
	}
	return NULL;
}

int Webapp::genThumb(const char* file, double shortmax, double longmax) {
	string storepath = database.select_single(SELECT_SYSTEM("store_path"));
	string thumbspath = database.select_single(SELECT_SYSTEM("thumbs_path"));
	string imagepath = basepath + '/' + storepath + '/' + file;
	string thumbpath = basepath + '/' + thumbspath + '/' + file;
	
	//Check if thumb already exists.
	if(FileSystem::Open(thumbpath))
		return ERROR_SUCCESS;

	Image image(imagepath);
	int err = image.GetLastError();
	if(image.GetLastError() != ERROR_SUCCESS){
		return err;
	}
	//Calculate correct size (keeping aspect ratio) to shrink image to.
	double wRatio = 1;
	double hRatio = 1;
	double width = image.getWidth();
	double height = image.getHeight();
	if(width >= height) {
		if(width > longmax || height > shortmax) {
			wRatio = longmax / width;
			hRatio = shortmax / height;
		}
	}
	else {
		if(height > longmax || width > shortmax) {
			wRatio = shortmax / width;
			hRatio = longmax / height;
		}
	}

	double ratio = min(wRatio, hRatio);
	double newWidth = width * ratio;
	double newHeight = height * ratio;


    image.resize(newWidth, newHeight);
	image.save(thumbpath);

	if(!FileSystem::Open(thumbpath)) {
		logger->printf("An error occured generating %s.", thumbpath.c_str());
	}

	return ERROR_SUCCESS;
}

int Webapp::hasAlbums() {
	string albumsStr = database.select_single(SELECT_ALBUM_COUNT, NULL, "0");
	int nAlbums = stoi(albumsStr);
	if(nAlbums == 0) {
		return 0;
	}
	return 1;
}

void Webapp::refresh_templates() {
	//Force reload templates
	mutable_default_template_cache()->ReloadAllIfChanged(TemplateCache::IMMEDIATE_RELOAD);
	//Load content files (applicable templates)
	contentList.clear();
	string basepath = this->basepath + '/';
	string templatepath = basepath + "content/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string s : files) {
			//Preload templates.
			LoadTemplate(templatepath + s, STRIP_WHITESPACE);
			contentList.push_back(s);
		}
	}

	//Load server templates.
	templatepath = basepath + "templates/server/";
	serverTemplateFiles.clear();

	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		serverTemplateFiles.reserve(files.size());
		for(string s: files) {
			File f;
			FileData data;
			FileSystem::Open(templatepath + s, "rb", &f);
			string template_name = "T_" + s.substr(0, s.find_last_of("."));
			FileSystem::Read(&f, &data);
			std::transform(template_name.begin(), template_name.end(), template_name.begin(), ::toupper);
			mutable_default_template_cache()->Delete(template_name);
			StringToTemplateCache(template_name, data.data, data.size, STRIP_WHITESPACE);
			serverTemplateFiles.push_back(template_name);
		}
	}
	
	//Load client templates (inline insertion into content templates) - these are Handbrake (JS) templates.
	//First delete existing filedata. This should be optimised later.
	for(TemplateData file: clientTemplateFiles) {
		delete file.data;
	}
	clientTemplateFiles.clear();

	templatepath = basepath + "templates/client/";
	{
		vector<string> files = FileSystem::GetFiles(templatepath, "", 0);
		contentList.reserve(files.size());
		for(string s: FileSystem::GetFiles(templatepath, "", 0)) {
			File f;
			FileData* data = new FileData();
			FileSystem::Open(templatepath + s, "rb", &f);
			string template_name = "T_" + s.substr(0, s.find_last_of("."));
			FileSystem::Read(&f, data);
			std::transform(template_name.begin(), template_name.end(), template_name.begin(), ::toupper);
			TemplateData d = {template_name, data};
			clientTemplateFiles.push_back(d);
		}
	}
}


void Webapp::addFile(const string& file, int nGenThumbs, const string& thumbspath, const string& path, const string& date, const string& albumID) {
	string thumbID;
	//Generate thumb.
	if(nGenThumbs) {
		FileSystem::MakePath(basepath + '/' + thumbspath + '/' + path + '/' + file);
		if(genThumb((path + '/' + file).c_str(), 200, 200) == ERROR_SUCCESS) {
			QueryRow params;
			params.push_back(path + '/' + file);
			int nThumbID = database.exec(INSERT_THUMB, &params);
			if(nThumbID > 0) {
				thumbID = to_string(nThumbID);
			}
		}
	}

	//Insert file 
	QueryRow params;
	params.push_back(file);
	params.push_back(file);
	params.push_back(date);
	int fileID;
	if(!thumbID.empty()) {
		params.push_back(thumbID);
		fileID = database.exec(INSERT_FILE, &params);
	} else {
		fileID = database.exec(INSERT_FILE_NO_THUMB, &params);
	}
	//Add entry into albumFiles
	QueryRow fparams;
	fparams.push_back(albumID);
	fparams.push_back(to_string(fileID));
	database.exec(INSERT_ALBUM_FILE, &fparams);
}

//Gallery specific functionality
int Webapp::getDuplicates( string& name, string& path ) {
	QueryRow params;
	params.push_back(name);
	params.push_back(path);
	string dupAlbumStr = database.select_single(SELECT_DUPLICATE_ALBUM_COUNT, &params);
	return stoi(dupAlbumStr);
}