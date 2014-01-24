/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef FILE_H
#define FILE_H

#include "Platform.h"

class webapp_str_t;

class File {
	webapp_str_t _fileName;
	webapp_str_t _flags;
	webapp_str_t buffer;
	FILE* pszFile = NULL;
	uint64_t size = 0;
	int refresh = 1;
public:
	int Open(const webapp_str_t& fileName, const webapp_str_t& flags);
	File() : _flags(2) {}
	File(const webapp_str_t& fileName, const webapp_str_t& flags) : File() {
		Open(fileName, flags);
	}
	~File();

	uint16_t Read(uint16_t n_bytes);
	void Close();
	void Cleanup();
	uint64_t Size();
	void Refresh();
	
	void Write(const webapp_str_t& buffer);
	void WriteLine(const webapp_str_t& buffer);
	FILE* GetPointer();
	FILE* Detach();
};

namespace FileSystem {
	int MakePath(const webapp_str_t& path);
	void DeletePath(const webapp_str_t& path);
};
#endif
