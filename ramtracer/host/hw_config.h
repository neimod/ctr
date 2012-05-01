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

#ifndef __HW_CONFIG_H_
#define __HW_CONFIG_H_

#include "fastftdi.h"

/*
 * HWConfig --  Worker structure for containing a complete configuration stream.
 */


typedef struct
{
	unsigned char* data;
	unsigned int size;
	unsigned int capacity;
} HWConfig;


/*
 * Public functions
 */

void HW_ConfigInit(HWConfig* config);
void HW_ConfigClear(HWConfig* config);
void HW_ConfigDestroy(HWConfig* config);
void HW_ConfigAddressRead(HWConfig* config, unsigned int address);
void HW_ConfigAddressWrite(HWConfig* config, unsigned int address, unsigned int value);
void HW_ConfigDevice(FTDIDevice* dev, HWConfig* config, bool async);

#endif // __HW_CONFIG_H_
