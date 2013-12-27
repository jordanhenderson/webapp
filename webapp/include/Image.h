#ifndef IMAGE_H
#define IMAGE_H
#include "Platform.h"
#include "CPlatform.h"
#include "FileSystem.h"
#define IMAGE_TYPE_JPEG 0
#define IMAGE_TYPE_PNG 1
#define IMAGE_TYPE_GIF 2
#define THUMB_EXTENSIONS_JPEG_D {".jpg", ".jpeg", NULL};

struct GifFileType;

class Image {
private:
	unsigned int nError = 0;
	int imageType = 0;
	int width = 0;
	int height = 0;
	int nBytes = 0;
	unsigned char* pixels = NULL;
	void changeType(const std::string& filename);
	//PNG ONLY
	int bitdepth = 0;
	unsigned char** row_pointers = NULL; 
	//GIF ONLY
	int imagecount = 0;
	GifFileType* gif = NULL;
	unsigned char** frames = NULL;
	int gifGetTransparentColor(int frame);
	void gifInsertFrame(int frame);
	void gifMakeMap(unsigned char* image, int width, int height, unsigned char** map, unsigned char** raster);
	void cleanup();
	unsigned char* _resize(unsigned char* image, int width, int height, int oldWidth, int oldHeight);
	void regenRowPointers();
public:
	Image(const std::string& filename);
	~Image();
	inline int getWidth() {
		return width;
	}
	inline int getHeight() {
		return height;	
	};
	inline unsigned int GetLastError() { return nError; };
	void load(const std::string& filename);
	void resize(int width, int height);
	void save(const std::string& filename);
};

#endif