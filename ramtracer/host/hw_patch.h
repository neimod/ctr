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

#ifndef __HW_PATCH_H_
#define __HW_PATCH_H_

#include <stdint.h>
#include "fastftdi.h"
#include "hw_config.h"

#define HWCAMENTRYCOUNT 16
#define	HWPATCHDATASIZE (16 * 1024)

/*
 * HWPatchCamEntry -- Patch address CAM entry.
 */

typedef struct {
	unsigned int data;
	unsigned int mask;
} HWPatchCamEntry;

typedef struct {
	HWPatchCamEntry entries[HWCAMENTRYCOUNT];
} HWPatchCamMemory;

typedef struct {
	unsigned short entries[HWCAMENTRYCOUNT];
} HWPatchOffsetMemory;

typedef struct {
	unsigned char data[HWPATCHDATASIZE];
} HWPatchContentMemory;

typedef struct {
	unsigned int address;
	unsigned int count;
	unsigned int bypassaddress;
	unsigned int datalo;
	unsigned int datahi;	
} HWPatchTrigger;


typedef struct {
	HWPatchCamMemory cam;
	HWPatchOffsetMemory offset;
	HWPatchContentMemory content;
	HWPatchTrigger trigger;

	unsigned int currententryindex;
	unsigned int currentdatawordindex;
	int mode;
	
} HWPatchContext;

/*
 * Public functions
 */
void HW_PatchInit(HWPatchContext* ctx);
void HW_SetPatchAddress(HWPatchContext* ctx, unsigned int index, unsigned int address, unsigned int mask);
void HW_SetPatchOffset(HWPatchContext* ctx, unsigned int index, unsigned int offset, unsigned int address);
void HW_SetPatchData(HWPatchContext* ctx, unsigned int offset, unsigned char* data, unsigned int size);
void HW_AddPatch(HWPatchContext* ctx, unsigned int address, unsigned char* data, unsigned int size);
void HW_PatchDevice(HWPatchContext* ctx, FTDIDevice *dev);
void HW_SetPatchWriteTrigger(HWPatchContext* ctx, unsigned int address, unsigned int count, unsigned char* word);
void HW_SetPatchTriggerBypass(HWPatchContext* ctx, unsigned int address);
void HW_SetPatchingMode(HWPatchContext* ctx, int enabled);
void HW_ConfigureClockSpeed(HWConfig* config, FTDIDevice* dev, int clockspeed);

#endif // __HW_PATCH_H_
