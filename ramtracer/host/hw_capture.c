/*
 * hw_capture.h - Real-time capture processing of RAM tracing data.
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
#include <pthread.h>
#include <zlib.h>
#include "hw_capture.h"
#include "hw_config.h"

#include "utils.h"

#define ZCHUNK (64 * 1024 * 1024)
#define ZCOMPLEVEL 2

static HWBuffer configctx;
static HWBuffer writefifo;
static FILE* fifofile = 0;

// Private functions
static void* HW_CaptureThread(void* arg);

static void fix_data_order(unsigned int *mask, unsigned char *data)
{
	unsigned char input_data[8];
   
	*mask = ((*mask << 2) | (*mask >> 6)) & 0xFF;
   
	memcpy(input_data, data, 8);
	data[0] = input_data[6];
	data[1] = input_data[7];
	data[2] = input_data[0];
	data[3] = input_data[1];
	data[4] = input_data[2];
	data[5] = input_data[3];
	data[6] = input_data[4];
	data[7] = input_data[5];
}

int HW_ProcessSample(FTDIDevice* dev, uint8_t* sampledata)
{
   unsigned int header;
	unsigned int address;
	static unsigned int fifocount = 0;
   
	header = sampledata[0];
	address = sampledata[1] | (sampledata[2]<<8) | (sampledata[3]<<16);
   
	if ( (header == 1) && (address == 0x7FFFE0))
	{
      unsigned char memdata[8];
      unsigned int mask = sampledata[12];
      static unsigned char testbuffer[4] = {1, 2, 3, 4};
      
		//fifocount++;
		//fprintf(stdout, "\nFifo was read %d\n", fifocount);
		
      memcpy(memdata, sampledata+4, 8);
      fix_data_order(&mask, memdata);
      
      if (memdata[0] == 0 && memdata[1] == 0 && memdata[2] == 0 && memdata[3] == 0)
      {
         if (fifofile)
            fread(testbuffer, 1, 4, fifofile);
         
         HW_FifoWrite(&writefifo, testbuffer, 4);
         
      }
	}
}

int HW_ProcessBlock(FTDIDevice* dev, uint8_t *buffer, int length)
{
	static unsigned int samplesize = 0;
	static unsigned char sampledata[13];
	
	if (samplesize && (samplesize+length >= 13))
	{
		unsigned int readlen = 13-samplesize;
		memcpy(sampledata+samplesize, buffer, readlen);
		
		HW_ProcessSample(dev, sampledata);
		
		length -= readlen;
		samplesize = 0;
		buffer += readlen;
	}
	
	while(length >= 13)
	{
		HW_ProcessSample(dev, buffer);
		
		buffer += 13;
		length -= 13;
	}
	
	if (length)
	{
		memcpy(sampledata+samplesize, buffer, length);
		samplesize += length;
	}
}

void HW_CaptureBegin(HWCapture* capture, FILE* outputFile, FTDIDevice* dev)
{
	int err;

	HW_ConfigInit(&configctx);
   HW_BufferInit(&writefifo, 16);

	capture->outputFile = outputFile;
	capture->compressedsize = 0;
   capture->done = 0;
   capture->dev = dev;

	if (outputFile == 0)
		return;

	pthread_mutex_init(&capture->mutex, 0);
	capture->running = 1;

	err = pthread_create(&capture->thread, NULL, HW_CaptureThread, capture);

	if (err != 0)
	{
		perror("error starting capture thread");
		exit(1);
	}
}

unsigned int HW_CaptureTryStop(HWCapture* capture)
{
   capture->running = 0;
   return capture->done;
}



void HW_CaptureFinish(HWCapture* capture)
{
	int err;

	capture->running = 0;
	err = pthread_join(capture->thread, 0);
	
	if (err != 0)
	{
		perror("error stopping capture thread");
		exit(1);
	}
	
	pthread_mutex_destroy(&capture->mutex);
}
	


void HW_CaptureDataBlock(HWCapture* capture, uint8_t* buffer, unsigned int length)
{
	HWBuffer* node = 0;
	unsigned int pos = 0;
	
	if (capture->running == 0)
		return;

	pthread_mutex_lock(&capture->mutex);
	node = HW_BufferChainGetLast(&capture->chain);
	

	if (node == 0)
		node = HW_BufferChainAppendNew(&capture->chain);
	

	while(pos < length)
	{
		if (node == 0)
		{
			perror("error adding hw buffer");
			exit(1);
		}
		
		pos += HW_BufferFill(node, buffer + pos, length - pos);
					
		if (pos < length)
			node = HW_BufferChainAppendNew(&capture->chain);
	}
	
	pthread_mutex_unlock(&capture->mutex);
}

static void* HW_CaptureThread(void* arg)
{
	HWCapture* capture = (HWCapture*)arg;
	z_stream stream;
	unsigned char* zbuffer = 0;
	int result;
	unsigned int nodecount = 0;
	unsigned int have = 0;
	

   fifofile = fopen("fifo.bin", "rb");

   {
      unsigned char testbuffer[0x100];
      
      if (fifofile)
         fread(testbuffer, 1, 0x100, fifofile);
   
      HW_FifoWrite(&writefifo, testbuffer, 0x100);
   }
   
   
	if (capture->outputFile == 0)
	{
		fprintf(stderr, "Error no output file provided\n");
		exit(1);
	}

	zbuffer = malloc(ZCHUNK);
	if (zbuffer == 0)
	{
		fprintf(stderr, "Error allocating zbuffer\n");
		exit(1);
	}

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    result = deflateInit(&stream, ZCOMPLEVEL);
    if (result != Z_OK)
	{
		fprintf(stderr, "Error initializing ZLIB\n");
		exit(1);
	}
	
	while(capture->running)
	{
		
		HWBuffer* node = 0;
		unsigned int available;
		unsigned int capacity;
		unsigned char* buffer;
      unsigned int buffersize;
      unsigned int bufferpos;
   
		pthread_mutex_lock(&capture->mutex);
		
		node = HW_BufferChainGetFirst(&capture->chain);
		
		if (node)
		{
			available = node->capacity - node->size;
         buffersize = node->size;
         bufferpos = node->pos;
			buffer = node->buffer;
			capacity = node->capacity;
         
         node->pos = node->size;
		}
		
		pthread_mutex_unlock(&capture->mutex);
      
      if (node && (buffersize - bufferpos > 0))
      {
         HW_ProcessBlock(capture->dev, buffer + bufferpos, buffersize - bufferpos);
         
         if (writefifo.size)
         {
            int err = HW_ConfigDevice(capture->dev, &writefifo, 1);
            
            if (err == 0) {
               HW_BufferClear(&writefifo);
            } else {
               printf("\nError write submit\n");
            }
         }
      }
		
		if (node == 0 || available != 0)
		{
			mssleep(1);
			continue;
		}
      
		stream.avail_in = capacity;
		stream.next_in = buffer;
		
		do
		{
			stream.avail_out = ZCHUNK;
			stream.next_out = zbuffer;
			
			result = deflate(&stream, Z_NO_FLUSH);
			
			if (result == Z_STREAM_ERROR)
			{
				fprintf(stderr, "Error compressing stream\n");
				exit(1);
			}
			
			have = ZCHUNK - stream.avail_out;
			capture->compressedsize += have;
			if (capture->outputFile) 
			{
				if (fwrite(zbuffer, have, 1, capture->outputFile) != 1) 
				{
					deflateEnd(&stream);
					fprintf(stderr, "Write error\n");
					exit(1);
				}			
			}
		} while(stream.avail_out == 0);

		
		nodecount = 0;
		pthread_mutex_lock(&capture->mutex);
		HW_BufferChainDestroyFirst(&capture->chain);
		node = HW_BufferChainGetFirst(&capture->chain);
		while(1)
		{			
			if (node == 0)
				break;
			node = node->next;
			nodecount++;
		}
		pthread_mutex_unlock(&capture->mutex);
		if (nodecount > 1)
			fprintf(stdout, "WARNING: %d hwbuffers still remaining\n", nodecount);
	}
	
	// Write last remaining data in the buffers
	while(1)
	{		
		HWBuffer* node = 0;
		unsigned int available;
		unsigned int capacity;
		unsigned char* buffer;
				
		pthread_mutex_lock(&capture->mutex);
		
		node = HW_BufferChainGetFirst(&capture->chain);
		
		if (node)
		{
			available = node->capacity - node->size;
			buffer = node->buffer;
			capacity = node->capacity;
		}
		
		pthread_mutex_unlock(&capture->mutex);
		
		if (node == 0)
			break;
		
		if (available == capacity)
			break;
		
		
		stream.avail_in = capacity - available;
		stream.next_in = buffer;
		
		do
		{
			stream.avail_out = ZCHUNK;
			stream.next_out = zbuffer;
			
			result = deflate(&stream, Z_NO_FLUSH);
			
			if (result == Z_STREAM_ERROR)
			{
				fprintf(stderr, "Error compressing stream\n");
				exit(1);
			}
			
			have = ZCHUNK - stream.avail_out;
			capture->compressedsize += have;
			if (capture->outputFile) 
			{
				if (fwrite(zbuffer, have, 1, capture->outputFile) != 1)
				{
					deflateEnd(&stream);
					fprintf(stderr, "Write error\n");
					exit(1);
				}
			}
			
		} while(stream.avail_out == 0);
				
		pthread_mutex_lock(&capture->mutex);
		HW_BufferChainDestroyFirst(&capture->chain);
		pthread_mutex_unlock(&capture->mutex);		
	}
	
	stream.avail_in = 0;
	stream.next_in = NULL;
	
	do
	{
		stream.avail_out = ZCHUNK;
		stream.next_out = zbuffer;
		
		result = deflate(&stream, Z_FINISH);
		
		if (result == Z_STREAM_ERROR)
		{
			fprintf(stderr, "Error compressing stream\n");
			exit(1);
		}
		
		have = ZCHUNK - stream.avail_out;
		capture->compressedsize += have;
		if (capture->outputFile)
		{
			if (fwrite(zbuffer, have, 1, capture->outputFile) != 1)
			{
				deflateEnd(&stream);
				fprintf(stderr, "Write error\n");
				exit(1);
			}		
		}
	} while(stream.avail_out == 0);
	
	
	deflateEnd(&stream);
	free(zbuffer);
   
   capture->done = 1;
   
   printf("\n\nThread stopped.\n\n");

	return 0;
}

