/*
 * hw_main.h -  Main functionality for talking to the 3DS RAM
 *               tracing/injecting hardware over USB.
 *
 * Copyright (C) 2009 Micah Dowty
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

#ifndef __HW_COMMON_H_
#define __HW_COMMON_H_

#include "fastftdi.h"


/*
 * Public
 */

void HW_Init();
void HW_LoadPatchFile(const char* filename);
void HW_LoadFlatPatchFile(unsigned int address, const char* filename);
void HW_Setup(FTDIDevice *dev, const char *bitstream);
void HW_Trace(FTDIDevice *dev, const char *filename);

#endif // __HW_COMMON_H_
