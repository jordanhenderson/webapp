/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Image.h"
#include "gif_lib.h"
extern "C" {
#include "neuquant32.h"
}

/**
 * Produce a decoded frame (at the specified index) storing appropriately
 * in the Image object. Produced frames are stored in RGBA format.
 * @param frame the index of the frame to decode
*/
void Image::gifInsertFrame(unsigned int frame)
{
	GifImageDesc* img = &gif->SavedImages[frame].ImageDesc;
	int rastersize = width * height * 4;
	//Set color map to use local or global colour map. Local overrides global.
	ColorMapObject* map = img->ColorMap ? img->ColorMap : gif->SColorMap;
	//Get the GCB to determine transparent colour.
	GraphicsControlBlock gcb;
	DGifSavedExtensionToGCB(gif, frame, &gcb);

	int alpha = gcb.TransparentColor;
	unsigned char bgcolor = 0;
	if(gif->SColorMap)
		bgcolor = gif->SBackGroundColor;
	else if(alpha >= 0) {
		bgcolor = alpha;
	} else {
		bgcolor = 0;
	}

	unsigned char* framePixels = frames[frame];
	//Allocate the initial frame.
	if(frame == 0) {
		framePixels = new unsigned char[rastersize];

		//Fill the frame using it's background color.
		unsigned char target[4];
		GifColorType c;
		c = map->Colors[bgcolor];

		target[0] = c.Red;
		target[1] = c.Green;
		target[2] = c.Blue;
		target[3] = 255;
		for(int i = 0; i < rastersize; i += 4) {
			memcpy(framePixels + i, &target, 4);
		}
	}

	//Iterate over the rasterized pixel data, finding it's actual colour
	//value in the colour map, and producing RGBA pixel data in framePixels.
	for(int i = 0; i < img->Height; i++) {
		int pixelRowNumber = i;
		//Offset the image by the Top value
		pixelRowNumber += img->Top;
		if( pixelRowNumber < height ) {
			int k = pixelRowNumber * width;
			//Offset the x by the Left value.
			int dest_x = k + img->Left;
			int dlim = dest_x + img->Width;
			if( (k + width) < dlim ) dlim = k + width; // past dest edge
			int source_x = i * img->Width;
			while (dest_x < dlim) {
				int indexInColourTable = (int) gif->SavedImages[frame].RasterBits[source_x++];
				GifColorType c = {};
				unsigned char a = 255;
				//dispose of transparent colours.
				if(gcb.TransparentColor > 0 && indexInColourTable == alpha) {
					a = 0;
				} else {
					if(indexInColourTable < map->ColorCount) {
						c = map->Colors[indexInColourTable];
					}
					//Invalid colour (out of map range).
					else c.Red = c.Blue = c.Green = 0;
				}
				framePixels[dest_x*4] = c.Red;
				framePixels[dest_x*4+1] = c.Green;
				framePixels[dest_x*4+2] = c.Blue;
				framePixels[dest_x*4+3] = a;
				dest_x++;
			}
		}
	}

	//If there is a frame left to process
	if(frame + 1 < imagecount) {
		unsigned char* nextFrame = frames[frame + 1];
		nextFrame = new unsigned char[rastersize];
		if(gcb.DisposalMode == DISPOSE_DO_NOT)
			//Keep the current frame's pixels in the next frame.
			memcpy(nextFrame,framePixels, rastersize);
		else if(gcb.DisposalMode == DISPOSE_BACKGROUND) {
			//Apply background to next frame.
			if(bgcolor == alpha) memset(nextFrame, '\0', rastersize);
			else {
				//Apply background colour to next frame.
				GifColorType c = map->Colors[bgcolor];
				for(int i = 0; i < rastersize; i += 4) {
					memcpy(nextFrame + i, &c, 3);
					*(nextFrame + i + 3) = 255;
				}
			}
		} else if(gcb.DisposalMode == DISPOSE_PREVIOUS && frame > 0) {
			//Not supported, just use current frame.
			memcpy(nextFrame, framePixels, rastersize);
		} else {
			//Unspecified.
			if(frame == 0)
				memset(nextFrame, '\0', rastersize);
			else
				memcpy(nextFrame, framePixels, rastersize);
		}
	}

	//Mark the gcb as not having a transparent color.
	gcb.TransparentColor = -1;
	//Remove disposal mode (unoptimised, but required for now).
	gcb.DisposalMode = DISPOSE_BACKGROUND;

	//Store the new gif data.
	EGifGCBToSavedExtension(&gcb, gif, frame);
	delete[] framePixels;
}

/**
 * Rasterize a gif image, producing an array of rasterized bits and a
 * colour map.
 * @param frame the frame to rasterize.
 * @param map the output colour map destination
 * @param raster the output raster array destination
*/
void Image::gifRasterizeFrame(unsigned int frame, unsigned char** map,
							  unsigned char** raster)
{
	unsigned char* image = frames[frame];
	palinitnet(NULL, 0, 1.0, image, width * height * 4, 256,
			   1, 1.8, 0.0, 0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.5, 0);

	unsigned int sample_factor = (int)(1 +
									   (double)width * (double)height / (512*512));
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

	*(map) = (unsigned char*)GifMakeMapObject(256, (GifColorType*)o);

	//Allocate raster bits
	int rasterSize = width * height * sizeof(GifByteType);
	(*raster) = (GifByteType*)malloc(rasterSize);

	//Write the frame to the savedImage's rasterbits.
	/* Assign the new colors */
	for( int j=0; j<rasterSize; j++) {
		(*raster)[j] = remap[inxsearch(image[j*4+3],
									   image[j*4+2],
									   image[j*4+1],
									   image[j*4])];
	}
}
