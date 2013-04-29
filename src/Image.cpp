#include "Image.h"
#include "jpeglib.h"
#include "jerror.h"
#include "png.h"
const char* THUMB_EXTENSIONS_JPEG[] = THUMB_EXTENSIONS_JPEG_D;
using namespace std;
Image::Image(string& filename) {
	if(!FileSystem::Exists(filename)){
		nError = ERROR_FILE_NOT_FOUND;
	}
	//Check image extension. Use IMAGE_TYPE_JPEG for bmp/gif/jpg/jpeg, IMAGE_TYPE_PNG for png.
	//nError = ERROR_IMAGE_TYPE_NOT_SUPPORTED if image extension not recognised.
	int imageType;
	for(int i = 0; THUMB_EXTENSIONS_JPEG[i] != NULL; i++) {
		if(endsWith(filename, THUMB_EXTENSIONS_JPEG[i])) {
			imageType = IMAGE_TYPE_JPEG;
		}
	}
	if(endsWith(filename, ".png")) {
		imageType = IMAGE_TYPE_PNG;
	}
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		{
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr jerr;
			cinfo.err = jpeg_std_error (&jerr);
			jpeg_create_decompress (&cinfo);
			unique_ptr<File> file = FileSystem::Open(filename, "r");
			jpeg_stdio_src(&cinfo, file->pszFile);
			jpeg_read_header(&cinfo, TRUE);
			int width = cinfo.image_width;
			int height = cinfo.image_height;
		}
	}
		
}

void Image::resize(int width, int height) {
	//Resize the pixel data 
}