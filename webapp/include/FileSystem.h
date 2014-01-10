/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef FILE_H
#define FILE_H

#include "Platform.h"
#include "CPlatform.h"

#ifdef _WIN32
#define ENV_NEWLINE "\r\n"
#else
#define ENV_NEWLINE "\n"
#include <unistd.h>
#endif

class File {
public:
	FILE* pszFile = NULL;
	std::string fileName;
	std::string flags;
	~File();
	File() {};
};

class FileData {
public:
	size_t size = 0;
	char* data = NULL;
	~FileData();
	FileData() {};
};

namespace FileSystem {
	int Open(const std::string& fileName, const std::string& flags="rb", File* outFile=NULL);
	void Read(File* file, FileData* data);
	void Close(File* file);
	void Write(File* file, const std::string& buffer);
	void WriteLine(File* file, const std::string& buffer);
	long Size(File* file);
	int MakePath(const std::string& path);
	void DeletePath(const std::string& path);
	std::vector<std::string> GetFiles(const std::string& base, const std::string& path, int recurse);
};
#endif
