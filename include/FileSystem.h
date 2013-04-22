#ifndef FILE_H
#define FILE_H

#include "Platform.h"
struct File {
	FILE* pszFile;
	const char* fileName;
	const char* flags;
};

struct FileData {
	size_t size;
	char* data;
};

typedef void (*FILE_LINE_CALLBACK)(void*, char*);

class FileSystem {
public:
	//Load a file, returning a File* to the handle.
	static File* Open(const char* fileName, const char* flags);
	//Read an entire file, returning a char* of it's contents.
	//Same as calling ProcessFile with null callback.
	inline static FileData* Read(File* file) {
		return Process(file, NULL, NULL);
	}
	
	//Process a file, using a callback function to process each line.
	static FileData* Process(File* file, void* userdata, void* function);
	//Closes the file.
	static void Close(File* file);
	static void Write(File* file, std::string buffer);
	static void WriteLine(File* file, std::string buffer);
	static int Exists(const char* path);
	static long Size(File* file);
	FileSystem() {};
	~FileSystem() {};
};		
#endif