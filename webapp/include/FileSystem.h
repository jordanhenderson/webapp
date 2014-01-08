/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef FILE_H
#define FILE_H

#include "Platform.h"
#include "CPlatform.h"
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

	//Load a file, writing file pointer, etc to outFile. Returns success of opening.
	int Open(const std::string& fileName, const std::string& flags="rb", File* outFile=NULL);
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
	long Size(File* file);
	int MakePath(const std::string& path);
	void DeletePath(const std::string& path);
	//Return a vector containing a list of files found in path.
	std::vector<std::string> GetFiles(const std::string& base, const std::string& path, int recurse);
	std::list<std::string> GetFilesAsList(const std::string& base, const std::string& path, int recurse);
};
#endif
