#include "FileSystem.h"

File* FileSystem::Open(tstring fileName, tstring flags) {
	File* tmpFile = new File;
	tmpFile->pszFile = _tfopen(fileName.c_str(), flags.c_str());
	tmpFile->fileName = fileName;
	tmpFile->flags = flags;
	tstring errType = _T("");
	if(tmpFile->pszFile == NULL) {
		if(flags.compare(_T("w")) == 0 || flags.compare(_T("a")) == 0 || flags.compare(_T("+")) == 0)
			errType = _T("writing");
		else if(flags.compare(_T("r")) == 0)
			errType = _T("reading");
		//Debug Message: File Access error: $fileName could not be opened for $errType.
		return NULL;
	}

	//Debug Message: File $fileName opened successfully. Flags: $flags.
	return tmpFile;
}

tstring FileSystem::Read(File* file) {
	return Process(file, NULL, NULL);
}

void FileSystem::Close(File* file) {
	fclose(file->pszFile);
}

tstring FileSystem::Process(File* file, void* userdata, ...) {
	FILE* tmpFile = file->pszFile;
	TCHAR* tmpString = NULL;
	int count = 4096;
	while(!feof(tmpFile)) {
		tmpString = (TCHAR*)realloc(tmpString, count * sizeof(TCHAR));
		_fgetts((tmpString + count - 4096), 4096, tmpFile);
		int* pos = (int*)&userdata+1;

		if(*pos != NULL) {
			FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(*pos);
			callbackFn(userdata, (tmpString + count - 4096));
		}
		count += 4096;
	}
	return tstring(tmpString);
}

void FileSystem::Write(File* file, tstring buffer) {
	_fputts(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

//Returns true if the specified path exists and can be read.
int FileSystem::Exists(tstring path) {
	struct _stat buf;
	if(_tstat(path.c_str(), &buf) == 0)
		return 1;
	return 0;
}