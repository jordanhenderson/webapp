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

typedef void (*FILE_LINE_CALLBACK)(void*, char*, int);

class FileSystem {
public:
	//Load a file, returning a File* to the handle.
	static std::unique_ptr<File> Open(const char* fileName, const char* flags);
	//Read an entire file, returning a char* of it's contents.
	//Same as calling ProcessFile with null callback.
	inline static std::unique_ptr<FileData> Read(std::unique_ptr<File>&  file) {
		return Process(file, NULL, NULL);
	}
	
	//Process a file, using a callback function to process each line.
	static std::unique_ptr<FileData> Process(std::unique_ptr<File>& file, void* userdata, void* function);
	//Closes the file.
	static void Close(std::unique_ptr<File>&  file);
	static void Write(std::unique_ptr<File>&  file, std::string buffer);
	static void WriteLine(std::unique_ptr<File>&  file, std::string buffer);
	static int Exists(const char* path);
	static long Size(std::unique_ptr<File>& file);
	FileSystem() {};
	~FileSystem() {};
};		
#endif