#include "Image.h"
#include "gif_lib.h"
extern "C" {
#include "neuquant32.h"
}
#include <ipp.h>


void Image::gifInsertFrame(int frame) {
	unsigned char bgcolor;
	GifImageDesc* img = &gif->SavedImages[frame].ImageDesc;
	int rastersize = gif->SWidth * gif->SHeight;
	//Allocate the frame.
	frames[frame] = ippsMalloc_8u(rastersize * 4);
	//Set color map to use local or global color map. Local overrides global.
	ColorMapObject* map = img->ColorMap ? img->ColorMap : gif->SColorMap;
	GraphicsControlBlock gcb;
	DGifSavedExtensionToGCB(gif, frame, &gcb);

	int alpha = gcb.TransparentColor;
	if(gif->SColorMap)
		bgcolor = gif->SBackGroundColor;
	else if(alpha >= 0) {
		bgcolor = alpha;
	} else {
		bgcolor = 0;
	}

	if(frame > 0 && gcb.DisposalMode == DISPOSE_DO_NOT) {
		memcpy(frames[frame],frames[frame - 1], rastersize*4);
	}

	unsigned char* framePixels = new unsigned char[gif->SWidth * gif->SHeight * 4]();

	for( int i = 0; i < img->Height; i++) {
		int pixelRowNumber = i;	
		pixelRowNumber += img->Top;
		if( pixelRowNumber < gif->SHeight ) {
			int k = pixelRowNumber * gif->SWidth;
			int dx = k + img->Left;
			int dlim = dx + img->Width; 
			if( (k + gif->SWidth) < dlim ) {
				dlim = k + gif->SWidth; // past dest edge
			}
			int sx = i * img->Width;
			while (dx < dlim) {
				int indexInColourTable = (int) gif->SavedImages[frame].RasterBits[sx++];
				GifColorType c;
				unsigned char a;
				if(gcb.TransparentColor > 0 && indexInColourTable == alpha) {
					c.Red = c.Blue = c.Green = a = 0;
				}
				else {
					if(indexInColourTable < map->ColorCount) {
						c = map->Colors[indexInColourTable];
						a = 255;
					}
					else {
						c.Red = c.Blue = c.Green = 0;
						a = 255;
					}
				}
				framePixels[dx*4] = c.Red;
				framePixels[dx*4+1] = c.Green;
				framePixels[dx*4+2] = c.Blue;
				framePixels[dx*4+3] = a;
				dx++;
			}
		}
	}
	
	int count = 0;
	for( int th = 0; th < gif->SHeight; th++ ) {
		for( int tw = 0; tw < gif->SWidth; tw++ ) {
			if( framePixels[count*4] != 0 && framePixels[(count*4)+1] != 0 && framePixels[(count*4)+2] != 0 && framePixels[(count*4)+3] != 0) {
				memcpy(&frames[frame][((th*gif->SWidth)+tw)*4], &framePixels[(count*4)], 4);
			}
			count++;
		}
	}
	//Mark the gcb as not having a transparent color.
	gcb.TransparentColor = -1;
	EGifGCBToSavedExtension(&gcb, gif, frame);
	delete[] framePixels;

}

void Image::gifMakeMap(unsigned char* image, int width, int height, unsigned char** map, unsigned char** raster) {
	
	palinitnet(NULL, 0, 1.0, image,width*height*4,256,
		1, 1.8, 0.0,
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