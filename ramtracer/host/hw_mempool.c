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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw_mempool.h"

	
	
void* HW_MemoryPoolAlloc(HWMemoryPool* pool, unsigned int size)
{
	unsigned int available = pool->capacity - pool->size;
	void* result = 0;
	
	size = (size + 8) & ~8;
	
	if (available < size)
	{
		fprintf(stderr, "error allocating memory from pool\n");
		return 0;
	}

	result = (void*)&pool->data[pool->size];
	
	pool->size += size;
	return result;
}

void HW_MemoryPoolFree(HWMemoryPool* pool, void* ptr)
{
}


void HW_MemoryPoolInit(HWMemoryPool* pool, unsigned int capacity)
{
	pool->data = (unsigned char*)malloc(capacity);
	pool->capacity = capacity;
	pool->size = 0;
}

void HW_MemoryPoolClear(HWMemoryPool* pool)
{
	pool->size = 0;
}


void HW_MemoryPoolDestroy(HWMemoryPool* pool)
{
	free(pool->data);
	pool->size = 0;
	pool->capacity = 0;
}
