/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "FileSystem.h"
#include "tinydir.h"
using namespace std;
int FileSystem::Open(const string& fileName, const string& flags, File* outFile) {
	//ensure file is opened in binary mode
	int flen = flags.length();
	string actualFlag = flags;
	if(flags[flen-1] != 'b') {
		actualFlag.append("b");
	}
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

FileData::~FileData() {
	delete[] data;
}

File::~File() {
	if(pszFile != NULL)
		fclose(pszFile);
}

File::File() {
	pszFile = NULL;
}

void FileSystem::Close(File* file) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fclose(file->pszFile);
	file->pszFile = NULL;
}

long FileSystem::Size(File* file) {
	int old = ftell(file->pszFile);
	fseek(file->pszFile, 0L, SEEK_END);
	int sz = ftell(file->pszFile);
	//restore previous position
	fseek(file->pszFile, 0L, old);
	return sz;
}


void FileSystem::Process(File* file, void* userdata, void* callback, FileData* outData) {
	if(file == NULL || file->pszFile == NULL || outData == NULL)
		return;
	
	//Seek to the beginning.
	
	FILE* tmpFile = file->pszFile;
	rewind(file->pszFile);
	int size = Size(file);
	
	outData->data = new char[size * sizeof(char) + 1];

		if(callback != NULL) {
			int baseBytes = 0;
			int count = 0;
			while(!feof(tmpFile)) {
					
				//Read files line by line
					int oldBaseBytes = ftell(tmpFile);
					fgets(outData->data + baseBytes, 4096, tmpFile);
					int nBytesRead = ftell(tmpFile) - oldBaseBytes;
					baseBytes += nBytesRead;
					FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(callback);
					callbackFn(userdata, outData->data + (oldBaseBytes-count), nBytesRead);
				} 
				baseBytes--;
				
		}
		else {
			//Simply read the entire file. Hopefully improve performance.

			size_t nRead = fread(outData->data, sizeof(char), size, tmpFile);
			
			outData->data[nRead] = '\0';
		}
	outData->size = size;
	return;
}

void FileSystem::Write(File* file, const string& buffer) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fputs(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

void FileSystem::WriteLine(File* file, const string& buffer) {
	Write(file, string(buffer).append(ENV_NEWLINE));
}

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

vector<string> FileSystem::GetFiles(const string& base, const string& path, int recurse) {
	vector<string> files;

	tinydir_dir dir;
	string abase = base + '/' + path;
	tinydir_open(&dir, abase.c_str());
	while(dir.has_next) {
		
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		string dpath = path.empty() ? file.name : path + '/' + file.name;
		if(!file.is_dir) 
			files.push_back(dpath);
		else if(recurse && file.name[0] != '.') {
			//Get+append files from subdirectory. (recursive case)
			vector<string> rfiles = GetFiles(base, dpath, 1);
			files.insert(files.end(), rfiles.begin(), rfiles.end());
		}
		tinydir_next(&dir);
		
	}
	tinydir_close(&dir);
		
	return files;
	
}


list<string> FileSystem::GetFilesAsList(const string& base, const string& path, int recurse) {
	list<string> files;

	tinydir_dir dir;
	string abase = base + '/' + path;
	tinydir_open(&dir, abase.c_str());
	while(dir.has_next) {
		
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		string dpath = path.empty() ? file.name : path + '/' + file.name;
		if(!file.is_dir) 
			files.push_back(dpath);
		else if(recurse && file.name[0] != '.') {
			//Get+append files from subdirectory. (recursive case)
			list<string> rfiles = GetFilesAsList(base, dpath, 1);
			files.insert(files.end(), rfiles.begin(), rfiles.end());
		}
		tinydir_next(&dir);
		
	}
	tinydir_close(&dir);
		
	return files;
	
}