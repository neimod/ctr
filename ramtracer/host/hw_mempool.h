/*
 * hw_mempool.h - Memory pool.
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

#ifndef __HW_MEMPOOL_H_
#define __HW_MEMPOOL_H_

#include <stdio.h>
#include "hw_buffer.h"

/*
 * HWMemoryPool -- The memory pool structure.
 */


typedef struct
{
	unsigned char* data;
	unsigned int capacity;
	unsigned int size;
} HWMemoryPool;

/*
 * Public functions
 */
void* HW_MemoryPoolAlloc(HWMemoryPool* pool, unsigned int size);
void HW_MemoryPoolFree(HWMemoryPool* pool, void* ptr);
void HW_MemoryPoolDestroy(HWMemoryPool* pool);
void HW_MemoryPoolInit(HWMemoryPool* pool, unsigned int capacity);
void HW_MemoryPoolClear(HWMemoryPool* pool);


#endif // __HW_MEMPOOL_H_
