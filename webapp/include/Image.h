/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#ifndef IMAGE_H
#define IMAGE_H

#include "Platform.h"
#include "CPlatform.h"

#define IMAGE_TYPE_JPEG 0
#define IMAGE_TYPE_PNG 1
#define IMAGE_TYPE_GIF 2
#define THUMB_EXTENSIONS_JPEG_D {".jpg", ".jpeg", NULL};
#define ERROR_INVALID_IMAGE 1
#define ERROR_IMAGE_NOT_SUPPORTED 2
#define ERROR_FILE_NOT_FOUND 3
#define ERROR_IMAGE_PROCESSING_FAILED 4

struct GifFileType;

class Image {
private:
	int imageType = 0;
	int width = 0;
	int height = 0;
	int nBytes = 0;
	unsigned char* pixels = NULL;
	unsigned char* _resize(unsigned char* image, int width, int height, 
		int oldWidth, int oldHeight);
	void changeType(const std::string& filename);
	//PNG ONLY
	int bitdepth = 0;
	unsigned char** row_pointers = NULL; 
	//GIF ONLY
	int imagecount = 0;
	GifFileType* gif = NULL;
	unsigned char** frames = NULL;
	void gifInsertFrame(unsigned int frame);
	void gifRasterizeFrame(unsigned int frame, unsigned char** map,
		unsigned char** raster);
	void cleanup();
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
	int load(const std::string& filename);
	void resize(int width, int height);
	int save(const std::string& filename);
};

#endif //IMAGE_H
