#include <stdlib.h>
#include <string.h>
#include "hw_buffer.h"

// Allocate buffer of static size (usually does not change size)
HWBuffer* HW_BufferAllocate(unsigned int size)
{
   unsigned int headersize = (sizeof(HWBuffer) + 7) & (~7);
	unsigned char* memory = 0;	
   HWBuffer* node = 0;
   
   if (size < 16)
      size = 16;

	memory = malloc(headersize + size);	
   node = (HWBuffer*)memory;   
   

	node->size = 0;
   node->pos = 0;
	node->capacity = size;
	node->next = 0;
   node->flags = HWBUF_FLAG_ALLOC_NODE;
   node->buffer = memory + headersize;

   return node;
}

// Initialize buffer with initial size (can change)
void HW_BufferInit(HWBuffer* node, unsigned int size)
{
	unsigned char* buffer = 0;
   
   if (size < 16)
      size = 16;
   
	buffer = malloc(size);	
   
	node->size = 0;
   node->pos = 0;
	node->capacity = size;
	node->next = 0;
   node->flags = HWBUF_FLAG_ALLOC_BUFFER;
   node->buffer = buffer;
}

void HW_BufferDestroy(HWBuffer* node)
{
   if (node->flags & HWBUF_FLAG_ALLOC_BUFFER)
   {
      free(node->buffer);
      node->buffer = 0;
   }

   if (node->flags & HWBUF_FLAG_ALLOC_NODE)
      free(node);
}

void HW_BufferClear(HWBuffer* node)
{
   node->size = 0;
   node->pos = 0;
}

void HW_BufferRemoveFront(HWBuffer* node, unsigned int size)
{
   unsigned int available = node->size - node->pos;
   
   if (size > available)
      size = available;
   
   memmove(node->buffer, node->buffer + node->pos + size, available - size);
   
   node->size = available - size;
   node->pos = 0;
}


// Fill buffer data -- only up to the capacity of the HWBuffer
unsigned int HW_BufferFill(HWBuffer* node, unsigned char* buffer, unsigned int size)
{
   unsigned int available = node->capacity - node->size;
   
	if (available < size)
		size = available;
	
	memcpy(node->buffer + node->size, buffer, size);
	
	node->size += size;
	
	return size;
}

// Append buffer data -- allow growing of buffer
void HW_BufferAppend(HWBuffer* node, const void* buffer, unsigned int size)
{
   unsigned int available = node->capacity - node->size;
   
   if (available < size)
		HW_BufferGrow(node, node->size + size);
	
	memcpy(node->buffer + node->size, buffer, size);
	
	node->size += size;
}

void HW_BufferReserve(HWBuffer* node, unsigned int size)
{
   unsigned int available = node->capacity - node->size;
   
   if (available < size)
      HW_BufferGrow(node, node->size + size);
}

void HW_BufferGrow(HWBuffer* node, unsigned int minimumsize)
{
   unsigned int capacity = node->capacity;
   
   if (capacity < 16)
      capacity = 16;
   
   while(capacity <= minimumsize)
      capacity *= 2;
   
   HW_BufferResize(node, capacity);
}

void HW_BufferResize(HWBuffer* node, unsigned int size)
{
   unsigned char* buffer = 0;

   if (size < node->capacity)
   {
      node->capacity = size;
      
      if (node->size > node->capacity)
         node->size = node->capacity;
      
      return;
   }
   
   buffer = malloc(size);
   
   memcpy(buffer, node->buffer, node->size);
   node->capacity = size;
   
   if (node->flags & HWBUF_FLAG_ALLOC_BUFFER)
      free(node->buffer);
   
   node->flags |= HWBUF_FLAG_ALLOC_BUFFER;
   node->buffer = buffer;
}

void HW_BufferChainInit(HWBufferChain* chain)
{
   chain->head = 0;
   chain->tail = 0;
}

HWBuffer* HW_BufferChainGetFirst(HWBufferChain* chain)
{
	return chain->head;
}

HWBuffer* HW_BufferChainGetLast(HWBufferChain* chain)
{
	return chain->tail;
}

HWBuffer* HW_BufferChainRemoveFirst(HWBufferChain* chain)
{
	HWBuffer* node = chain->head;
	
	if (node)
	{
		chain->head = node->next;
		if (chain->head == 0)
			chain->tail = 0;
	}
   
   return node;
}


void HW_BufferChainDestroyFirst(HWBufferChain* chain)
{
	HWBuffer* node = HW_BufferChainRemoveFirst(chain);
	
	if (node)
      HW_BufferDestroy(node);
}

void HW_BufferChainAppend(HWBufferChain* chain, HWBuffer* node)
{
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
}


HWBuffer* HW_BufferChainAppendNew(HWBufferChain* chain)
{
   HWBuffer* node = HW_BufferAllocate(HWBUF_SIZE);
   
   HW_BufferChainAppend(chain, node);
	
	return node;
}
