#include "Image.h"
#include "jpeglib.h"
#include "jerror.h"
#include "png.h"
#include "gif_lib.h"

#include <ipp.h>
const char* THUMB_EXTENSIONS_JPEG[] = THUMB_EXTENSIONS_JPEG_D;
using namespace std;
void Image::cleanup() {
	//Clean up various image allocations.
	switch(imageType) {

	case IMAGE_TYPE_PNG:
		if(row_pointers != NULL)
			//row_pointers points to memory allocated as pixels, therefore does
				//not need to be delete[]'d
					delete row_pointers;
		//No break; intentional.
	case IMAGE_TYPE_JPEG:
		if(pixels != NULL) {
			ippsFree(pixels);
			pixels = NULL;
		}
		break;
	case IMAGE_TYPE_GIF:
		if(gif != NULL)
			DGifCloseFile(gif);
		break;
	}
}

Image::Image(string& filename) {
	imageType = -1; 
	row_pointers = NULL;
	pixels = NULL;
	load(filename);
	return;
	
}

void Image::changeType(std::string& filename) {
	for(int i = 0; THUMB_EXTENSIONS_JPEG[i] != NULL; i++) {
		if(endsWith(filename, THUMB_EXTENSIONS_JPEG[i])) {
			imageType = IMAGE_TYPE_JPEG;
		}
	}

	if(endsWith(filename, ".png")) {
		if(imageType >= 0 && imageType != IMAGE_TYPE_PNG) {
			regenRowPointers();
		}
		imageType = IMAGE_TYPE_PNG;
	}

	if(endsWith(filename, ".gif")) {
		imageType = IMAGE_TYPE_GIF;
	}
}

void Image::load(std::string& filename) {
	if(!FileSystem::Exists(filename)){
		nError = ERROR_FILE_NOT_FOUND;
		return;
	}
	//Check image extension. Use IMAGE_TYPE_JPEG for bmp/gif/jpg/jpeg, IMAGE_TYPE_PNG for png.
	//nError = ERROR_IMAGE_TYPE_NOT_SUPPORTED if image extension not recognised.
	width = height = nBytes = nChannels = colorspace = bitdepth = 0;

	cleanup();

	changeType(filename);

	unique_ptr<File> file = FileSystem::Open(filename, "rb");

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
			bitdepth = 8;
			setColorSpace(cinfo.out_color_space);
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
		break;
	case IMAGE_TYPE_PNG: 
		{
			png_byte header[8];
			fread(header, 1, 8, file->pszFile);
			int is_png = !png_sig_cmp(header, 0, 8);
			if(!is_png) {
				nError = ERROR_INVALID_IMAGE;
				goto finish;
			}
			png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!png_ptr)
				goto finish;

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr)
			{
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			//Set our error handler.
			if(setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			png_init_io(png_ptr, file->pszFile);
			png_set_sig_bytes(png_ptr, 8);

			png_read_info(png_ptr, info_ptr);
		
			png_set_interlace_handling(png_ptr);
			png_set_strip_16(png_ptr);
			png_set_expand(png_ptr);
			png_set_gray_to_rgb(png_ptr);
			png_read_update_info(png_ptr, info_ptr);

			width = png_get_image_width(png_ptr, info_ptr);
			height = png_get_image_height(png_ptr, info_ptr);
			setColorSpace(png_get_color_type(png_ptr, info_ptr));

			bitdepth = png_get_bit_depth(png_ptr, info_ptr);
			nChannels = png_get_channels(png_ptr, info_ptr);

			//Get bytes per row.

			int scanline_length = png_get_rowbytes(png_ptr, info_ptr);
			nBytes = height * scanline_length;

			//Allocate the pixel dump
			pixels = ippsMalloc_8u(nBytes);

			if(row_pointers != NULL) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			regenRowPointers();

			png_read_image(png_ptr, row_pointers);
			png_read_end(png_ptr, info_ptr);
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		}
		break;
	case IMAGE_TYPE_GIF: 
		{

			//DGifOpenFileHandle(file->pszFile);
			int error;
			gif = DGifOpenFileHandle(fileno(file->pszFile), &error);

			file->pszFile = NULL;
			if(!gif) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}
			//Read the image
			if(DGifSlurp(gif) != GIF_OK) {
				DGifCloseFile(gif);
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			width = gif->SWidth;
			height = gif->SHeight;
			bitdepth = 8;
			
			nChannels = 4;
			colorspace = IMAGE_COLORSPACE_RGBA;

			//Allocate our frame array.
			frames = new unsigned char*[gif->ImageCount]();
			maps = new unsigned char*[gif->ImageCount]();
			for(int i = 0; i < gif->ImageCount; i++) {
				gifInsertFrame(i);
			}
			pixels = frames[0];
			
			//File now handled by giflib.
			file->pszFile = NULL;
			
		}
		break;
	}

	nError = ERROR_SUCCESS;
