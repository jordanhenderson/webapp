#include "Image.h"
#include "jpeglib.h"
#include "jerror.h"
#include "png.h"
#include <ipp.h>
const char* THUMB_EXTENSIONS_JPEG[] = THUMB_EXTENSIONS_JPEG_D;
using namespace std;
Image::Image(string& filename) {
	if(!FileSystem::Exists(filename)){
		nError = ERROR_FILE_NOT_FOUND;
		return;
	}
	//Check image extension. Use IMAGE_TYPE_JPEG for bmp/gif/jpg/jpeg, IMAGE_TYPE_PNG for png.
	//nError = ERROR_IMAGE_TYPE_NOT_SUPPORTED if image extension not recognised.
	imageType = width = height = nBytes = nChannels = 0;
	pixels = NULL;
	
	for(int i = 0; THUMB_EXTENSIONS_JPEG[i] != NULL; i++) {
		if(endsWith(filename, THUMB_EXTENSIONS_JPEG[i])) {
			imageType = IMAGE_TYPE_JPEG;
		}
	}
	if(endsWith(filename, ".png")) {
		imageType = IMAGE_TYPE_PNG;
	}


	unique_ptr<File> file = FileSystem::Open(filename, "r");
	
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		{
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr jerr;
			unsigned char* output_data[1];
			cinfo.err = jpeg_std_error (&jerr);
			jpeg_create_decompress (&cinfo);
			
			jpeg_stdio_src(&cinfo, file->pszFile);
			jpeg_read_header(&cinfo, TRUE);

			width = cinfo.image_width;
			height = cinfo.image_height;
			colorspace = cinfo.out_color_space;
			jpeg_start_decompress(&cinfo);
			
			nChannels = cinfo.output_components;

			//Allocate pixels array.
			nBytes = cinfo.output_width * cinfo.output_height * cinfo.output_components;
			
			pixels = ippsMalloc_8u(nBytes);
			unsigned int scanline_count = 0;
			unsigned int scanline_length = cinfo.output_width * cinfo.output_components;
			while(cinfo.output_scanline < cinfo.output_height) {
			
				output_data[0] = (pixels + (scanline_count * scanline_length));
				jpeg_read_scanlines(&cinfo, output_data, 1);
				scanline_count++;
			}
			
			jpeg_finish_decompress(&cinfo);
			jpeg_destroy_decompress(&cinfo);
			
			
		}

	}
	FileSystem::Close(file);
	nError = ERROR_SUCCESS;
	
}

void Image::save(std::string& filename) {
	unique_ptr<File> file = FileSystem::Open(filename, "wb");
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		{
			struct jpeg_error_mgr jerr;
			struct jpeg_compress_struct cinfo;
			unsigned char* input_data[1];
			cinfo.err = jpeg_std_error (&jerr);
			jpeg_create_compress(&cinfo);
			jpeg_stdio_dest(&cinfo, file->pszFile);
			cinfo.image_width = width;
			cinfo.image_height = height;
			cinfo.input_components = nChannels;
			cinfo.in_color_space = (J_COLOR_SPACE)colorspace;

			jpeg_set_defaults(&cinfo);
			jpeg_set_quality(&cinfo, 100, TRUE);
			jpeg_start_compress(&cinfo, TRUE);
			unsigned int scanline_count = 0;
			unsigned int scanline_length = width * nChannels;
			while(cinfo.next_scanline < cinfo.image_height) {

				input_data[0] = (pixels + (scanline_count * scanline_length));
				jpeg_write_scanlines(&cinfo, input_data, 1);
				scanline_count++;
			}

			jpeg_finish_compress(&cinfo);
			jpeg_destroy_compress(&cinfo);
			

		}
	}
	FileSystem::Close(file);
	nError = ERROR_SUCCESS;
}

void Image::resize(double width, double height) {
	//Resize the pixel data 
	IppiRect srect = {0,0,this->width, this->height};
	IppiRect drect = {0,0,width,height};
	IppiSize size = {this->width, this->height};
	IppiSize dstsize = {width, height};
	

	IppStatus status = ippStsNoErr;
	int bufsize;
	int specSize;
	Ipp8u* tmpBuf = ippsMalloc_8u(width * height * nChannels);
	ippiResizeGetSize_8u(size, dstsize, ippLanczos, 0, &specSize, &bufsize);
	Ipp8u *initBuf = ippsMalloc_8u(bufsize);

	IppiResizeSpec_32f* pSpec = (IppiResizeSpec_32f*)ippsMalloc_8u(specSize);
	ippiResizeLanczosInit_8u(size, dstsize, 3, pSpec, initBuf);

	ippiResizeGetBufferSize_8u(pSpec,dstsize,nChannels,&bufsize);
	Ipp8u* pBuffer=ippsMalloc_8u(bufsize);

	IppiPoint dstOffset = {0,0};

	double xresizef = width / this->width;
	double yresizef = height / this->height;

	if(pBuffer != NULL) {
		if(nChannels == 1) {
			status = ippiResizeLanczos_8u_C1R((const Ipp8u*)pixels, this->width*nChannels, tmpBuf, width*nChannels, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
		} else if(nChannels == 3) {
			status = ippiResizeLanczos_8u_C3R((const Ipp8u*)pixels, this->width*nChannels, tmpBuf, width*nChannels, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
		} else if(nChannels == 4) {
			status = ippiResizeLanczos_8u_C4R((const Ipp8u*)pixels, this->width*nChannels, tmpBuf, width*nChannels, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
		}
		

	}
	ippsFree(initBuf);
	ippsFree(pSpec);
	ippsFree(pBuffer);
	ippsFree(pixels);
	pixels = tmpBuf;
	nBytes = width * height * nChannels;
	this->width = width;
	this->height = height;
	

}

Image::~Image() {
	if(pixels != NULL)
		ippsFree(pixels);
}