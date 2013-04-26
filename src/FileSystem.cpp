#include "FileSystem.h"
#include <codecvt>
using namespace std;
File* FileSystem::Open(const char* fileName, const char* flags) {
	File* tmpFile = new File;
	locale loc;
	//ensure file is opened in binary mode
	int flen = strlen(flags);
	char* actualFlag = new char[flen + 2];
	if(flags[flen-1] != 'b') {
		strcpy(actualFlag, flags);
		strcat(actualFlag, "b");
	}
	
#ifdef WIN32
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	tmpFile->pszFile = _wfopen(conv.from_bytes(fileName).c_str(), conv.from_bytes(flags).c_str());
#else
	tmpFile->pszFile = fopen(fileName, flags);
#endif
	tmpFile->fileName = fileName;
	tmpFile->flags = actualFlag;
	char* errType = "";
	if(tmpFile->pszFile == NULL) {
		if(flags[0] == 'w' || flags[0] == 'a' || flags[1] == '+')
			errType = "writing";
		else if(flags[0] == 'r')
			errType = "reading";
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
					fgets(fdata->data + baseBytes, 4096, tmpFile);
					int nBytesRead = ftell(tmpFile) - oldBaseBytes;
					baseBytes += nBytesRead;
					FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(callback);
					callbackFn(userdata, fdata->data + (oldBaseBytes-count), nBytesRead);
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

void FileSystem::Write(File* file, string buffer) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fputs(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

void FileSystem::WriteLine(File* file, string buffer) {
	string tmp = string(buffer);
	Write(file, tmp.append(ENV_NEWLINE));
}
//Returns true if the specified path exists and can be read.
int FileSystem::Exists(const char* path) {
	struct stat buf;
	
	if(stat(path, &buf) == 0)
		return 1;
	return 0;
}