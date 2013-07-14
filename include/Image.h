#ifndef IMAGE_H
#define IMAGE_H
#include "Platform.h"
#include "FileSystem.h"
#define IMAGE_TYPE_JPEG 0
#define IMAGE_TYPE_PNG 1
#define IMAGE_TYPE_GIF 2
#define THUMB_EXTENSIONS_JPEG_D {".jpg", ".jpeg", NULL};

struct GifFileType;

class Image : public Internal {
private:
	void cleanup();
	unsigned char* _resize(unsigned char* image, int width, int height, int oldWidth, int oldHeight);
	int imageType;
	int width;
	int height;
	int nBytes;
	unsigned char* pixels;
	void changeType(const std::string& filename);
	//PNG ONLY
	int bitdepth;
	unsigned char** row_pointers; 
	void regenRowPointers();
	//GIF ONLY

	GifFileType* gif;
	unsigned char** frames;
	unsigned char** maps;
	int gifGetTransparentColor(int frame);
	void gifInsertFrame(int frame);
	void gifMakeMap(unsigned char* image, int width, int height, unsigned char** map, unsigned char** raster);
public:
	Image(const std::string& filename);
	~Image();
	inline int getWidth() {
		return width;
	}
	inline int getHeight() {
		return height;	
	};
	void load(const std::string& filename);
	void resize(int width, int height);
	void save(const std::string& filename);
};

#endif