/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "FileSystem.h"
#include "tinydir.h"

using namespace std;

/**
 * Open a File object.
 * @param fileName the file to open
 * @param flags the file mode used to open the file.
 * @param outFile the File object to open.
 * @return the success of the file.
*/
int FileSystem::Open(const string& fileName, const string& flags, File* outFile) {
	//ensure file is opened in binary mode
	int flag_len = flags.length();
	string actualFlag = flags;
	if(flags[flag_len - 1] != 'b') actualFlag += 'b';
	FILE* tmpFile;
#ifdef _WIN32
	wchar_t* wfileName, *wflags;
	wfileName = strtowide(fileName.c_str());
	wflags = strtowide(flags.c_str());

	tmpFile = _wfopen(wfileName, wflags);
	delete[] wfileName;
	delete[] wflags;
#else
	tmpFile = fopen(fileName.c_str(), flags.c_str());
#endif
	int success = (tmpFile != NULL);
	if(success && outFile != NULL) {
		outFile->fileName = fileName;
		outFile->flags = actualFlag;
		outFile->pszFile = tmpFile;
	} else if(success) 
		fclose(tmpFile);
	
	return success;
}

/**
 * Destroy the FileData object.
*/
FileData::~FileData() {
	if (data != NULL) delete[] data;
}

/**
 * Destroy the File object.
*/
File::~File() {
	if(pszFile != NULL) fclose(pszFile);
}

/**
 * Close a File object, setting pointers to NULL.
 * @param file the File object to close.
*/
void FileSystem::Close(File* file) {
	if(file == NULL || file->pszFile == NULL) return;
	fclose(file->pszFile);
	file->pszFile = NULL;
}

/**
 * Seek a file to the end, retrieving file size, then restore its previous
 * position.
 * @param file the File object 
 * @return file size
*/
long FileSystem::Size(File* file) {
	//Seek the file to the end, retrieve
	int old = ftell(file->pszFile);
	fseek(file->pszFile, 0L, SEEK_END);
	int sz = ftell(file->pszFile);
	//restore previous position
	fseek(file->pszFile, 0L, old);
	return sz;
}

/**
 * Read a file in full, storing the read data in outData.
 * @param file the File object to read.
 * @param outData the location to store the read file.
*/
void FileSystem::Read(File* file, FileData* outData) {
	if(file == NULL || file->pszFile == NULL || outData == NULL) return;
	//Clean up existing file data, if any.
	if(outData->data != NULL) delete[] outData;
	
	//Seek to the beginning.
	FILE* tmpFile = file->pszFile;
	rewind(file->pszFile);
	
	//Get the file size.
	int size = Size(file);
	
	//Allocate room for the data.
	outData->data = new char[size * sizeof(char)];
	//Read the entire file into memory. Not for large files.
	size_t nRead = fread(outData->data, sizeof(char), size, tmpFile);
	
	//Update the File size.
	outData->size = size;
	return;
}

/**
 * Write the provided buffer to a file object.
 * Writes according to filemode specified upon opening the file.
 * @param file the target File object
 * @param buffer the buffer to write
*/
void FileSystem::Write(File* file, const string& buffer) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fputs(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

/**
 * Write the provided line to a file object, appending an appropriate newline
 * @param file the target File object
 * @param buffer the buffer to write, followed by a NEWLINE character
 * @see Write
*/
void FileSystem::WriteLine(File* file, const string& buffer) {
	Write(file, string(buffer).append(ENV_NEWLINE));
}

/**
 * Create a directory tree recursively using tinydir.
 * @param path the directory tree which should be created.
 * @return whether the directory tree was created successfully.
*/
int FileSystem::MakePath(const string& path) {
	//Recurisvely make a path structure.
	string tmpPath = string(path);
	int nFilename = tinydir_todir((char*)tmpPath.c_str(), tmpPath.length());
	
	for(int i = 0; i <= tmpPath.length() - nFilename; i++) {
		if(tmpPath[i] == '/' || tmpPath[i] == 0) {
			tmpPath[i] = 0;
			tinydir_dir dir;
			if(tinydir_open(&dir, tmpPath.c_str()) == -1) {
				//make the directory
				if(!tinydir_create(tmpPath.c_str())) return 0;
			}
			tinydir_close(&dir);
			tmpPath[i] = '/';
		}
	}
	return 1;
}

/**
 * Delete a path recursively, including all files contained within.
 * @param path the directory to delete.
*/
void FileSystem::DeletePath(const string& path) {
	tinydir_dir dir;
	tinydir_open(&dir, path.c_str());

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
		}
		else if(f.name[0] != '.') {
			DeletePath(f.path);
		}
		tinydir_next(&dir);
	}
	
	tinydir_close(&dir);
#ifdef _WIN32
	wchar_t* wPath = strtowide(path.c_str());
	_wrmdir(wPath);
	delete[] wPath;
#else
	rmdir(path.c_str());
#endif
}

/**
 * Get a vector containing a list of files within a directory.
 * @param base the base directory to begin searching.
 * @param path the path to prepend to each filename in the output.
 * @param recurse whether to recurse into subdirectories.
 * @return a vector containing a list of files within the directory path.
*/
vector<string> FileSystem::GetFiles(const string& base, const string& path, int recurse) {
	vector<string> files;
	tinydir_dir dir;
	string start_dir = base + '/' + path;
	tinydir_open(&dir, start_dir.c_str());
	//Iterate over each item in the directory.
	while(dir.has_next) {
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		//If path provided prepend it, otherwise just store the filename.
		string dir_path = path.empty() ? file.name : path + '/' + file.name;
		//Push the filename onto the resultant vector.
		if(!file.is_dir) files.push_back(dir_path);
		else if(recurse && file.name[0] != '.') {
			//Retrieve and append files from subdirectory.
			vector<string> subdir_files = GetFiles(base, dir_path, 1);
			files.insert(files.end(), subdir_files.begin(), subdir_files.end());
		}
		tinydir_next(&dir);
	}
	
	tinydir_close(&dir);
	return files;
}
