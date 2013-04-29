#include "FileSystem.h"
using namespace std;
unique_ptr<File> FileSystem::Open(const string& fileName, const string& flags) {
	unique_ptr<File> tmpFile = unique_ptr<File>(new File);
	//ensure file is opened in binary mode
	int flen = flags.length();
	string actualFlag = flags;
	if(flags[flen-1] != 'b') {
		actualFlag.append("b");
	}
	
#ifdef WIN32
	std::wstring wfileName, wflags;
	wfileName.assign(fileName.begin(), fileName.end());
	wflags.assign(actualFlag.begin(), actualFlag.end());
	tmpFile->pszFile = _wfopen(wfileName.c_str(), wflags.c_str());
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
		return move(tmpFile);
	}

	//Debug Message: File $fileName opened successfully. Flags: $flags.
	return move(tmpFile);
}

FileData::~FileData() {
	delete[] data;
}

File::~File() {
	if(pszFile != NULL)
		fclose(pszFile);
}

void FileSystem::Close(unique_ptr<File>& file) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fclose(file->pszFile);
	file->pszFile = NULL;
}

long FileSystem::Size(unique_ptr<File>& file) {
	int old = ftell(file->pszFile);
	fseek(file->pszFile, 0L, SEEK_END);
	int sz = ftell(file->pszFile);
	//restore previous position
	fseek(file->pszFile, 0L, old);
	return sz;
}


unique_ptr<FileData> FileSystem::Process(unique_ptr<File>& file, void* userdata, void* callback) {
	unique_ptr<FileData> fdata = unique_ptr<FileData>(new FileData);
	if(file == NULL || file->pszFile == NULL)
		return move(fdata);
	int baseBytes = 0;
	int oldBaseBytes = 0;
	
	//Seek to the beginning.
	
	FILE* tmpFile = file->pszFile;
	rewind(file->pszFile);
	int count = 0;
	int size = Size(file);
	
	fdata->data = new char[size * sizeof(char) + 1]();

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
			fdata->data[nRead] = '\0';
		}
	fdata->size = size;
	return move(fdata);
}

void FileSystem::Write(unique_ptr<File>& file, string& buffer) {
	if(file == NULL || file->pszFile == NULL)
		return;
	fputs(buffer.c_str(), file->pszFile);
	fflush(file->pszFile);
}

void FileSystem::WriteLine(unique_ptr<File>& file, string& buffer) {
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