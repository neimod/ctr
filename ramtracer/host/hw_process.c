/*
 * hw_process.h - Processing of RAM tracing data.
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
#include "hw_config.h"
#include "utils.h"
#include "hw_process.h"
#include "hw_command.h"
#include "server.h"

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



void HW_ProcessInit(HWProcess* process, FTDIDevice* dev, int enabled) 
{
   unsigned int i;
   
   for(i=0; i<MAXFILES; i++)
   {
      process->filemap[i].used = 0;
      process->filemap[i].file = 0;
   }
   
   process->enabled = enabled;
   process->dev = dev;
   process->writefifocapacity = 256;
   HW_BufferInit(&process->writefifo, 16);
   HW_BufferInit(&process->readfifo, 16);
   HW_BufferInit(&process->databuffer, 16);   
   HW_ConfigInit(&process->config);  

   HW_CommandInit(&process->command, enabled);
   
   if (ServerIsEnabled())
      ServerBegin(&process->server);
}


void HW_ProcessDestroy(HWProcess* process) 
{
   if (ServerIsEnabled())
      ServerFinish(&process->server);

   HW_CommandDestroy(&process->command);

   HW_BufferDestroy(&process->writefifo);
   HW_BufferDestroy(&process->config);
}

int verboseprocessing = 0;
void HW_ProcessServicePrint(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int i;
   
   if (buffer[0] == '>' && buffer[1] == ' ' && buffer[2] == 'E')
	verboseprocessing = 1;
   
   for(i=0; i<buffersize; i++)
      printf("%c", buffer[i]);
}

void HW_ProcessServiceFopen(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   char fname[256];
   FILE* f = 0;
   unsigned int index = ~0;
   unsigned int i;
      
   printf("@");
   if (buffersize >= 256)
   {
      printf("ERROR fopen: fname size too big\n");
      return;
   }
   
   for(i=0; i<MAXFILES; i++)
   {
      if (process->filemap[i].used == 0)
         index = i;
   }
   
   if (index == ~0)
   {
      printf("ERROR fopen: max files in use\n");
      return;
   }
   
   memset(fname, 0, sizeof(fname));
   memcpy(fname, buffer, buffersize);
   f = fopen(fname, (command==0x02)?"rb":"wb");

   if (f == 0)
   {
      printf("ERROR fopen: could not open file\n");
	  return;
   }
   
   process->filemap[index].used = 1;
   process->filemap[index].file = f;
     
   HW_BufferAppend(&process->writefifo, &index, 4);
}

void HW_ProcessServiceFwrite(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileid = 0;
   unsigned int bufferpos = 0;
   unsigned int datasize;
   int res;
      
   printf(".");
   if (buffersize < 4)
   {
      printf("ERROR fwrite: no file id\n");
      return;
   }
   
   fileid = buffer_readle32(buffer, &bufferpos, buffersize);   
   if (process->filemap[fileid].used == 0)
   {
      printf("ERROR fwrite: invalid file id\n");
	  return;
   }
      
   datasize = buffersize - bufferpos;
   res = fwrite(buffer_readdata(buffer, &bufferpos, buffersize, datasize), datasize, 1, process->filemap[fileid].file);
   
   if (res != 1)
   {
      printf("ERROR fwrite: write error\n");
   }
}

void HW_ProcessServiceFread(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileid = 0;
   unsigned int bufferpos = 0;
   unsigned int datasize;
   int readsize;
   int readsizealigned;
   int readsizerest;
   unsigned char dummybuffer[4] = {0, 0, 0, 0};
   
   printf("*");
   if (buffersize < 8)
   {
      printf("ERROR fread: no file id or size\n");
      return;
   }
   
   fileid = buffer_readle32(buffer, &bufferpos, buffersize);
   if (process->filemap[fileid].used == 0)
   {
      printf("ERROR fread: invalid file id\n");
	  return;
   }

   datasize = buffer_readle32(buffer, &bufferpos, buffersize);
   
   HW_BufferClear(&process->databuffer);
   HW_BufferReserve(&process->databuffer, datasize);
   
   readsize = fread(process->databuffer.buffer, 1, datasize, process->filemap[fileid].file);
   if (readsize < 0)
   {
      printf("ERROR fread: read error\n");
      return;
   }

   HW_BufferAppend(&process->writefifo, &readsize, 4);
   HW_BufferAppend(&process->writefifo, process->databuffer.buffer, readsize);
   
   readsizealigned = (readsize+3) & (~3);
   if (readsizealigned != readsize)
      HW_BufferAppend(&process->writefifo, dummybuffer, readsizealigned-readsize);
   HW_BufferClear(&process->databuffer);
}

void HW_ProcessServiceFclose(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileid = 0;
   unsigned int bufferpos = 0;
   
   printf("#");
   if (buffersize < 4)
   {
      printf("ERROR fclose: no file id\n");
      return;
   }
   
   fileid = buffer_readle32(buffer, &bufferpos, buffersize);
   if (process->filemap[fileid].used == 0)
   {
      printf("ERROR fclose: invalid file id\n");
	  return;
   }
   
   fclose(process->filemap[fileid].file);
   process->filemap[fileid].file = 0;
   process->filemap[fileid].used = 0;
}

void HW_ProcessServiceFseek(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileid = 0;
   unsigned int offset = 0;
   unsigned int bufferpos = 0;
   
   printf(":");
   if (buffersize < 8)
   {
      printf("ERROR fseek: no file id or offset\n");
      return;
   }
   
   fileid = buffer_readle32(buffer, &bufferpos, buffersize);
   offset = buffer_readle32(buffer, &bufferpos, buffersize);
   if (process->filemap[fileid].used == 0)
   {
      printf("ERROR fseek: invalid file id\n");
	  return;
   }
   
   fseek(process->filemap[fileid].file, offset, SEEK_SET);
}

void HW_ProcessServiceFcopy(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileiddst = 0;
   unsigned int fileidsrc = 0;
   unsigned int bufferpos = 0;
   unsigned char copybuffer[4096];
   unsigned int copysize;
   unsigned int maxsize;
   FILE* fdst;
   FILE* fsrc;
   
   printf("+");
   if (buffersize < 8)
   {
      printf("ERROR fseek: no src or dst file id\n");
      return;
   }
   
   fileiddst = buffer_readle32(buffer, &bufferpos, buffersize);
   fileidsrc = buffer_readle32(buffer, &bufferpos, buffersize);
   if (process->filemap[fileiddst].used == 0)
   {
      printf("ERROR fseek: invalid dst file id\n");
	  return;
   }

   if (process->filemap[fileidsrc].used == 0)
   {
      printf("ERROR fseek: invalid src file id\n");
	  return;
   }
   
   fsrc = process->filemap[fileidsrc].file;
   fdst = process->filemap[fileiddst].file;
   
   fseek(fsrc, 0, SEEK_END);
   copysize = ftell(fsrc);
   fseek(fsrc, 0, SEEK_SET);
   fseek(fdst, 0, SEEK_SET);
   
   while(copysize)
   {
      maxsize = sizeof(copybuffer);
	  if (maxsize > copysize)
	    maxsize = copysize;
		
	  fread(copybuffer, 1, maxsize, fsrc);
	  fwrite(copybuffer, 1, maxsize, fdst);
	  copysize -= maxsize;
   }
   
   fseek(fsrc, 0, SEEK_SET);
   fseek(fdst, 0, SEEK_SET);   
}

void HW_ProcessServiceFsize(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned int fileid = 0;
   unsigned int bufferpos = 0;
   FILE* file = 0;
   unsigned int filesize = 0;
   
   printf("$");
   if (buffersize < 4)
   {
      printf("ERROR fsize: no file id\n");
      return;
   }
   
   fileid = buffer_readle32(buffer, &bufferpos, buffersize);
   if (process->filemap[fileid].used == 0)
   {
      printf("ERROR fsize: invalid file id\n");
	  return;
   }
   
   file = process->filemap[fileid].file;
   fseek(file, 0, SEEK_END);
   filesize = ftell(file);
   fseek(file, 0, SEEK_SET);
   
   HW_BufferAppend(&process->writefifo, &filesize, 4);
}

void HW_ProcessServiceCommandRequest(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   unsigned char idle[8] = { CMD_IDLE, 0, 0, 0, 0, 0, 0, 0};
   
   if (0 == HW_CommandRead(&process->command, &process->writefifo))
   {
      // No command in queue, so try the server
      if (0 == ServerRead(&process->server, &process->writefifo))
	  {	  
		  // No commands in queue or server, so send the idle command
		  HW_BufferAppend(&process->writefifo, idle, 8);
	  }
   }
}



void HW_ProcessServiceCommandResponse(HWProcess* process, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
   if (buffersize >= 4)
   {
      command = (buffer[0]<<0) | (buffer[1]<<8) | (buffer[2]<<16) | (buffer[3]<<24);
	  
	  if (command >= SERVER_MIN_CMD_ID)
	    ServerWrite(&process->server, command, buffer+4, buffersize-4);
	  else
		HW_CommandProcessResponse(&process->command, command, buffer+4, buffersize-4);
   }
}


void HW_Process(HWProcess* process, unsigned char* buffer, unsigned int buffersize) 
{
   unsigned int maxsize;
   

   if (process->enabled == 0)
      return;
   
   HW_ProcessBlock(process, buffer, buffersize);
   
   
   while(1) 
   {
      if (process->readfifo.size >= 5)
      {
         unsigned char* readfifobuffer = process->readfifo.buffer;
         unsigned int readfifopos = 0;
         unsigned int readfifosize = process->readfifo.size;

         unsigned int command = buffer_readbyte(readfifobuffer, &readfifopos, readfifosize);
         unsigned int commandsize = buffer_readle32(readfifobuffer, &readfifopos, readfifosize);
         unsigned int i;

         if (process->readfifo.size >= 5 + commandsize)
         {
            if (command == 0x01)
               HW_ProcessServicePrint(process, command, readfifobuffer+readfifopos, commandsize);
            else if (command == 0x02 || command == 0x03)
               HW_ProcessServiceFopen(process, command, readfifobuffer+readfifopos, commandsize);               
            else if (command == 0x04)
               HW_ProcessServiceFwrite(process, command, readfifobuffer+readfifopos, commandsize);               
            else if (command == 0x05)
               HW_ProcessServiceFread(process, command, readfifobuffer+readfifopos, commandsize);               
            else if (command == 0x06)
               HW_ProcessServiceFclose(process, command, readfifobuffer+readfifopos, commandsize);           
            else if (command == 0x07)
               HW_ProcessServiceFsize(process, command, readfifobuffer+readfifopos, commandsize);
            else if (command == 0x08)
               HW_ProcessServiceCommandRequest(process, command, readfifobuffer+readfifopos, commandsize);
            else if (command == 0x09)
               HW_ProcessServiceCommandResponse(process, command, readfifobuffer+readfifopos, commandsize);
            else if (command == 0x0A)
               HW_ProcessServiceFseek(process, command, readfifobuffer+readfifopos, commandsize);
            else if (command == 0x0B)
               HW_ProcessServiceFcopy(process, command, readfifobuffer+readfifopos, commandsize);
			   
            HW_BufferRemoveFront(&process->readfifo, 5 + commandsize);
         }
         else 
         {
            break;
         }
      }
      else 
      {
         break;
      }

   }
   
   maxsize = process->writefifo.size;
   if (maxsize > process->writefifocapacity)
      maxsize = process->writefifocapacity;
   
   if (maxsize)
   {
      HW_FifoWrite(&process->config, process->writefifo.buffer, maxsize);
      process->writefifocapacity -= maxsize;
      HW_BufferRemoveFront(&process->writefifo, maxsize);
   }
   
   if (process->config.size)
   {
      int err = HW_ConfigDevice(process->dev, &process->config, 1);
      
      if (err == 0) {
         HW_BufferClear(&process->config);
      } else {
         printf("\nError write submit\n");
      }
   }   
}


int HW_ProcessSample(HWProcess* process, uint8_t* sampledata)
{
   unsigned int header;
	unsigned int address;
	static unsigned int fifocount = 0;
   unsigned int i;
   unsigned char resynctoken[16] = {0x11, 0x11, 0x11, 0x11, 0x66, 0x66, 0x66, 0x66, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33};
   unsigned char magictoken[3] = {0x33, 0xAB, 0xB0};

 
	header = sampledata[0];
	address = sampledata[1] | (sampledata[2]<<8) | (sampledata[3]<<16);
	
   
   if ( (header != 1) && (address == 0x7FFFE8))
   {
      unsigned char memdata[8];
      unsigned int mask = sampledata[12];
      
      memcpy(memdata, sampledata+4, 8);
      fix_data_order(&mask, memdata);
      
	  // Communication interrupt received, clear read and write fifo, and send the resynchronization token
      if (!memcmp(memdata+1, magictoken, 3) && memdata[0] <= 4)
      {
		 HW_BufferClear(&process->readfifo);
		 HW_BufferClear(&process->writefifo);
		 HW_BufferAppend(&process->writefifo, resynctoken, 16);
      }      
   }
   else if ( (header != 1) && (address == 0x7FFFE4))
   {
      unsigned char memdata[8];
      unsigned int mask = sampledata[12];
      
      memcpy(memdata, sampledata+4, 8);
      fix_data_order(&mask, memdata);
	  
      if ( (mask & 0x0F) == 0 )
        memcpy(process->fifoincoming, memdata+0, 4);

      if ( (mask & 0xF0) == 0 )
      {
		 
         memcpy(process->fifoincoming+4, memdata+4, 4);
		 
         if (!memcmp(process->fifoincoming+1, magictoken, 3) && process->fifoincoming[0] <= 4)
         {
            HW_BufferAppend(&process->readfifo, process->fifoincoming+4, process->fifoincoming[0]);
         }
		 memset(process->fifoincoming, 0, 8);
      }
   }
	else if ( (header == 1) && (address == 0x7FFFE0))
	{
      unsigned char memdata[8];
      unsigned int mask = sampledata[12];
      static unsigned char testbuffer[4] = {1, 2, 3, 4};
      		
      memcpy(memdata, sampledata+4, 8);
      fix_data_order(&mask, memdata);
      
      if (memdata[0] == 0 && memdata[1] == 0 && memdata[2] == 0 && memdata[3] == 0)
      {
         process->writefifocapacity += 4;
      }
	}
}

int HW_ProcessBlock(HWProcess* process, uint8_t *buffer, int length)
{
	static unsigned int samplesize = 0;
	static unsigned char sampledata[13];
	
	if (samplesize && (samplesize+length >= 13))
	{
		unsigned int readlen = 13-samplesize;
		memcpy(sampledata+samplesize, buffer, readlen);
		
		HW_ProcessSample(process, sampledata);
		
		length -= readlen;
		samplesize = 0;
		buffer += readlen;
	}
	
	while(length >= 13)
	{
		HW_ProcessSample(process, buffer);
		
		buffer += 13;
		length -= 13;
	}
	
	if (length)
	{
		memcpy(sampledata+samplesize, buffer, length);
		samplesize += length;
	}
}      
