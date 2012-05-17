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
#include "hw_buffer.h"

/*
 * HWConfig --  Worker structure for containing a complete configuration stream.
 */

/*
 * Public functions
 */

void HW_ConfigInit(HWBuffer* buffer);
void HW_ConfigClear(HWBuffer* buffer);
void HW_ConfigDestroy(HWBuffer* buffer);
void HW_ConfigAddressRead(HWBuffer* buffer, unsigned int address);
void HW_ConfigAddressWrite(HWBuffer* buffer, unsigned int address, unsigned int value);
int HW_ConfigDevice(FTDIDevice* dev, HWBuffer* buffer, bool async);

#endif // __HW_CONFIG_H_
