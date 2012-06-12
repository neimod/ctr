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

#define HWBUF_SIZE (64 * 1024 * 1024)

#define HWBUF_FLAG_ALLOC_NODE (1<<0)
#define HWBUF_FLAG_ALLOC_BUFFER (1<<1)


/*
 * HWBuffer -- Buffer for storing real-time captured data.
 */

typedef struct HWBuffer {
	unsigned int size;
	unsigned int capacity;
	struct HWBuffer* next;
   unsigned int flags;
	unsigned char* buffer;
   unsigned int pos;
} HWBuffer;


typedef struct { 
	HWBuffer* head;
	HWBuffer* tail;
} HWBufferChain;

/*
 * Public functions
 */


HWBuffer*      HW_BufferAllocate(unsigned int size);
void           HW_BufferClear(HWBuffer* node);
unsigned int   HW_BufferFill(HWBuffer* node, unsigned char* buffer, unsigned int size);
void           HW_BufferReserve(HWBuffer* node, unsigned int size);
void           HW_BufferInit(HWBuffer* node, unsigned int size);
void           HW_BufferDestroy(HWBuffer* node);
void           HW_BufferAppend(HWBuffer* node, const void* buffer, unsigned int size);
void           HW_BufferResize(HWBuffer* node, unsigned int size);
void           HW_BufferGrow(HWBuffer* node, unsigned int minimumsize);
void           HW_BufferRemoveFront(HWBuffer* node, unsigned int size);

void           HW_BufferChainInit(HWBufferChain* chain);
HWBuffer*      HW_BufferChainGetFirst(HWBufferChain* chain);
HWBuffer*      HW_BufferChainGetLast(HWBufferChain* chain);
HWBuffer*      HW_BufferChainRemoveFirst(HWBufferChain* chain);
void           HW_BufferChainDestroyFirst(HWBufferChain* chain);
void           HW_BufferChainAppend(HWBufferChain* chain, HWBuffer* node);
HWBuffer*      HW_BufferChainAppendNew(HWBufferChain* chain);

#endif // __HW_BUFFER_H_
