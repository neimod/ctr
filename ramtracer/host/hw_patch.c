/*
 * hw_patch.h - RAM patching related functionality.
 *
 * Copyright (C) 2012 neimod
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hw_patch.h"
#include "fastftdi.h"
#include "hw_config.h"

#define PATCHOFFSETRAM		0x1000
#define PATCHDATARAM		0x2000
#define PATCHCAMRAM			0x3000
#define PATCHDATAUPPER		0x0
#define PATCHCAMMASK		0x1


void HW_PatchInit(HWPatchContext* ctx)
{
	unsigned int i;


	for(i=0; i<HWCAMENTRYCOUNT; i++)
	{
		// Set to unmatchable
		ctx->cam.entries[i].data = ~0;
		ctx->cam.entries[i].mask = ~0;

		ctx->offset.entries[i] = 0;
	}

	memset(ctx->content.data, 0, HWPATCHDATASIZE);

	ctx->currententryindex = 0;
	ctx->currentdataoffset = 0;
}

// Sets an address range of a RAM address that needs to be patched, translated into a patch CAM entry
void HW_SetPatchAddress(HWPatchContext* ctx, unsigned int index, unsigned int address, unsigned int mask)
{
	HWPatchCamEntry entry;
	
	entry.data = (address & (~mask));
	entry.mask = ((~address) & (~mask));

	ctx->cam.entries[index] = entry;
}

// Sets the offset pointer for a CAM entry into the patch buffer.
// NOTE: Needs to have the CAM entry set first.
void HW_SetPatchOffset(HWPatchContext* ctx, unsigned int index, unsigned int offset, unsigned int address)
{
	unsigned short hwoffset = offset - address;
	
	ctx->offset.entries[index] = hwoffset;
}

void HW_SetPatchData(HWPatchContext* ctx, unsigned int offset, unsigned char* data, unsigned int size)
{
	memcpy(ctx->content.data + offset, data, size);
}


void HW_AddPatch(HWPatchContext* ctx, unsigned int address, unsigned char* data, unsigned int size)
{
	unsigned int words;


	if (address & 7)
	{
		fprintf(stderr, "PATCH: Address must be aligned to 8 bytes.\n");
		exit(1);
	}

	if (size & 7)
	{
		fprintf(stderr, "PATCH: Size must be aligned to 8 bytes.\n");
		exit(1);
	}

	address /= 8;
	words = size / 8;

	while (words) 
	{
		unsigned int blockMask = 0x00ffffff;
		unsigned int blockOffset = ctx->currentdataoffset;
		unsigned int blockIndex = ctx->currententryindex;
		unsigned int blockWords;
		unsigned int blockSize;

		if (blockIndex >= HWCAMENTRYCOUNT)
		{
			fprintf(stderr, "PATCH: Out of CAM entries (patch region too complex)\n");
			exit(1);
		}

		while (blockMask >= words || (blockMask & address)) 
		{
			blockMask >>= 1;
		}

		blockWords = blockMask + 1;
		blockSize = blockWords * 8;

		fprintf(stdout, "PATCH: CAM entry %02d, addr=%08x mask=%08x offset=%04x size=%08x\n", blockIndex, address, blockMask, blockOffset, blockSize);

		HW_SetPatchAddress(ctx, blockIndex, address, blockMask);
		HW_SetPatchOffset(ctx, blockIndex, blockOffset, address);
		HW_SetPatchData(ctx, blockOffset, data, blockSize);

		words -= blockWords;
		address += blockWords;
		data += blockSize;

		ctx->currentdataoffset += blockSize;
		ctx->currententryindex++;
	}
}

void HW_PatchDevice(HWPatchContext* ctx, FTDIDevice *dev)
{
	HWConfig config;
	unsigned int i;

	HW_ConfigInit(&config);

	for(i=0; i<HWCAMENTRYCOUNT; i++)
		HW_ConfigAddressWrite(&config, PATCHOFFSETRAM + i, ctx->offset.entries[i]);

	for(i=0; i<HWCAMENTRYCOUNT; i++)
	{
		HW_ConfigAddressWrite(&config, PATCHCAMMASK, ctx->cam.entries[i].mask);
		HW_ConfigAddressWrite(&config, PATCHCAMRAM + i, ctx->cam.entries[i].data);
	}
	
	for(i=0; i<HWPATCHDATASIZE/8; i++)
	{
		unsigned char* data = ctx->content.data + i*8;
		unsigned int lowerdata;
		unsigned int upperdata;

		lowerdata = (data[4]<<24) | (data[5]<<16) | (data[6]<<8) | data[7];
		upperdata = (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3];

		HW_ConfigAddressWrite(&config, PATCHDATAUPPER, upperdata);
		HW_ConfigAddressWrite(&config, PATCHDATARAM + i, lowerdata);
	}

	HW_ConfigDevice(dev, &config);

	fprintf(stdout, "PATCH: Configured patches.\n");
}
