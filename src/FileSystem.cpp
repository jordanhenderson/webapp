#include "FileSystem.h"

File* FileSystem::Open(const TCHAR* fileName, const TCHAR* flags) {
	File* tmpFile = new File;
	//ensure file is opened in binary mode
	int flen = _tcslen(flags);
	TCHAR* actualFlag = new TCHAR[flen + 2];
	if(flags[flen] != 'b') {
		_tcscpy(actualFlag, flags);
		_tcscat(actualFlag, "b");
	}
	tmpFile->pszFile = _tfopen(fileName, flags);
	tmpFile->fileName = fileName;
	tmpFile->flags = actualFlag;
	TCHAR* errType = _T("");
	if(tmpFile->pszFile == NULL) {
		if(flags[0] == 'w' || flags[0] == 'a' || flags[1] == '+')
			errType = _T("writing");
		else if(flags[0] == 'r')
			errType = _T("reading");
		//Debug Message: File Access error: $fileName could not be opened for $errType.
		return NULL;
	}

	//Debug Message: File $fileName opened successfully. Flags: $flags.
	return tmpFile;
}

tstring FileSystem::Read(File* file) {
	if(file == NULL || file->pszFile == NULL)
		return NULL;
	return Process(file, NULL, NULL);
}

void FileSystem::Close(File* file) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fclose(file->pszFile);
}

long FileSystem::Size(File* file) {
	int old = ftell(file->pszFile);
	fseek(file->pszFile, 0L, SEEK_END);
	int sz = ftell(file->pszFile);
	//restore previous position
	fseek(file->pszFile, 0L, old);
	return sz;
}

TCHAR* FileSystem::Process(File* file, void* userdata, ...) {
	if(file == NULL || file->pszFile == NULL)
		return NULL;
	FILE* tmpFile = file->pszFile;
	TCHAR* tmpString = (TCHAR*)malloc(4096 * sizeof(TCHAR));
	while(!feof(tmpFile)) {
		_fgetts(tmpString, 4096, tmpFile);
		int* pos = (int*)&userdata+1;

		if(*pos != NULL) {
			FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(*pos);
			callbackFn(userdata, tmpString);
		}
	}
	return tmpString;
}

void FileSystem::Write(File* file, tstring buffer) {
	if(file == NULL || file->pszFile == NULL)
		return;
	_fputts(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

void FileSystem::WriteLine(File* file, tstring buffer) {
	tstring tmp = tstring(buffer);
	Write(file, tmp.append(ENV_NEWLINE));
}
//Returns true if the specified path exists and can be read.
int FileSystem::Exists(tstring path) {
	struct _stat buf;
	
	if(_tstat(path.c_str(), &buf) == 0)
		return 1;
	return 0;
}