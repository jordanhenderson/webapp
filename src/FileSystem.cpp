#include "FileSystem.h"
#include "tinydir.h"
using namespace std;
void FileSystem::Open(const string& fileName, const string& flags, File* outFile) {
	//Ensure outFile != NULL
	if(outFile == NULL) return;

	//ensure file is opened in binary mode
	int flen = flags.length();
	string actualFlag = flags;
	if(flags[flen-1] != 'b') {
		actualFlag.append("b");
	}

#ifdef WIN32
	wstring wfileName, wflags;
	wfileName.assign(fileName.begin(), fileName.end());
	wflags.assign(actualFlag.begin(), actualFlag.end());
	outFile->pszFile = _wfopen(wfileName.c_str(), wflags.c_str());
#else
	outFile.pszFile = fopen(fileName, flags);
#endif
	outFile->fileName = fileName;
	outFile->flags = actualFlag;
	char* errType = "";
	if(outFile->pszFile == NULL) {
		if(flags[0] == 'w' || flags[0] == 'a' || flags[1] == '+')
			errType = "writing";
		else if(flags[0] == 'r')
			errType = "reading";
		//Debug Message: File Access error: $fileName could not be opened for $errType.
		
	}

	//Debug Message: File $fileName opened successfully. Flags: $flags.
	return;
}

FileData::~FileData() {
	delete[] data;
}

File::~File() {
	if(pszFile != NULL)
		fclose(pszFile);
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
	int baseBytes = 0;
	int oldBaseBytes = 0;
	
	//Seek to the beginning.
	
	FILE* tmpFile = file->pszFile;
	rewind(file->pszFile);
	int count = 0;
	int size = Size(file);
	
	outData->data = new char[size * sizeof(char) + 1];

		if(callback != NULL) {
			while(!feof(tmpFile)) {
				//Read files line by line
		
					oldBaseBytes = ftell(tmpFile);
					fgets(outData->data + baseBytes, 4096, tmpFile);
					int nBytesRead = ftell(tmpFile) - oldBaseBytes;
					baseBytes += nBytesRead;
					FILE_LINE_CALLBACK callbackFn = (FILE_LINE_CALLBACK)(callback);
					callbackFn(userdata, outData->data + (oldBaseBytes-count), nBytesRead);
				} 
				baseBytes--;
				count = 1;
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
	string tmp = string(buffer);
	Write(file, tmp.append(ENV_NEWLINE));
}
//Returns true if the specified path exists and can be read.
int FileSystem::Exists(const string& path) {
	struct stat buf;
	
	if(stat(path.c_str(), &buf) == 0)
		return 1;
	return 0;
}

void FileSystem::MakePath(const string& path) {
	//Recurisvely make a path structure.
	string tmpPath = string(path);
	int nFilename = tinydir_todir((char*)tmpPath.c_str(), tmpPath.length());
	const char* lastSep = tmpPath.c_str();

	for(int i = 0; i <= tmpPath.length() - nFilename; i++) {
		if(tmpPath[i] == '/' || tmpPath[i] == 0) {
			tmpPath[i] = 0;
			tinydir_dir dir;
			if(tinydir_open(&dir, tmpPath.c_str()) == -1) {
				//make the directory
				tinydir_create(tmpPath.c_str());
			}
			tinydir_close(&dir);
			tmpPath[i] = '/';
		}
	}
}

void FileSystem::DeletePath(const string& path) {
	if(!FileSystem::Exists(path))
		return;

	tinydir_dir dir;
	tinydir_open(&dir, path.c_str());

	//Delete all files/directories.
	while(dir.has_next) {
		tinydir_file f;
		tinydir_readfile(&dir, &f);
		if(!f.is_dir) {
#ifdef WIN32
			_wunlink(strtowide(f.path));
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
#ifdef WIN32
	wstring wPath;
	wPath.assign(path.begin(), path.end());
	int err = _wrmdir(wPath.c_str());
#else
	rmdir(path.c_str());
#endif
	
}

vector<string> FileSystem::GetFiles(const string& base, const string& path, int recurse) {
	vector<string> files;

	tinydir_dir dir;
	string abase = base + PATHSEP + path;
	tinydir_open(&dir, abase.c_str());
	while(dir.has_next) {
		
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		string dpath = path.empty() ? file.name : path + PATHSEP + file.name;
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
	string abase = base + PATHSEP + path;
	tinydir_open(&dir, abase.c_str());
	while(dir.has_next) {
		
		tinydir_file file;
		tinydir_readfile(&dir, &file);
		string dpath = path.empty() ? file.name : path + PATHSEP + file.name;
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