finish:
	if(file->pszFile != NULL)
		FileSystem::Close(file);

}

void Image::setColorSpace(int colorSpace) {
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		if(colorSpace == JCS_EXT_RGB) colorspace = IMAGE_COLORSPACE_RGB;
		else if(colorSpace == JCS_EXT_RGBA) colorspace = IMAGE_COLORSPACE_RGBA;
		else goto error;
		break;
	case IMAGE_TYPE_PNG:
		if(colorSpace == PNG_COLOR_TYPE_RGB) colorspace = IMAGE_COLORSPACE_RGB;
		else if(colorSpace == PNG_COLOR_TYPE_RGBA) colorspace = IMAGE_COLORSPACE_RGBA;
		else goto error;
		break;
	default:
		goto error;
	} 
	return;

error:
	nError = ERROR_IMAGE_PROCESSING_FAILED;
}

int Image::getColorSpace() {
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		if(colorspace == IMAGE_COLORSPACE_RGB) return JCS_EXT_RGB;
		else if(colorspace == IMAGE_COLORSPACE_RGBA) return JCS_EXT_RGBA;
		else goto error;
		break;
	case IMAGE_TYPE_PNG:
		if(colorspace == IMAGE_COLORSPACE_RGB) return PNG_COLOR_TYPE_RGB;
		else if(colorspace == IMAGE_COLORSPACE_RGBA) return PNG_COLOR_TYPE_RGBA;
		else goto error;
		break;
	default:
		goto error;
	}

	//Gif does not need a colorspace. We assume RGBA at this point.
error:
	nError = ERROR_IMAGE_PROCESSING_FAILED;
	return 0;
}

void Image::save(std::string& filename) {
	unique_ptr<File> file = FileSystem::Open(filename, "wb");
	//Temporarily change the type, to allow output handling to correctly work with different image types.
	int oldType = imageType;
	changeType(filename);
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
			cinfo.in_color_space = (J_COLOR_SPACE)getColorSpace();

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
		break;
	case IMAGE_TYPE_PNG:
		{

			png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!png_ptr) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr) {
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			//Set our error handler.
			if(setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_write_struct(&png_ptr, &info_ptr);
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			

			png_init_io(png_ptr, file->pszFile);
			png_set_IHDR(png_ptr, info_ptr, width, height,
				bitdepth, getColorSpace(), PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

			png_write_info(png_ptr, info_ptr);

			//Write bytes
			png_write_image(png_ptr,row_pointers);

			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);

		}
		break;
	case IMAGE_TYPE_GIF: {
		int error;
		GifFileType* output = EGifOpenFileHandle(fileno(file->pszFile), &error);
		if(!output) {
			nError = ERROR_IMAGE_PROCESSING_FAILED;
			goto finish;
		}

		output->SWidth = width;
		output->SHeight = height;
		output->SColorResolution = bitdepth;
		output->SBackGroundColor = gif->SBackGroundColor;
		output->SColorMap = GifMakeMapObject(
			gif->SColorMap->ColorCount,
			gif->SColorMap->Colors);

		for (int i = 0; i < gif->ImageCount; i++)
			(void) GifMakeSavedImage(output, &gif->SavedImages[i]);
		
		if(EGifSpew(output) != GIF_OK) {
			nError = ERROR_IMAGE_PROCESSING_FAILED;
			goto finish;
		}
		file->pszFile = NULL;

		}
		break;
	}

	nError = ERROR_SUCCESS;
