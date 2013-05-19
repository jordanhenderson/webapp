#include "Image.h"
#include "gif_lib.h"
extern "C" {
#include "neuquant32.h"
}
#include <ipp.h>
//This file contains gif-specific implementations.
//Some code referenced from http://wiki.swftools.org/viewgit/?a=viewblob&p=swftools-git%20&h=0d51d6c8780b4d2ea2fc6c0804f0af42ed164b96&hb=5ac80872f927e25921a47b3a39eed2cd9edc03d2&f=src/gif2swf.c
int Image::gifGetTransparentColor(int frame)
{
	int i;
	ExtensionBlock *ext = gif->SavedImages[frame].ExtensionBlocks;

	for (i = 0; i < gif->SavedImages[frame].ExtensionBlockCount; i++, ext++) {
		if ((ext->Function == GRAPHICS_EXT_FUNC_CODE) && (ext->Bytes[0] & 1)) {
			return ext->Bytes[3] == 0 ? 0 : (unsigned char) ext->Bytes[3];
		}
	}

	return -1;
}

void Image::gifInsertFrame(int frame) {
	unsigned char bgcolor;
	GifImageDesc* img = &gif->SavedImages[frame].ImageDesc;
	int rastersize = img->Height * img->Width;
	//Allocate the frame.
	frames[frame] = ippsMalloc_8u(rastersize * 4);
	//Set color map to use local or global color map. Local overrides global.
	ColorMapObject* map = img->ColorMap ? img->ColorMap : gif->SColorMap;
	int alpha = gifGetTransparentColor(frame);

	

	if(gif->SColorMap)
		bgcolor = gif->SBackGroundColor;
	else if(alpha >= 0) {
		bgcolor = alpha;
	} else {
		bgcolor = 0;
	}

	for(int i = 0; i < rastersize; i++) {
		unsigned char pixel = gif->SavedImages[frame].RasterBits[i];
		GifColorType c = map->Colors[pixel];
		int nColor = i * 4;
		if (pixel == bgcolor || pixel == alpha)
			frames[frame][nColor] = frames[frame][nColor+1] = frames[frame][nColor+2] = frames[frame][nColor+3] = 0;
		else {
		frames[frame][nColor] = c.Red;
		frames[frame][nColor + 1] = c.Green;
		frames[frame][nColor + 2] = c.Blue;
		frames[frame][nColor + 3] = 255;
		}
	}



}

void Image::gifMakeMap(unsigned char* image, int width, int height, unsigned char** map, unsigned char** raster) {
	palinitnet(NULL, 0, 1.0, image,width*height*4,256,
		IMAGE_COLORSPACE_RGBA, 1.8, 0.0,
		0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 0);

	double sample_factor = 1 + (double)width*(double)height / (512*512);
	if (sample_factor > 10) {
		sample_factor = 10;
	}

	learn(sample_factor, 0.0, 0);

	unsigned char cmap[MAXNETSIZE][4];

	getcolormap((unsigned char*)cmap, false);

	unsigned char remap[MAXNETSIZE];

	int x, top_idx, bot_idx;
	for (top_idx = 255, bot_idx = x = 0;  x < 256;  ++x) {
		if ( cmap[x][3] == 255) { /* maxval */
			remap[x] = top_idx--;
		} else {
			remap[x] = bot_idx++;
		}
	}


	GifColorType o[256];
	for(int c = 0; c < 256; c++) {
		o[remap[c]].Red = cmap[c][0];
		o[remap[c]].Green = cmap[c][1];
		o[remap[c]].Blue = cmap[c][2];
	}

	*map = (unsigned char*)GifMakeMapObject(
		256,
		(GifColorType*)o);

	//Allocate raster bits
	int rasterSize = width*height;
	*raster = (GifByteType*)malloc(rasterSize);

	//Write the frame to the savedImage's rasterbits.
	/* Assign the new colors */
	for( int j=0;j<rasterSize;j++) {
		(*raster)[j] = remap[inxsearch(image[j*4+3],
			image[j*4+2],
			image[j*4+1],
			image[j*4])];
	}

}