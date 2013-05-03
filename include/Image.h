#ifndef IMAGE_H
#define IMAGE_H
#include "Platform.h"
#include "FileSystem.h"
#define IMAGE_TYPE_JPEG 0
#define IMAGE_TYPE_PNG 1
#define IMAGE_TYPE_GIF 2
#define THUMB_EXTENSIONS_JPEG_D {".gif", ".jpg", ".jpeg", NULL};


class Image : public Internal {
private:
	int imageType;
	int width;
	int height;
	int nBytes;
	int nChannels;
	int colorspace;
	unsigned char* pixels;
	//PNG ONLY
	int bitdepth;
	unsigned char** row_pointers; 
	
public:
	Image(std::string& filename);
	~Image();
	inline int getWidth() {
		return width;
	}
	inline int getHeight() {
		return height;	
	};
	void load(std::string& filename);
	void resize(int width, int height);
	void save(std::string& filename);
};

#endif