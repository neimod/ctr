/*
 * hw_commandbuffer.h - Command buffer.
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

#ifndef __HW_COMMANDBUFFER_H_
#define __HW_COMMANDBUFFER_H_

#include "hw_mempool.h"


#define NUMTYPE_BYTE 1
#define NUMTYPE_SHORT 2
#define NUMTYPE_LONG 4

#define MAXSTRINGSIZE	255


/*
 * HWCommandBuffer -- The command buffer structure.
 */


typedef struct
{
	unsigned char* data;
	unsigned int size;
	unsigned int capacity;
	HWMemoryPool* pool;
} HWCommandBuffer;

typedef struct HWCommandNumber {
	unsigned int value;
	unsigned int type;
} HWCommandNumber;


typedef struct HWCommandString {
	char* value;
	unsigned int length;
} HWCommandString;

typedef struct HWCommandWideString {
	short* value;
	unsigned int length;
} HWCommandWideString;

/*
 * Public functions
 */

HWCommandBuffer* HW_CommandBufferAlloc(HWMemoryPool* pool);
HWCommandBuffer* HW_CommandBufferAllocWithNumber(HWMemoryPool* pool, HWCommandNumber* num);
HWCommandBuffer* HW_CommandBufferAllocWithString(HWMemoryPool* pool, HWCommandString* string);
HWCommandBuffer* HW_CommandBufferAllocWithWideString(HWMemoryPool* pool, HWCommandWideString* wstring);
void HW_CommandBufferDestroy(HWCommandBuffer* buf);
int HW_CommandBufferReserve(HWCommandBuffer* buf, unsigned int size);
int HW_CommandBufferGrow(HWCommandBuffer* buf, unsigned int capacity);
void HW_CommandBufferAppend(HWCommandBuffer* buf, const void* data, unsigned int size);
HWCommandBuffer* HW_CommandBufferAppendNumber(HWCommandBuffer* buf, HWCommandNumber* num);
HWCommandBuffer* HW_CommandBufferAppendString(HWCommandBuffer* buf, HWCommandString* string);
HWCommandBuffer* HW_CommandBufferAppendWideString(HWCommandBuffer* buf, HWCommandWideString* wstring);
void HW_CommandBufferDump(HWCommandBuffer* buf);

#endif // __HW_COMMANDBUFFER_H_
