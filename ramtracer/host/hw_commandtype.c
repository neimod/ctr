/*
 * hw_commandtype.h - Command type.
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
#include "hw_commandtype.h"
#include "hw_mempool.h"


HWCommandType* HW_CommandTypeAllocWithNumber(HWMemoryPool* pool, HWCommandNumber* num)
{
	HWCommandNumberType* tp = 0;
	
	if (num == 0)
		return;

	tp = (HWCommandNumberType*)HW_MemoryPoolAlloc(pool, sizeof(HWCommandNumberType));
	if (tp == 0)
		return 0;
		
	tp->header.type = CMDTYPE_NUMBER;
	tp->header.number = tp;
	tp->header.next = 0;
	tp->data = *num;
	
	return &tp->header;
}

HWCommandType* HW_CommandTypeAllocWithBuffer(HWMemoryPool* pool, HWCommandBuffer* buffer)
{
	HWCommandBufferType* tp = 0;
	
	if (buffer == 0)
		return 0;
		
	tp = (HWCommandBufferType*)HW_MemoryPoolAlloc(pool, sizeof(HWCommandBufferType));		
	if (tp == 0)
		return 0;
		
	tp->header.type = CMDTYPE_BUFFER;
	tp->header.buffer = tp;
	tp->header.next = 0;
	tp->data = buffer;
	
	return &tp->header;
}

HWCommandType* HW_CommandTypeAppend(HWCommandType* root, HWCommandType* child)
{
	HWCommandType* parent;
	
	if (root == 0 || child == 0)
		return root;
		
	parent = root;
	while(parent->next != 0)
		parent = parent->next;

	parent->next = child;
	
	return root;
}
