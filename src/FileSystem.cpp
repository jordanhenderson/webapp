#include "FileSystem.h"

File* FileSystem::Open(const TCHAR* fileName, const TCHAR* flags) {
	File* tmpFile = new File;
	//ensure file is opened in binary mode
	int flen = _tcslen(flags);
	TCHAR* actualFlag = new TCHAR[flen + 2];
	if(flags[flen-1] != 'b') {
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


FileData* FileSystem::Process(File* file, void* userdata, void* callback) {
	if(file == NULL || file->pszFile == NULL)
		return NULL;
	int baseBytes = 0;
	int oldBaseBytes = 0;
	
	//Seek to the beginning.
	
	FILE* tmpFile = file->pszFile;
	rewind(file->pszFile);
	int count = 0;
	int size = Size(file);
	FileData* fdata = new FileData;
	fdata->data = new char[size * sizeof(char) + 1];

		if(callback != NULL) {
			while(!feof(tmpFile)) {
				//Read files line by line
		
					oldBaseBytes = ftell(tmpFile);
					_fgetts(fdata->data + baseBytes, 4096, tmpFile);
					baseBytes += ftell(tmpFile) - oldBaseBytes;
					FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(callback);
					callbackFn(userdata, fdata->data + (oldBaseBytes-count));
				} 
				baseBytes--;
				count = 1;
		}
		else {
			//Simply read the entire file. Hopefully improve performance.

			size_t nRead = fread(fdata->data, sizeof(char), size, tmpFile);
			fdata->data[++nRead] = '\0';
		}
	fdata->size = size;
	return fdata;
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