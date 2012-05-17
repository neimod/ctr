/*
 * hw_config.h - Functionality for configuring the RAM tracer.
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
#include "hw_config.h"

#define HWCONFIG_INITIAL_BUFSIZE 16

// Private functions
static void HW_ConfigWrite16(HWBuffer* buffer, unsigned int n);
static void HW_ConfigWrite32(HWBuffer* buffer, unsigned int n);

void HW_ConfigInit(HWBuffer* buffer)
{
   HW_BufferInit(buffer, HWCONFIG_INITIAL_BUFSIZE);
}

void HW_ConfigClear(HWBuffer* buffer)
{
   HW_BufferClear(buffer);
}

void HW_ConfigDestroy(HWBuffer* buffer)
{
   HW_BufferDestroy(buffer);
}

void HW_ConfigWrite16(HWBuffer* buffer, unsigned int n)
{
	unsigned char data[2];

	data[0] = n>>0;
	data[1] = n>>8;

	HW_BufferAppend(buffer, data, 2);
}

void HW_ConfigWrite32(HWBuffer* buffer, unsigned int n)
{
	unsigned char data[4];

	data[0] = n>>0;
	data[1] = n>>8;
	data[2] = n>>16;
	data[3] = n>>24;

	HW_BufferAppend(buffer, data, 4);
}

void HW_ConfigAddressRead(HWBuffer* buffer, unsigned int address)
{
	HW_ConfigWrite16(buffer, address & ~0x8000);
}

void HW_ConfigAddressWrite(HWBuffer* buffer, unsigned int address, unsigned int value)
{
	HW_ConfigWrite16(buffer, address | 0x8000);
	HW_ConfigWrite32(buffer, value);
}

int HW_ConfigDevice(FTDIDevice* dev, HWBuffer* buffer, bool async)
{
	return FTDIDevice_Write(dev, FTDI_INTERFACE_A, buffer->buffer, buffer->size, async);
}
