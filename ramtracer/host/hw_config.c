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


// Private functions
void HW_ConfigGrow(HWConfig* config);
void HW_ConfigReserve(HWConfig* config, unsigned int size);
void HW_ConfigWrite(HWConfig* config, unsigned char* data, unsigned int size);
void HW_ConfigWrite16(HWConfig* config, unsigned int n);
void HW_ConfigWrite32(HWConfig* config, unsigned int n);



void HW_ConfigInit(HWConfig* config)
{
	config->data = 0;
	config->size = 0;
	config->capacity = 0;
}

void HW_ConfigClear(HWConfig* config)
{
   config->size = 0;
}

void HW_ConfigDestroy(HWConfig* config)
{
	if (config->data)
		free(config->data);
	config->data = 0;
	config->size = 0;
	config->capacity = 0;
}

void HW_ConfigGrow(HWConfig* config)
{
	unsigned int nextcapacity = config->capacity * 2;
	unsigned char* buffer = 0;


	if (nextcapacity == 0)
		nextcapacity = 16;
	
	buffer = malloc(nextcapacity);

	if (config->data)
	{
		memcpy(buffer, config->data, config->size);
		free(config->data);
	}

	config->capacity = nextcapacity;
	config->data = buffer;
}

void HW_ConfigReserve(HWConfig* config, unsigned int size)
{
	while(config->size + size > config->capacity)
		HW_ConfigGrow(config);
}

void HW_ConfigWrite(HWConfig* config, unsigned char* data, unsigned int size)
{
	HW_ConfigReserve(config, size);

	memcpy(config->data + config->size, data, size);
	
	config->size += size;
}

void HW_ConfigWrite16(HWConfig* config, unsigned int n)
{
	unsigned char data[2];

	data[0] = n>>0;
	data[1] = n>>8;

	HW_ConfigWrite(config, data, 2);
}

void HW_ConfigWrite32(HWConfig* config, unsigned int n)
{
	unsigned char data[4];

	data[0] = n>>0;
	data[1] = n>>8;
	data[2] = n>>16;
	data[3] = n>>24;

	HW_ConfigWrite(config, data, 4);
}

void HW_ConfigAddressRead(HWConfig* config, unsigned int address)
{
	HW_ConfigWrite16(config, address & ~0x8000);
}

void HW_ConfigAddressWrite(HWConfig* config, unsigned int address, unsigned int value)
{
	HW_ConfigWrite16(config, address | 0x8000);
	HW_ConfigWrite32(config, value);
}

void HW_ConfigDevice(FTDIDevice* dev, HWConfig* config, bool async)
{
	if (FTDIDevice_Write(dev, FTDI_INTERFACE_A, config->data, config->size, async)) {
		perror("Error writing configuration");
		exit(1);
	}
}
