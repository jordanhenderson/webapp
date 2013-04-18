// gallery.cpp : Defines the entry point for the console application.
//
#include "Platform.h"
#include "Logging.h"
#include "Gallery.h"


Gallery::Gallery(Parameters* params, Logging* logger) {
	this->logger = logger;
}



tstring Gallery::process(char** request) {
	tstring data = HTML_HEADER;
	File* f = FileSystem::Open("gallery/templates/index.html", "r");
	data.append(FileSystem::Read(f));
	return data.c_str();
	
}

