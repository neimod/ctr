/*
 * hw_commandtype.h - Command types.
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

#ifndef __HW_COMMANDTYPE_H_
#define __HW_COMMANDTYPE_H_

#include "hw_mempool.h"
#include "hw_commandbuffer.h"


#define CMDTYPE_NUMBER 1
#define CMDTYPE_BUFFER 2



/*
 * HWCommandType -- The command type structure.
 */

struct HWCommandNumberType;
struct HWCommandBufferType;

typedef struct HWCommandType
{
	unsigned int type;
	struct HWCommandType* next;
	union {
		struct HWCommandNumberType* number;
		struct HWCommandBufferType* buffer;
	};
} HWCommandType;

typedef struct HWCommandNumberType {
	HWCommandType header;
	HWCommandNumber data;
} HWCommandNumberType;

typedef struct HWCommandBufferType {
	HWCommandType header;
	HWCommandBuffer* data;
} HWCommandBufferType;


/*
 * Public functions
 */
HWCommandType* HW_CommandTypeAllocWithNumber(HWMemoryPool* pool, HWCommandNumber* num);
HWCommandType* HW_CommandTypeAllocWithBuffer(HWMemoryPool* pool, HWCommandBuffer* buffer);
HWCommandType* HW_CommandTypeAppend(HWCommandType* root, HWCommandType* child);

#endif // __HW_COMMANDTYPE_H_
