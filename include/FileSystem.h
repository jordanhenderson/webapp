#ifndef FILE_H
#define FILE_H


#include "Platform.h"
class File {
public:
	FILE* pszFile;
	std::string fileName;
	std::string flags;
	~File();
	File();
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
	void Open(const std::string& fileName, const std::string& flags, File* outFile);
	//Read an entire file, returning a char* of it's contents.
	//Same as calling ProcessFile with null callback.
	//Process a file, using a callback function to process each line.
	void Process(File* file, void* userdata, void* function, FileData* outData);

	inline void Read(File* file, FileData* data) {
		return Process(file, NULL, NULL, data);
	}
	
	//Closes the file.
	void Close(File*  file);
	void Write(File*  file, const std::string& buffer);
	void WriteLine(File*  file, const std::string& buffer);
	int Exists(const std::string& path);
	inline int Exists(std::string& path) {
		return Exists(path.c_str());	
	};
	long Size(File* file);
	void MakePath(const std::string& path);
	void DeletePath(const std::string& path);
	//Return a vector containing a list of files found in path.
	std::vector<std::string> GetFiles(const std::string& base, const std::string& path, int recurse);
	std::list<std::string> GetFilesAsList(const std::string& base, const std::string& path, int recurse);
};		
#endif