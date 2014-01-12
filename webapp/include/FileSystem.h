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
	FILE* pszFile = NULL;
	std::string _fileName;
	std::string _flags;
	long long size = 0;
	int refresh = 1;
	std::vector<const char*> buffers;
public:
	int Open(const std::string& fileName, const std::string& flags="rb");
	File() {}
	File(const std::string& fileName, const std::string& flags="rb") {
		Open(fileName, flags); }
	~File();

	const char* Read();
	void Close();
	void Cleanup();
	long long Size();
	void Refresh();
	
	void Write(const std::string& buffer);
	void WriteLine(const std::string& buffer);
	FILE* GetPointer();
	FILE* Detach();
};

namespace FileSystem {
	int MakePath(const std::string& path);
	void DeletePath(const std::string& path);
	std::vector<std::string> GetFiles(const std::string& base,
									  const std::string& path, int recurse);
};
#endif
