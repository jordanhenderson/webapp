/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include <opencv2/imgproc.hpp>
#include "Image.h"
#include "FileSystem.h"

//JPEG includes
#include "jpeglib.h"
#include "jerror.h"

//PNG includes
#include "png.h"

//Gif includes
#include "gif_lib.h"

const char* THUMB_EXTENSIONS_JPEG[] = THUMB_EXTENSIONS_JPEG_D;
using namespace std;

#ifdef _MSC_VER
#define fileno _fileno
#endif

//JPEG jmp (hacky, but less code than official setjmp solution.)
extern "C" {
	jmp_buf buf;
}
void Image::cleanup() {
	//Clean up various image allocations.
	switch(imageType) {

	case IMAGE_TYPE_PNG:
		if(row_pointers != NULL)
			delete[] row_pointers;
			row_pointers = NULL;
		//No break; intentional.
	case IMAGE_TYPE_JPEG:
		if(pixels != NULL) {
			delete[] pixels;
			pixels = NULL;
		}
		break;
	case IMAGE_TYPE_GIF:
			//Dealloc the frames
		if(frames != NULL) {
			for(int i = 0; i < imagecount; i++) {
				free(frames[i]);
			}
			delete[] frames;
		}
		if(gif != NULL)
			DGifCloseFile(gif);


		break;
	}
}

Image::Image(const webapp_str_t& filename) {
	imageType = -1; 
	row_pointers = NULL;
	pixels = NULL;
	imagecount = 0;
	load(filename);
	return;
	
}

void Image::changeType(const webapp_str_t& filename) {
	string f = string(filename.data, filename.len);
	std::transform(f.begin(), f.end(),f.begin(), ::tolower);
	for(int i = 0; THUMB_EXTENSIONS_JPEG[i] != NULL; i++) {
		if(endsWith(f, THUMB_EXTENSIONS_JPEG[i])) {
			imageType = IMAGE_TYPE_JPEG;
		}
	}

	if(endsWith(f, ".png")) {
		if(imageType >= 0 && imageType != IMAGE_TYPE_PNG) {
			regenRowPointers();
		}
		imageType = IMAGE_TYPE_PNG;
	}

	if(endsWith(f, ".gif")) {
		//PNG/JPG->GIF
		if(imageType >= 0 && imageType != IMAGE_TYPE_GIF) {
			if(frames != NULL)
				delete[] frames;
			if(gif)
				DGifCloseFile(gif);
			frames = new unsigned char*[1];
			frames[0] = pixels;
			gif = EGifOpen(NULL, 0, NULL);
			imagecount = 1;

		}
		imageType = IMAGE_TYPE_GIF;
	}
	
}

