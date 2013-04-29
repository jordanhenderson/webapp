#ifndef IMAGE_H
#define IMAGE_H
#include "Platform.h"
#include "FileSystem.h"
#define IMAGE_TYPE_JPEG 0
#define IMAGE_TYPE_PNG 1
#define THUMB_EXTENSIONS_JPEG_D {".gif", ".jpg", ".jpeg", NULL};
class Image : public Internal {
private:
	int imageType;
	double width;
	double height;
	int nBytes;
	int nChannels;
	int colorspace;
	unsigned char* pixels;
public:
	Image(std::string& filename);
	~Image();
	inline double getWidth() {
		return width;
	}
	inline double getHeight() {
		return height;	
	};
	void resize(double width, double height);
	void save(std::string& filename);
};

#endif