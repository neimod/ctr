/*
 * hw_command.h - Input command processing.
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
#include <stdbool.h>
#include <string.h>
#include "hw_commandbuffer.h"
#include "hw_command.h"

HWCommandBuffer* HW_CommandBufferAlloc(HWMemoryPool* pool)
{
	unsigned int capacity = 128;
	HWCommandBuffer* buf = (HWCommandBuffer*)HW_MemoryPoolAlloc(pool, sizeof(HWCommandBuffer));
	
	if (buf == 0)
		return 0;
	
	buf->data = (unsigned char*)HW_MemoryPoolAlloc(pool, capacity);		
	if (buf->data == 0)
		return 0;
		
	buf->capacity = capacity;
	buf->size = 0;
	buf->pool = pool;
	
	return buf;
}


void HW_CommandBufferDestroy(HWCommandBuffer* buf)
{
	if (buf == 0)
		return;
	HW_MemoryPoolFree(buf->pool, buf->data);
	HW_MemoryPoolFree(buf->pool, buf);
}	

int HW_CommandBufferReserve(HWCommandBuffer* buf, unsigned int size)
{
	unsigned int available;
	unsigned int capacity;
	
	if (buf == 0)
		return 0;
	
	available = buf->capacity - buf->size;
	capacity = buf->capacity;

	if (available >= size)
		return 1;
	
	while(available < size)
	{
		capacity *= 2;
		available = capacity - buf->size;
	}
	
	return HW_CommandBufferGrow(buf, capacity);
}
	
int HW_CommandBufferGrow(HWCommandBuffer* buf, unsigned int capacity)
{
	unsigned char* data;
	
	if (buf == 0)
		return 0;

	data = (unsigned char*)HW_MemoryPoolAlloc(buf->pool, capacity);
	if (data == 0)
		return 0;	
	
	if (buf->data)
	{
		memcpy(data, buf->data, buf->size);
		HW_MemoryPoolFree(buf->pool, buf->data);
	}
	
	buf->data = data;
	buf->capacity = capacity;
	
	return 1;
}

void HW_CommandBufferAppend(HWCommandBuffer* buf, const void* data, unsigned int size)
{
	if (buf == 0)
		return;
	
	
	if (HW_CommandBufferReserve(buf, size))
	{
		memcpy(buf->data + buf->size, data, size);
		buf->size += size;
	}
}

HWCommandBuffer* HW_CommandBufferAppendNumber(HWCommandBuffer* buf, HWCommandNumber* num)
{
	unsigned char buffer[4];
	
	if (buf == 0)
		return 0;
	
	buffer[0] = num->value >> 0;
	buffer[1] = num->value >> 8;
	buffer[2] = num->value >> 16;
	buffer[3] = num->value >> 24;
	
	HW_CommandBufferAppend(buf, buffer, num->type);
	
	return buf;
}

HWCommandBuffer* HW_CommandBufferAppendString(HWCommandBuffer* buf, HWCommandString* string)
{
	if (buf == 0)
		return 0;
	
	// Length does not include the NULL-terminator, so increase by 1 (string contains NULL-terminator).
	HW_CommandBufferAppend(buf, string->value, 1 + string->length);
	
	return buf;
}

HWCommandBuffer* HW_CommandBufferAppendWideString(HWCommandBuffer* buf, HWCommandWideString* wstring)
{
	if (buf == 0)
		return 0;
	
	// Length does not include the NULL-terminator, so increase by 1 (string contains NULL-terminator).
	HW_CommandBufferAppend(buf, wstring->value, (1+wstring->length) * sizeof(short));
	
	return buf;
}

HWCommandBuffer* HW_CommandBufferAllocWithNumber(HWMemoryPool* pool, HWCommandNumber* num)
{
	HWCommandBuffer* buf = HW_CommandBufferAlloc(pool);
	HW_CommandBufferAppendNumber(buf, num);
	return buf;
}

HWCommandBuffer* HW_CommandBufferAllocWithString(HWMemoryPool* pool, HWCommandString* string)
{
	HWCommandBuffer* buf = HW_CommandBufferAlloc(pool);
	HW_CommandBufferAppendString(buf, string);
	return buf;
}

HWCommandBuffer* HW_CommandBufferAllocWithWideString(HWMemoryPool* pool, HWCommandWideString* wstring)
{
	HWCommandBuffer* buf = HW_CommandBufferAlloc(pool);
	HW_CommandBufferAppendWideString(buf, wstring);
	return buf;
}

void HW_CommandBufferDump(HWCommandBuffer* buf)
{
	unsigned int i;
	if (buf == 0)
		return;
		
	for(i=0; i<buf->size; i++)
		printf("%02X ", buf->data[i]);
}