int Image::load(const webapp_str_t& filename) {
	int err = ERROR_SUCCESS;
	//Check image extension. Use IMAGE_TYPE_JPEG for bmp/gif/jpg/jpeg, IMAGE_TYPE_PNG for png.
	width = height = nBytes = bitdepth = 0;
	gif = NULL;

	cleanup();

	changeType(filename);

	if(imageType == -1) {
		return ERROR_IMAGE_NOT_SUPPORTED;
	}

	File file;
	if(!file.Open(filename, "rb")) {
		return ERROR_FILE_NOT_FOUND;
	}
	FILE* file_ptr = file.GetPointer();
	
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		{
			int v = setjmp(buf);
			if(v) {
				err = ERROR_INVALID_IMAGE;
				goto finish;
			}
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr jerr;
			unsigned char* output_data[1];
			cinfo.err = jpeg_std_error (&jerr);
			jpeg_create_decompress (&cinfo);

			jpeg_stdio_src(&cinfo, file_ptr);
			jpeg_read_header(&cinfo, TRUE);

			width = cinfo.image_width;
			height = cinfo.image_height;
			bitdepth = 8;
			cinfo.out_color_space = JCS_EXT_RGBA;
			jpeg_start_decompress(&cinfo);

			//Allocate pixels array.
			nBytes = width * height * 4;

			pixels = new unsigned char[nBytes];
			unsigned int scanline_count = 0;
			unsigned int scanline_length = cinfo.output_width * 4;
			while(cinfo.output_scanline < cinfo.output_height) {
				output_data[0] = (pixels + (scanline_count * scanline_length));
				jpeg_read_scanlines(&cinfo, output_data, 1);
				scanline_count++;
			}

			imagecount = 1;
			jpeg_finish_decompress(&cinfo);
			jpeg_destroy_decompress(&cinfo);
		}
		break;
	case IMAGE_TYPE_PNG: 
		{
			png_byte header[8];
			fread(header, 1, 8, file_ptr);
			int is_png = !png_sig_cmp(header, 0, 8);
			if(!is_png) {
				err = ERROR_INVALID_IMAGE;
				goto finish;
			}
			png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!png_ptr) goto finish;

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr) {
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			//Set our error handler.
			if(setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			png_init_io(png_ptr, file_ptr);
			png_set_sig_bytes(png_ptr, 8);

			png_read_info(png_ptr, info_ptr);
		
			png_set_interlace_handling(png_ptr);
			png_set_strip_16(png_ptr);
			
			int colorType = png_get_color_type(png_ptr, info_ptr);
			bitdepth = png_get_bit_depth(png_ptr, info_ptr);
			int trns = png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS);
			if(colorType == PNG_COLOR_TYPE_RGB)
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
			if (colorType == PNG_COLOR_TYPE_PALETTE) {
				png_set_expand(png_ptr);
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
			}

			/* expand grayscale images to the full 8 bits */
			if (colorType == PNG_COLOR_TYPE_GRAY &&
				bitdepth < 8)
				png_set_expand(png_ptr);

			/* expand images with transparency to full alpha channels */
			if (trns)
				png_set_expand(png_ptr);

			if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
				png_set_gray_to_rgb(png_ptr);
				png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
			}
	
			png_read_update_info(png_ptr, info_ptr);

			width = png_get_image_width(png_ptr, info_ptr);
			height = png_get_image_height(png_ptr, info_ptr);

			//Get bytes per row.
			int scanline_length = png_get_rowbytes(png_ptr, info_ptr);
			nBytes = height * scanline_length;

			//Allocate the pixel dump
			pixels = new unsigned char[nBytes];

			if(row_pointers != NULL) {
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			imagecount = 1;
			regenRowPointers();

			png_read_image(png_ptr, row_pointers);
			png_read_end(png_ptr, info_ptr);
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		}
		break;
	case IMAGE_TYPE_GIF: 
		{
			int error;
			gif = DGifOpenFileHandle(fileno(file_ptr), &error);

			if(!gif) {
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}
			//Read the image
			if(DGifSlurp(gif) != GIF_OK) {
				DGifCloseFile(gif);
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			width = gif->SWidth;
			height = gif->SHeight;
			bitdepth = 8;
			imagecount = gif->ImageCount;
			//Allocate our frame array.
			frames = new unsigned char*[imagecount];
			for(int i = 0; i < imagecount; i++) {
				gifInsertFrame(i);
			}
			pixels = frames[0];
		}
		break;
	}

	err = ERROR_SUCCESS;
finish:
	file.Close();
	return err;

}