finish:
	imageType = oldType;
	if(file->pszFile != NULL)
		FileSystem::Close(file);
	
}

void Image::resize(int width, int height) {
	if(imageType != IMAGE_TYPE_GIF)
		pixels = _resize(pixels, width, height, this->width, this->height);
	else {
		//Resize each frame.
		for (int i = 0; i < gif->ImageCount; i++) {

			unsigned char* newFrame = _resize(frames[i], width, height, gif->SavedImages[i].ImageDesc.Width,
				gif->SavedImages[i].ImageDesc.Height);
			
			if(newFrame == frames[i])
				continue;
			else {
				frames[i] = newFrame; 
			}

			free(gif->SavedImages[i].RasterBits);
			free(gif->SavedImages[i].ImageDesc.ColorMap);
			gif->SavedImages[i].ImageDesc.Width = width;
			gif->SavedImages[i].ImageDesc.Height = height;

			gifMakeMap(frames[i], width, height, (unsigned char**)&gif->SavedImages[i].ImageDesc.ColorMap, (unsigned char**)&gif->SavedImages[i].RasterBits);
			//Generate a new colour map for each frame.

			
		}
		this->width = width;
		this->height = height;
		//update pixels for animated gif -> static conversion.
		pixels = frames[0];
	}

	if(imageType == IMAGE_TYPE_PNG) {
		//We need to regenerate the row_pointers.
		regenRowPointers();
	
	}
}

void Image::regenRowPointers() {
	if(row_pointers != NULL)
		delete row_pointers;
	row_pointers = new png_bytep[height * sizeof(png_bytep)]();
	for(int i = 0; i < height; i++) {
		row_pointers[i] = pixels + (i*width*nChannels);
	}
}

unsigned char* Image::_resize(unsigned char* image, int width, int height, int oldWidth, int oldHeight) {

	if(width == oldWidth && height == oldHeight)
		return image;

	//Store constants in required structs.
	IppiRect srect = {0,0,oldWidth, oldHeight};
	IppiRect drect = {0,0,width,height};
	IppiSize size = {oldWidth, oldHeight};
	IppiSize dstsize = {width, height};
	IppiPoint dstOffset = {0,0};
	IppStatus status = ippStsNoErr;

	int bufsize;
	int specSize;

	ippiResizeGetSize_8u(size, dstsize, ippLanczos, 0, &specSize, &bufsize);
	Ipp8u *initBuf = ippsMalloc_8u(bufsize);

	//Create resize spec structure
	IppiResizeSpec_32f* pSpec = (IppiResizeSpec_32f*)ippsMalloc_8u(specSize);

	//Set lanczos scaling mode
	ippiResizeLanczosInit_8u(size, dstsize, 3, pSpec, initBuf);

	//Get the size required for the pBuffer, allocate it
	ippiResizeGetBufferSize_8u(pSpec,dstsize,nChannels,&bufsize);
	Ipp8u* pBuffer=ippsMalloc_8u(bufsize);

	//Allocate the temporary buffer used to store resized image.
	Ipp8u* tmpBuf = ippsMalloc_8u(width * height * nChannels);



	if(pBuffer != NULL) {
		//TODO optimize the following.
		if(nChannels == 3) {
			status = ippiResizeLanczos_8u_C3R((const Ipp8u*)image, oldWidth*nChannels, tmpBuf, width*nChannels, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
		} else if(nChannels == 4) {
			status = ippiResizeLanczos_8u_C4R((const Ipp8u*)image, oldWidth*nChannels, tmpBuf, width*nChannels, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
		} else {
			nError = ERROR_IMAGE_PROCESSING_FAILED;
		}
	}

	ippsFree(initBuf);
	ippsFree(pSpec);
	ippsFree(pBuffer);
	ippsFree(image);


	
	nBytes = width * height * nChannels;
	return tmpBuf;
}

Image::~Image() {
	cleanup();
}