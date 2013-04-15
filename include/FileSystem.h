#ifndef FILE_H
#define FILE_H

#include "Platform.h"
struct File {
	FILE* pszFile;
	tstring fileName;
	tstring flags;
};

typedef void (*FILE_LINE_CALLBACK)(void*, tstring);

class FileSystem {
public:
	//Load a file, returning a File* to the handle.
	static File* Open(tstring fileName, tstring flags);
	//Read an entire file, returning a char* of it's contents.
	//The File* will be closed after reading. Same as calling ProcessFile with null callback.
	static tstring Read(File* file);
	//Process a file, using a callback function to process each line.
	static tstring Process(File* file, void* userdata, ...);
	//Closes the file.
	static void Close(File* file);
	static void Write(File* file, tstring buffer);
	static int Exists(tstring path);
	FileSystem() {};
	~FileSystem() {};
};		
#endif