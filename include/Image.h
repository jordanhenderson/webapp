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
public:
	Image(std::string& filename);
	~Image() {};
	int width();
	int height();
	void resize(int width, int height);

};

#endif