#include <stdlib.h>
#include "hw_buffer.h"

unsigned int HW_FillBuffer(HWBuffer* node, unsigned char* buffer, unsigned int size)
{
	if (node->available < size)
	{
		size = node->available;
	}
	
	memcpy(node->buffer + node->capacity - node->available, buffer, size);
	
	node->available -= size;
	
	return size;
}

HWBuffer* HW_GetFirstBuffer(HWBufferChain* chain)
{
	return chain->head;
}

HWBuffer* HW_GetLastBuffer(HWBufferChain* chain)
{
	return chain->tail;
}

void HW_RemoveFirstBuffer(HWBufferChain* chain)
{
	HWBuffer* node = chain->head;
	
	if (node)
	{
		chain->head = node->next;
		if (chain->head == 0)
			chain->tail = 0;
		free(node);
	}
}

HWBuffer* HW_AddNewBuffer(HWBufferChain* chain)
{
	HWBuffer* node = malloc(sizeof(HWBuffer));
	
	node->available = HWBUFSIZE;
	node->capacity = HWBUFSIZE;
	node->next = 0;
	
	if (chain->head == 0)
	{
		chain->head = node;
		chain->tail = node;
	}
	else
	{
		chain->tail->next = node;
		chain->tail = node;
	}
	
	return node;
}
