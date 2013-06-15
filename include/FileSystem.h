#ifndef FILE_H
#define FILE_H

#include "Platform.h"
struct File {
public:
	FILE* pszFile;
	std::string fileName;
	std::string flags;
	~File();
};

class FileData {
public:
	size_t size;
	char* data;
	~FileData();
};

typedef void (*FILE_LINE_CALLBACK)(void*, char*, int);

namespace FileSystem {

	//Load a file, returning a File* to the handle.
	std::unique_ptr<File> Open(const std::string& fileName, const std::string& flags);
	//Read an entire file, returning a char* of it's contents.
	//Same as calling ProcessFile with null callback.
	//Process a file, using a callback function to process each line.
	std::unique_ptr<FileData> Process(std::unique_ptr<File>& file, void* userdata, void* function);

	inline std::unique_ptr<FileData> Read(std::unique_ptr<File>& file) {
		return Process(file, NULL, NULL);
	}
	
	//Closes the file.
	void Close(std::unique_ptr<File>&  file);
	void Write(std::unique_ptr<File>&  file, std::string& buffer);
	void WriteLine(std::unique_ptr<File>&  file, std::string& buffer);
	int Exists(const std::string& path);
	inline int Exists(std::string& path) {
		return Exists(path.c_str());	
	};
	long Size(std::unique_ptr<File>& file);
	void MakePath(const std::string& path);
	void DeletePath(const std::string& path);
	//Return a vector containing a list of files found in path.
	std::vector<std::string> GetFiles(const std::string& base, const std::string& path, int recurse);
};		
#endif