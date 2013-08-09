#include "Image.h"
#include "jpeglib.h"
#include "jerror.h"
#include "png.h"
#include "gif_lib.h"
#ifdef HAS_IPP
#include <ipp.h>
#endif

#include <algorithm>
const char* THUMB_EXTENSIONS_JPEG[] = THUMB_EXTENSIONS_JPEG_D;
using namespace std;

//JPEG jmp (hacky, but less code than official setjmp solution.)
extern "C" {
	jmp_buf buf;
}
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
#ifdef HAS_IPP
			ippsFree(pixels);
#else
			delete[] pixels;
#endif
			pixels = NULL;
		}
		break;
	case IMAGE_TYPE_GIF:
			//Dealloc the frames
		if(frames != NULL) {
			for(int i = 0; i < imagecount; i++) {
#ifdef HAS_IPP
				ippsFree(frames[i]);
#else
				free(frames[i]);
#endif
			}
			delete[] frames;
		}
		if(gif != NULL)
			DGifCloseFile(gif);


		break;
	}
}

Image::Image(const string& filename) {
	imageType = -1; 
	row_pointers = NULL;
	pixels = NULL;
	imagecount = 0;
	load(filename);
	return;
	
}

void Image::changeType(const string& filename) {
	string f = filename;
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

void Image::load(const string& filename) {

	//Check image extension. Use IMAGE_TYPE_JPEG for bmp/gif/jpg/jpeg, IMAGE_TYPE_PNG for png.
	//nError = ERROR_IMAGE_TYPE_NOT_SUPPORTED if image extension not recognised.
	width = height = nBytes = bitdepth = 0;
	gif = NULL;

	cleanup();

	changeType(filename);

	if(imageType == -1) {
		nError = ERROR_IMAGE_NOT_SUPPORTED;
		return;
	}

	File file;
	if(!FileSystem::Open(filename, "rb", &file)){
		nError = ERROR_FILE_NOT_FOUND;
		return;
	}
	
	switch(imageType) {
	case IMAGE_TYPE_JPEG: 
		{
			int v = setjmp(buf);
			if(v) {
				//ERROR
				nError = ERROR_INVALID_IMAGE;
				goto finish;
			}
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr jerr;
			unsigned char* output_data[1];
			cinfo.err = jpeg_std_error (&jerr);
			jpeg_create_decompress (&cinfo);

			jpeg_stdio_src(&cinfo, file.pszFile);
			jpeg_read_header(&cinfo, TRUE);

			width = cinfo.image_width;
			height = cinfo.image_height;
			bitdepth = 8;
			cinfo.out_color_space = JCS_EXT_RGBA;
			jpeg_start_decompress(&cinfo);

			//Allocate pixels array.
			nBytes = width * height * 4;

#ifdef HAS_IPP
			pixels = ippsMalloc_8u(nBytes);
#else	
			pixels = new unsigned char[nBytes];
#endif
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
			fread(header, 1, 8, file.pszFile);
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

			png_init_io(png_ptr, file.pszFile);
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

		   if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_RGB_ALPHA || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
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
#ifdef HAS_IPP
			pixels = ippsMalloc_8u(nBytes);
#else
			pixels = new unsigned char[nBytes];
#endif

			if(row_pointers != NULL) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
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
			gif = DGifOpenFileHandle(fileno(file.pszFile), &error);

			file.pszFile = NULL;
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
			imagecount = gif->ImageCount;
			//Allocate our frame array.
			frames = new unsigned char*[imagecount];
			for(int i = 0; i < imagecount; i++) {
				gifInsertFrame(i);
			}
			pixels = frames[0];

			//File now handled by giflib.
			file.pszFile = NULL;
			
		}
		break;

	}

	nError = ERROR_SUCCESS;
finish:
	if(file.pszFile != NULL)
		FileSystem::Close(&file);

}

void Image::save(const string& filename) {
	File file;
	FileSystem::Open(filename, "wb", &file);
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
			jpeg_stdio_dest(&cinfo, file.pszFile);
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

			png_init_io(png_ptr, file.pszFile);
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
			GifFileType* output = EGifOpenFileHandle(fileno(file.pszFile), &error);
			if(!output) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
				goto finish;
			}

			output->SWidth = width;
			output->SHeight = height;
			
			output->Image.Width = width;
			output->Image.Height = height;

			output->SColorResolution = bitdepth;
			output->SBackGroundColor = gif->SBackGroundColor;
			//TODO: remove this.
			output->SColorMap = GifMakeMapObject(gif->SColorMap->ColorCount, gif->SColorMap->Colors);
			output->ImageCount = imagecount;
			SavedImage* saved_images;
			saved_images = output->SavedImages = (SavedImage *)malloc(sizeof(SavedImage)*imagecount);

			//TODO Disposal optimisation for animations.
			for (int i = 0; i < imagecount; i++) {
				memset(&saved_images[i], '\0', sizeof(SavedImage));
				//Remove optimisation on savedimages.
				saved_images[i].ImageDesc.Width = width;
				saved_images[i].ImageDesc.Height = height;
				saved_images[i].ExtensionBlockCount = gif->SavedImages[i].ExtensionBlockCount;
				saved_images[i].ExtensionBlocks = gif->SavedImages[i].ExtensionBlocks;
			
				//Remove subimage rasterbits issues
				gifMakeMap(frames[i], width, height, (unsigned char**)&saved_images[i].ImageDesc.ColorMap, (unsigned char**)&saved_images[i].RasterBits);
				//Clean up rasterbits/maps.					
			}

			


			if(EGifSpew(output) != GIF_OK) {
				nError = ERROR_IMAGE_PROCESSING_FAILED;
			} else {
				file.pszFile = NULL;
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

	nError = ERROR_SUCCESS;
finish:
	imageType = oldType;
	if(file.pszFile != NULL)
		FileSystem::Close(&file);
	
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

unsigned char* Image::_resize(unsigned char* image, int width, int height, int oldWidth, int oldHeight) {
#ifdef HAS_IPP
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
	ippiResizeGetBufferSize_8u(pSpec,dstsize,4,&bufsize);
	Ipp8u* pBuffer=ippsMalloc_8u(bufsize);

	//Allocate the temporary buffer used to store resized image.
	Ipp8u* tmpBuf = ippsMalloc_8u(width * height * 4);

	if(pBuffer != NULL) {
			status = ippiResizeLanczos_8u_C4R((const Ipp8u*)image, oldWidth*4, tmpBuf, width*4, dstOffset, dstsize, ippBorderRepl,0,pSpec, pBuffer );
	}

	ippsFree(initBuf);
	ippsFree(pSpec);
	ippsFree(pBuffer);
	ippsFree(image);

	nBytes = width * height * 4;
	return tmpBuf;
#else
#warning Resize not implemented for non-IPP platforms.
	//Just realloc the buffer instead (for now).
	nBytes = width * height * 4;
	image = (unsigned char*)realloc(image, nBytes);
	return image;
	

#endif
}

Image::~Image() {
	cleanup();
}