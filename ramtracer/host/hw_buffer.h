/*
 * hw_buffer.h - Buffer for storing real-time captured data.
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

#ifndef __HW_BUFFER_H_
#define __HW_BUFFER_H_

#include <stdint.h>

#define HWBUFSIZE (64 * 1024 * 1024)

/*
 * HWBuffer -- Buffer for storing real-time captured data.
 */

typedef struct HWBuffer {
	unsigned int available;
	unsigned int capacity;
	struct HWBuffer* next;
	unsigned char buffer[HWBUFSIZE];
} HWBuffer;


typedef struct { 
	HWBuffer* head;
	HWBuffer* tail;
} HWBufferChain;

/*
 * Public functions
 */

unsigned int HW_FillBuffer(HWBuffer* node, unsigned char* buffer, unsigned int size);
HWBuffer* HW_GetFirstBuffer(HWBufferChain* chain);
HWBuffer* HW_GetLastBuffer(HWBufferChain* chain);
void HW_RemoveFirstBuffer(HWBufferChain* chain);
HWBuffer* HW_AddNewBuffer(HWBufferChain* chain);

#endif // __HW_BUFFER_H_
