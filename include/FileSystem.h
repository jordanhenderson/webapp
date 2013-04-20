#ifndef FILE_H
#define FILE_H

#include "Platform.h"
struct File {
	FILE* pszFile;
	const TCHAR* fileName;
	const TCHAR* flags;
};

struct FileData {
	size_t size;
	char* data;
};

typedef void (*FILE_LINE_CALLBACK)(void*, TCHAR*);

class FileSystem {
public:
	//Load a file, returning a File* to the handle.
	static File* Open(const TCHAR* fileName, const TCHAR* flags);
	//Read an entire file, returning a char* of it's contents.
	//Same as calling ProcessFile with null callback.
	inline static FileData* Read(File* file) {
		return Process(file, NULL, NULL);
	}
	
	//Process a file, using a callback function to process each line.
	static FileData* Process(File* file, void* userdata, void* function);
	//Closes the file.
	static void Close(File* file);
	static void Write(File* file, tstring buffer);
	static void WriteLine(File* file, tstring buffer);
	static int Exists(tstring path);
	static long Size(File* file);
	FileSystem() {};
	~FileSystem() {};
};		
#endif