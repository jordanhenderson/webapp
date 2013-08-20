#include "Image.h"
#include "gif_lib.h"
extern "C" {
#include "neuquant32.h"
}
#ifdef HAS_IPP
#include <ipp.h>
#endif

void Image::gifInsertFrame(int frame) {
	unsigned char bgcolor;
	GifImageDesc* img = &gif->SavedImages[frame].ImageDesc;
	int rastersize = width * height;

	
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

	//Allocate the initial frame.
	if(frame == 0) {
#ifdef HAS_IPP
		frames[frame] = ippsMalloc_8u(rastersize * 4);
#else
		frames[frame] = new unsigned char[rastersize*4];
#endif
		//Fill the frame using it's bg color.
		unsigned char target[4];
		GifColorType c;
		c = map->Colors[bgcolor];
		
		target[0] = c.Red;
		target[1] = c.Green;
		target[2] = c.Blue;
		target[3] = 255;
		for(int i = 0; i < rastersize; i++) {
			memcpy(frames[frame]+i*4, &target, 4);

		}
		

	}

	unsigned char* framePixels = new unsigned char[width * height * 4]();

	for( int i = 0; i < img->Height; i++) {
		int pixelRowNumber = i;	
		pixelRowNumber += img->Top;
		if( pixelRowNumber < height ) {
			int k = pixelRowNumber * width;
			int dx = k + img->Left;
			int dlim = dx + img->Width; 
			if( (k + width) < dlim ) {
				dlim = k + width; // past dest edge
			}
			int sx = i * img->Width;
			while (dx < dlim) {
				int indexInColourTable = (int) gif->SavedImages[frame].RasterBits[sx++];
				GifColorType c;
				unsigned char a = 255;
				//dispose of transparent colours.
				if(gcb.TransparentColor > 0 && indexInColourTable == alpha) {
					a = 0;
				}
				else {
					if(indexInColourTable < map->ColorCount) {
						c = map->Colors[indexInColourTable];
					}
					else {
						//Invalid colour (out of map range).
						c.Red = c.Blue = c.Green = 0;
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
	for( int th = 0; th < height; th++ ) {
		for( int tw = 0; tw < width; tw++ ) {
			if(framePixels[(count*4)+3] == 255) {
				memcpy(&frames[frame][((th*width)+tw)*4], &framePixels[(count*4)], 4);
			}
			count++;
		}
	}

	


	if(frame + 1 < imagecount) {
		#ifdef HAS_IPP
		frames[frame + 1] = ippsMalloc_8u(rastersize * 4);
#else
		frames[frame + 1] = new unsigned char[rastersize * 4]();
#endif
		
		if(gcb.DisposalMode == DISPOSE_DO_NOT)
			memcpy(frames[frame+1],frames[frame], rastersize*4);
		else if(gcb.DisposalMode == DISPOSE_BACKGROUND) {
			if(bgcolor == alpha) 
				memset(frames[frame+1], '\0', rastersize*4);
			else {
				GifColorType c = map->Colors[bgcolor];
				for(int i = 0; i < rastersize; i++) {
					memcpy(frames[frame+1]+i*4, &c, 3);
					*(frames[frame+1]+(i*4)+3) = 255;
				}

			}

		} else if(gcb.DisposalMode == DISPOSE_PREVIOUS && frame > 0) {
			//Not supported, just use current frame.
			memcpy(frames[frame+1], frames[frame], rastersize*4);
		} else {
			//Unspecified.
			if(frame == 0)
				memset(frames[frame+1], '\0', rastersize * 4);
			else
				memcpy(frames[frame+1], framePixels, rastersize*4);
		}
	}

	//Mark the gcb as not having a transparent color.
	gcb.TransparentColor = -1;
	//Remove disposal mode (unoptimised, but required for now).
	gcb.DisposalMode = DISPOSE_BACKGROUND;

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
	int rasterSize = width*height*sizeof(GifByteType);
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