int Image::save(const webapp_str_t& filename) {
	int err = ERROR_SUCCESS;
	File file(filename, "wb");
	FILE* file_ptr = file.GetPointer();
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
			jpeg_stdio_dest(&cinfo, file_ptr);
			cinfo.image_width = width;
			cinfo.image_height = height;
			cinfo.input_components = 4;
			cinfo.in_color_space = JCS_EXT_RGBA;

			jpeg_set_defaults(&cinfo);
			jpeg_set_quality(&cinfo, 100, TRUE);
			jpeg_start_compress(&cinfo, TRUE);
			unsigned int scanline_count = 0;
			unsigned int scanline_length = width * 4;
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
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr) {
				png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			//Set our error handler.
			if(setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_write_struct(&png_ptr, &info_ptr);
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			png_init_io(png_ptr, file_ptr);
			png_set_IHDR(png_ptr, info_ptr, width, height,
				bitdepth, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

			png_write_info(png_ptr, info_ptr);

			//Write bytes
			png_write_image(png_ptr,row_pointers);
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);
		}
		break;
	case IMAGE_TYPE_GIF: 
		{
			int error;
			GifFileType* output = EGifOpenFileHandle(fileno(file_ptr), &error);
			if(!output) {
				err = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			output->SWidth = width;
			output->SHeight = height;
			output->SColorResolution = bitdepth;
			output->SBackGroundColor = gif->SBackGroundColor;
			output->SColorMap = 
				GifMakeMapObject(gif->SColorMap->ColorCount, gif->SColorMap->Colors);
			output->ImageCount = imagecount;
			SavedImage* saved_images;
			saved_images = output->SavedImages = 
				(SavedImage*)malloc(sizeof(SavedImage) * imagecount);

			for (int i = 0; i < imagecount; i++) {
				memset(&saved_images[i], '\0', sizeof(SavedImage));
				//Remove optimisation on savedimages.
				saved_images[i].ImageDesc.Width = width;
				saved_images[i].ImageDesc.Height = height;
				saved_images[i].ExtensionBlockCount = 
					gif->SavedImages[i].ExtensionBlockCount;
				saved_images[i].ExtensionBlocks = 
					gif->SavedImages[i].ExtensionBlocks;
				
				//Rasterize the frame.
				gifRasterizeFrame(i, (unsigned char**)&saved_images[i].ImageDesc.ColorMap, 
					(unsigned char**)&saved_images[i].RasterBits);
			}

			if(EGifSpew(output) != GIF_OK) {
				err = ERROR_IMAGE_PROCESSING_FAILED;
			} else {
				file.Detach();
			}

			//Clean up generated maps.
			for(int i = 0; i < imagecount; i++) {
				if (saved_images[i].ImageDesc.ColorMap != NULL) {
					GifFreeMapObject(saved_images[i].ImageDesc.ColorMap);
					saved_images[i].ImageDesc.ColorMap = NULL;
				}

				if (saved_images[i].RasterBits != NULL)
					free((char *)saved_images[i].RasterBits);
			}

			free(saved_images);
		}
		break;
	}

	err = ERROR_SUCCESS;
finish:
	imageType = oldType;
	file.Close();
	return err;
}

void Image::resize(int width, int height) {
	if(imageType != IMAGE_TYPE_GIF) {
		pixels = _resize(pixels, width, height, this->width, this->height);
		this->width = width;
		this->height = height;
	}
	else {
		//Resize each frame.
		for (int i = 0; i < imagecount; i++) {

			unsigned char* newFrame = _resize(frames[i], width, height, this->width,
				this->height);

			if(newFrame == frames[i])
				continue;
			else
				frames[i] = newFrame; 
			
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
		delete[] row_pointers;
	row_pointers = new png_bytep[height * sizeof(png_bytep)];
	for(int i = 0; i < height; i++) {
		row_pointers[i] = pixels + (i*width*4);
	}
}

unsigned char* Image::_resize(unsigned char* image, int width, int height,
							  int oldWidth, int oldHeight) {
	if(width == oldWidth && height == oldHeight)
		return image;

	//Allocate new buffer - opencv needs an extra 4 bytes in some (odd) occasions!
	unsigned char* tmpBuf = new unsigned char[(width * height * 4) + 4];
	cv::Mat input(oldHeight, oldWidth, CV_8UC4, image);
	cv::Mat output(height, width, CV_8UC4, tmpBuf);
	
	cv::resize(input, output, output.size(), 0, 0, cv::INTER_AREA);
	
	delete[] image;
	nBytes = width * height * 4;
	return tmpBuf;
}

Image::~Image() {
	cleanup();
}
