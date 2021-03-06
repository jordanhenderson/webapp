/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"
#include "FileSystem.h"
#include <tinydir.h>

using namespace std;

/**
 * Open a File object.
 * @param fileName the file to open
 * @param flags the file mode used to open the file.
 * @return the success of the file.
*/
int File::Open(const webapp_str_t& fileName, const webapp_str_t& flags)
{
	FILE* tmpFile;

#ifdef _WIN32
	wchar_t* wfileName, *wflags;
	wfileName = strtowide(fileName.data);
	wflags = strtowide(flags.data);

	tmpFile = _wfopen(wfileName, wflags);
	delete[] wfileName;
	delete[] wflags;
#else
	tmpFile = fopen(fileName.data, flags.data);
#endif
	int success = (tmpFile != NULL);
	if(success) {
		_fileName = webapp_str_t(fileName);
		_flags = webapp_str_t(flags);
		pszFile = tmpFile;
	}

	//Update the file size.
	Size();

	return success;
}

/**
 * Destroy the File object.
*/
File::~File()
{
	if(pszFile != NULL) fclose(pszFile);
}

/**
 * Detach the File pointer (pszFile) from the object.
 * Allows other libraries to handle closing it (If absolutely necessary)
 * @return the detached file pointer.
*/
FILE* File::Detach()
{
	FILE* tmp = pszFile;
	pszFile = NULL;
	return tmp;
}

/**
 * Get the internal file pointer.
 * @return the file pointer.
*/
FILE* File::GetPointer()
{
	return pszFile;
}

/**
 * Signal any cached data (stat etc) to be reloaded at next pass.
*/
void File::Refresh()
{
	refresh = 1;
}

/**
 * Close a File object, setting pointers to NULL.
*/
void File::Close()
{
	if(pszFile == NULL) return;
	fclose(pszFile);
	pszFile = NULL;
}

/**
 * Seek a file to the end, retrieving file size, then restore its previous
 * position.
 * @return file size
*/
int64_t File::Size()
{
	if(refresh == 1) {
		//Seek the file to the end, retrieve
		int old = ftell64(pszFile);
		fseek64(pszFile, 0L, SEEK_END);
		size = ftell64(pszFile);
		//restore previous position
		fseek64(pszFile, 0L, old);
		refresh = 0;
	}
	return size;
}

/**
 * Reads a file, storing data in buffer.
 * Data returned is cleaned up by calling File::Cleanup();
 * @param out the char* to store read data
 * @return the file size
*/
int16_t File::Read(int16_t n_bytes)
{
	//Get the file size.
	int64_t size = Size();
	char* buf = NULL;

	int16_t to_read = size < n_bytes ? size : n_bytes;
	//Seek to the beginning.
	FILE* tmpFile = pszFile;
	rewind(pszFile);

	//Allocate room for the data.
	buf = new char[to_read];
	size_t nRead = fread(buf, sizeof(char), to_read, tmpFile);

	if(buffer.data != NULL) delete[] buffer.data;
	buffer.data = buf;
	buffer.len = nRead;
	buffer.allocated = 1;

	return (int16_t)nRead;
}

/**
 * Write the provided buffer to a file.
 * Writes according to filemode specified upon opening the file.
 * @param buffer the buffer to write
*/
void File::Write(const webapp_str_t& buffer)
{
	if(pszFile == NULL) return;
	fwrite(buffer.data, sizeof(char), buffer.len, pszFile);
	fflush(pszFile);
}

/**
 * Create a directory tree recursively using tinydir.
 * @param path the directory tree which should be created.
 * @return whether the directory tree was created successfully.
*/
int FileSystem::MakePath(const webapp_str_t& path)
{
	//Recurisvely make a path structure.
	int nFilename = tinydir_todir(path.data, path.len);

	for(int i = 0; i <= (int)path.len - nFilename; i++) {
		if(path.data[i] == '/' || path.data[i] == 0) {
			path.data[i] = 0;
			tinydir_dir dir;
			if(tinydir_open(&dir, path.data) == -1) {
				//make the directory
				if(!tinydir_create(path.data)) return 0;
			}
			tinydir_close(&dir);
			path.data[i] = '/';
		}
	}
	return 1;
}

/**
 * Delete a path recursively, including all files contained within.
 * @param path the directory to delete.
*/
void FileSystem::DeletePath(const webapp_str_t& path)
{
	tinydir_dir dir;
	tinydir_open(&dir, path.data);

	//Delete all files/directories.
	while(dir.has_next) {
		tinydir_file f;
		tinydir_readfile(&dir, &f);
		if(!f.is_dir) {
#ifdef _WIN32
			wchar_t* file = strtowide(f.path);
			_wunlink(file);
			delete[] file;
#else
			unlink(f.path);
#endif
		} else if(f.name[0] != '.') {
			DeletePath(f.path);
		}
		tinydir_next(&dir);
	}

	tinydir_close(&dir);
#ifdef _WIN32
	wchar_t* wPath = strtowide(path.data);
	_wrmdir(wPath);
	delete[] wPath;
#else
	rmdir(path.data);
#endif
}
