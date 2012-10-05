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
#include <pthread.h>
#include <zlib.h>
#include "utils.h"
#include "hw_command.h"

static void HW_CommandLock(HWCommand* command);
static void HW_CommandUnlock(HWCommand* command);
static int HW_CommandTryLock(HWCommand* command);

static void HW_CommandAddBegin(HWCommand* command, unsigned int cmdid);
static void HW_CommandAddBuffer(HWCommand* command, unsigned char* buffer, unsigned int size);
static void HW_CommandAddFile(HWCommand* command, const char* filename);
static void HW_CommandAddLong(HWCommand* command, unsigned int data);
static void HW_CommandAddEnd(HWCommand* command);
static void HW_CommandAdd(HWCommand* command, unsigned int cmdid, unsigned char* buffer, unsigned int size);

static void* HW_CommandParseThread(void* arg);
static void HW_CommandBegin(HWCommand* command);
static void HW_CommandStop(HWCommand* command);

void HW_CommandInit(HWCommand* command, int enabled)
{
	unsigned int i;

	command->curcmdactive = 0;
	command->curcmdpos = 0;
	command->curcmdsize = 0;
	command->running = 0;
	
	HW_BufferInit(&command->buffer, 16);
	HW_MemoryPoolInit(&command->mempool, 1024 * 1024);
	pthread_mutex_init(&command->mutex, 0);
	
	for(i=0; i<BREAKPOINTMAX; i++)
	{
		command->breakpoints[i].orgins = 0;
		command->breakpoints[i].address = 0;
		command->breakpoints[i].used = 0;
	}
	
	if (enabled)
		HW_CommandBegin(command);
}


static void HW_CommandBegin(HWCommand* command)
{
	int err;
	
	command->running = 1;
	err = pthread_create(&command->thread, NULL, HW_CommandParseThread, command);

	if (err != 0)
	{
		perror("error starting command thread");
		exit(1);
	}
}

static void HW_CommandStop(HWCommand* command)
{
	int err;

	command->running = 0;
	err = pthread_join(command->thread, 0);
	
	if (err != 0)
	{
		perror("error stopping command thread");
		exit(1);
	}
}

void HW_CommandDestroy(HWCommand* command)
{
	// It's too messy to cleanly stop the command thread, so just leave it running with the resources it needs intact. 
	// It'll be cleaned up by the OS anyway when the process quits.
#ifdef CLEANEXIT
	if (command->running)
		HW_CommandStop(command);
		
	pthread_mutex_destroy(&command->mutex);
	HW_BufferDestroy(&command->buffer);
	HW_MemoryPoolDestroy(&command->mempool);
#endif
}

static void HW_CommandLock(HWCommand* command)
{
	pthread_mutex_lock(&command->mutex);
}

static void HW_CommandUnlock(HWCommand* command)
{
	pthread_mutex_unlock(&command->mutex);
}

static int HW_CommandTryLock(HWCommand* command)
{
	return pthread_mutex_trylock(&command->mutex);
}

/**
	Reads a command from the HWCommand context, and appends it to the HWBuffer.
	
	Returns 1 if a command was read, 0 otherwise.
 */
unsigned int HW_CommandRead(HWCommand* command, HWBuffer* buffer)
{
	unsigned int result = 0;
	unsigned char dummybuffer[4] = {0, 0, 0, 0};
	unsigned int alignedsize, restsize;
	

	if (0 == HW_CommandTryLock(command))
	{
		// move first command from internal context to buffer
		if (command->buffer.size >= 8)
		{
			unsigned char* cmdbuf = command->buffer.buffer;
			unsigned int cmdid = (cmdbuf[0]<<0) | (cmdbuf[1]<<8) | (cmdbuf[2]<<16) | (cmdbuf[3]<<24);
			unsigned int cmdsize = (cmdbuf[4]<<0) | (cmdbuf[5]<<8) | (cmdbuf[6]<<16) | (cmdbuf[7]<<24);
			unsigned int totalsize = 8+cmdsize;
			
			if (command->buffer.size >= totalsize)
			{
				HW_BufferAppend(buffer, cmdbuf, totalsize);
				
				alignedsize = (cmdsize+3) & (~3);
				restsize = alignedsize - cmdsize;
				if (restsize)
					HW_BufferAppend(buffer, dummybuffer, restsize);
					
				HW_BufferRemoveFront(&command->buffer, totalsize);
				result = 1;
			}
		}
		
		HW_CommandUnlock(command);
	}
	
	return result;
}

static void HW_CommandAddBegin(HWCommand* command, unsigned int cmdid)
{
	unsigned char buffer[4];
	
	if (command->curcmdactive)
		return;
		
	HW_CommandLock(command);
	
	command->curcmdpos = command->buffer.size;
	command->curcmdsize = 0;
	
	buffer[0] = cmdid>>0;
	buffer[1] = cmdid>>8;
	buffer[2] = cmdid>>16;
	buffer[3] = cmdid>>24;
	HW_BufferAppend(&command->buffer, buffer, 4);
	
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;
	HW_BufferAppend(&command->buffer, buffer, 4);
	
	command->curcmdactive = 1;
}

static void HW_CommandAddBuffer(HWCommand* command, unsigned char* buffer, unsigned int size)
{
	if (!command->curcmdactive)
		return;

	command->curcmdsize += size;
	
	HW_BufferAppend(&command->buffer, buffer, size);	
}

static void HW_CommandAddLong(HWCommand* command, unsigned int data)
{
	unsigned char buffer[4];
	
	buffer[0] = data>>0;
	buffer[1] = data>>8;
	buffer[2] = data>>16;
	buffer[3] = data>>24;
	
	HW_CommandAddBuffer(command, buffer, 4);
}

static void HW_CommandAddFile(HWCommand* command, const char* filename)
{
	FILE* f = 0;
	unsigned char buffer[4096];
	unsigned int size;
	
	f = fopen(filename, "rb");
	
	if (f == 0)
	{
		fprintf(stdout, "Error, could not open file %s\n", filename);
		goto clean;
	}
	
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	while(size)
	{
		unsigned int max = sizeof(buffer);
		
		if (max > size)
			max = size;
		
		if (1 != fread(buffer, max, 1, f))
		{
			printf("Error, could not read file %s\n", filename);
			goto clean;
		}
		
		HW_CommandAddBuffer(command, buffer, max);
		
		size -= max;
	}
	
clean:
	if (f)
		fclose(f);
}

static void HW_CommandAddEnd(HWCommand* command)
{
	unsigned char buffer[4];

	if (!command->curcmdactive)
		return;
		
	buffer[0] = command->curcmdsize >> 0;
	buffer[1] = command->curcmdsize >> 8;
	buffer[2] = command->curcmdsize >> 16;
	buffer[3] = command->curcmdsize >> 24;
	
	memcpy(command->buffer.buffer + command->curcmdpos + 4, buffer, 4);
	
	HW_CommandUnlock(command);
	
	command->curcmdactive = 0;
}

static void HW_CommandAdd(HWCommand* command, unsigned int cmdid, unsigned char* buffer, unsigned int size)
{
	HW_CommandAddBegin(command, cmdid);
	HW_CommandAddBuffer(command, buffer, size);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteByte(HWCommand* command, unsigned int address, unsigned int data)
{
	HW_CommandAddBegin(command, CMD_WRITEBYTE);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, data);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteShort(HWCommand* command, unsigned int address, unsigned int data)
{
	HW_CommandAddBegin(command, CMD_WRITESHORT);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, data);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteLong(HWCommand* command, unsigned int address, unsigned int data)
{
	HW_CommandAddBegin(command, CMD_WRITELONG);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, data);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteMem(HWCommand* command, unsigned int address, HWCommandBuffer* cmdbuf)
{
	if (cmdbuf == 0)
		return;
	
	HW_CommandAddBegin(command, CMD_WRITEMEM);
	HW_CommandAddLong(command, address);
	HW_CommandAddBuffer(command, cmdbuf->data, cmdbuf->size);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteScatterTable(HWCommand* command, unsigned int scatteraddress, unsigned int targetaddress, unsigned int entries)
{
	HW_CommandAddBegin(command, CMD_WRITESCATTER);
	HW_CommandAddLong(command, scatteraddress);
	HW_CommandAddLong(command, targetaddress);
	HW_CommandAddLong(command, entries);
	HW_CommandAddEnd(command);
}

void HW_CommandWriteFile(HWCommand* command, unsigned int address, HWCommandString* filepath)
{
	if (filepath == 0)
		return;

	HW_CommandAddBegin(command, CMD_WRITEMEM);
	HW_CommandAddLong(command, address);
	HW_CommandAddFile(command, filepath->value);
	HW_CommandAddEnd(command);
}

void HW_CommandReadByte(HWCommand* command, unsigned int address)
{
	HW_CommandAddBegin(command, CMD_READBYTE);
	HW_CommandAddLong(command, address);
	HW_CommandAddEnd(command);
}

void HW_CommandReadShort(HWCommand* command, unsigned int address)
{
	HW_CommandAddBegin(command, CMD_READSHORT);
	HW_CommandAddLong(command, address);
	HW_CommandAddEnd(command);
}

void HW_CommandReadLong(HWCommand* command, unsigned int address)
{
	HW_CommandAddBegin(command, CMD_READLONG);
	HW_CommandAddLong(command, address);
	HW_CommandAddEnd(command);
}

void HW_CommandReadMem(HWCommand* command, unsigned int address, unsigned int size)
{
	HW_CommandAddBegin(command, CMD_READMEM);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, size);
	HW_CommandAddEnd(command);
}

void HW_CommandMemset(HWCommand* command, unsigned int address, unsigned int value, unsigned int size)
{
	HW_CommandAddBegin(command, CMD_MEMSET);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, value);
	HW_CommandAddLong(command, size);	
	HW_CommandAddEnd(command);
}


void HW_CommandSetBreakpoint(HWCommand* command, unsigned int address)
{
	HW_CommandAddBegin(command, CMD_SETBREAKPOINT);
	HW_CommandAddLong(command, address);
	HW_CommandAddEnd(command);
}

void HW_CommandUnsetBreakpoint(HWCommand* command, unsigned int address, unsigned int orgins)
{
	HW_CommandAddBegin(command, CMD_UNSETBREAKPOINT);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, orgins);
	HW_CommandAddEnd(command);
}

void HW_CommandContinue(HWCommand* command)
{
	HW_CommandAddBegin(command, CMD_CONTINUE);
	HW_CommandAddEnd(command);
}


void HW_CommandSetExceptionDataAbort(HWCommand* command, unsigned int value)
{
	HW_CommandAddBegin(command, CMD_SETEXCEPTION);
	HW_CommandAddLong(command, EXCEPTION_DATAABORT);
	HW_CommandAddLong(command, value);
	HW_CommandAddEnd(command);
}


void HW_CommandReadMemToFile(HWCommand* command, unsigned int address, unsigned int size, HWCommandString* filepath)
{
	unsigned char buffer[64];
	unsigned int pathsize = 62;
	if (filepath == 0)
		return;

	HW_CommandAddBegin(command, CMD_READMEMTOFILE);
	HW_CommandAddLong(command, address);
	HW_CommandAddLong(command, size);
	
	memset(buffer, 0, 64);
	if (filepath->length < pathsize)
		pathsize = filepath->length;
	memcpy(buffer, filepath->value, pathsize);
	HW_CommandAddBuffer(command, buffer, 64);
	HW_CommandAddEnd(command);
}

void HW_CommandPxi(HWCommand* command, unsigned int pxiid, unsigned int pxicmd, HWCommandType* items)
{
	unsigned int ptrcount = 0;
	unsigned int datacount = 0;
	unsigned int cmddatacount = 0;
	unsigned int cmdptrcount = 0;
	unsigned int ptrsize;
	unsigned int ptrindex = 0;

	unsigned int scatterentrycount;
	unsigned int scatteraddress;
	unsigned int targetaddress;
	
	unsigned int pxiptraddress = 0x20100000;
	unsigned int messagedata[128];
	unsigned int msgidx = 0;
	
	unsigned int i;
	
	cmddatacount = (pxicmd >> 6) & 0x3F;
	cmdptrcount = pxicmd & 0x3F;
	
	if (items)
	{
		HWCommandType* item = items;
		
		while(item != 0)
		{
			if (item->type == CMDTYPE_NUMBER)
			{
				if (datacount == cmddatacount)
				{
					if (0 == (ptrcount & 1))
					{
						ptrsize = item->number->data.value;
						
						if ( (ptrsize & 0xF) != 4 )
							printf("semantic error, pxi ptrsize 0x%08X should have tag 4.\n", ptrsize);
						if ( ((ptrsize>>4) & 0xF) != ptrindex )
							printf("semantic error, pxi ptrsize 0x%08X should have ptrindex %d.\n", ptrsize, ptrindex);
					}
					else
					{
						ptrindex++;
					}
					
					ptrcount++;
				}
				else
					datacount++;
			}
			else if (item->type == CMDTYPE_BUFFER)
			{
				if (datacount != cmddatacount)
				{
					printf("semantic error, pxi buffers should be last\n");
					return;
				}
			
				
				ptrcount+=2;
				ptrindex++;
			}
			
			item = item->next;
		}
		
		if (ptrcount != cmdptrcount || datacount != cmddatacount)
		{
			printf("semantic error, pxi command does not match data types\n");
			return;
		}

		datacount = 0;
		ptrcount = 0;
		item = items;
		ptrindex = 0;
		while(item != 0)
		{
			if (item->type == CMDTYPE_NUMBER)
			{
				messagedata[msgidx++] = item->number->data.value;
				if (datacount == cmddatacount)
				{
					if ((ptrcount&1) != 0)
						ptrindex++;
					ptrcount++;
				}
			}
			else if (item->type == CMDTYPE_BUFFER)
			{
				HWCommandBuffer* buffer = item->buffer->data;
				
				scatterentrycount = (buffer->size + 0xFFF) / 0x1000;
				pxiptraddress = (pxiptraddress + 0xFFF) & ~0xFFF;
				scatteraddress = pxiptraddress;
				pxiptraddress += scatterentrycount*0x4;
				pxiptraddress = (pxiptraddress + 0xFFF) & ~0xFFF;
				targetaddress = pxiptraddress;
				pxiptraddress += scatterentrycount*0x1000;
				
				HW_CommandWriteScatterTable(command, scatteraddress, targetaddress, scatterentrycount);
				HW_CommandWriteMem(command, targetaddress, buffer);
					
				messagedata[msgidx++] = (buffer->size << 8) | 4 | (ptrindex<<4);
				messagedata[msgidx++] = scatteraddress;
				
				ptrindex++;
			}
			
			item = item->next;
		}
	} 
	else
	{
		if (ptrcount != cmdptrcount || datacount != cmddatacount)
		{
			printf("semantic error, pxi command does not match data types\n");
			return;
		}
	}
	
	
	HW_CommandAddBegin(command, CMD_PXI);
	HW_CommandAddLong(command, pxiid);
	HW_CommandAddLong(command, pxicmd);
	
	for(i=0; i<msgidx; i++)
		HW_CommandAddLong(command, messagedata[i]);
		
	HW_CommandAddEnd(command);	
}

void HW_CommandProcessResponse(HWCommand* command, unsigned int cmdid, unsigned char* buffer, unsigned int buffersize)
{
	unsigned int bufferpos = 0;
	unsigned int address;
	unsigned int value;
	unsigned int i;
	
	if (cmdid == CMD_READBYTE)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		printf("> read8 [0x%08X] = 0x%02X\n", address, value);
	} 
	else if (cmdid == CMD_READSHORT)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		printf("> read16 [0x%08X] = 0x%04X\n", address, value);
	}
	else if (cmdid == CMD_READLONG)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		printf("> read32 [0x%08X] = 0x%08X\n", address, value);
	}
	else if (cmdid == CMD_READMEM)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		
		printf("> readmem 0x%08X:\n", address);
		hexdump(buffer+bufferpos, buffersize-bufferpos);
	}
	else if (cmdid == CMD_READMEMTOFILE)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		
		printf(">\b");
	} 	
	else if (cmdid == CMD_WRITEBYTE)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> write8 [0x%08X] = 0x%02X\n", address, value);
		printf(">\b");
	} 	
	else if (cmdid == CMD_WRITESHORT)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> write16 [0x%08X] = 0x%04X\n", address, value);
		printf(">\b");
	} 	
	else if (cmdid == CMD_WRITELONG)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		value = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> write32 [0x%08X] = 0x%08X\n", address, value);
		printf(">\b");
	} 	
	else if (cmdid == CMD_WRITEMEM)
	{
		address = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> writemem [0x%08X]\n", address);
		printf(">\b");
	}
	else if (cmdid == CMD_WRITESCATTER)
	{
		unsigned int scatteraddress = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int targetaddress = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int entrycount = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> writescatter [0x%08X]=%08X +%d\n", scatteraddress, targetaddress, entrycount);
		printf(">\b");
	}
	else if (cmdid == CMD_PXI)
	{
		unsigned int pxiid = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int pxicmd = buffer_readle32(buffer, &bufferpos, buffersize);
		//printf("> pxi [%d] %08X\n", pxiid, pxicmd);
		printf(">\b");
	}
	else if (cmdid == CMD_MEMSET)
	{
		unsigned int address = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int size = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int value = buffer_readle32(buffer, &bufferpos, buffersize);

		printf(">\b");
	}
	else if (cmdid == CMD_SETEXCEPTION)
	{
		unsigned int type = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int value = buffer_readle32(buffer, &bufferpos, buffersize);

		printf(">\b");
	}
	else if (cmdid == CMD_PXIREPLY)
	{
		unsigned int pxiid = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int pxicmd = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int wordcount = (pxicmd&0x3F) + ((pxicmd>>6)&0x3F);
		
		printf("> pxi [%d] %08X ", pxiid, pxicmd);
		for(i=0; i<wordcount; i++)
		{
			unsigned int pxidata = buffer_readle32(buffer, &bufferpos, buffersize);
			printf("%08X ", pxidata);
		}
		printf("\n");
	}
	else if (cmdid == CMD_SETBREAKPOINT)
	{
		unsigned int address = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int orgins = buffer_readle32(buffer, &bufferpos, buffersize);
	
		for(i=0; i<BREAKPOINTMAX; i++)
		{
			if (!command->breakpoints[i].used)
			{
				command->breakpoints[i].used = 1;
				command->breakpoints[i].address = address;
				command->breakpoints[i].orgins = orgins;
				
				break;
			}
		}		
		printf(">\b");
	}
	else if (cmdid == CMD_CONTINUE)
	{
		printf(">\b");
	}
	else if (cmdid == CMD_EXCEPTION)
	{
		unsigned int pc = buffer_readle32(buffer, &bufferpos, buffersize);
		unsigned int vector = buffer_readle32(buffer, &bufferpos, buffersize);
		int found = 0;
	
		for(i=0; i<BREAKPOINTMAX; i++)
		{
			if (command->breakpoints[i].used && (command->breakpoints[i].address & ~1) == pc)
			{
				command->breakpoints[i].used = 0;
				printf("> Breakpoint hit at 0x%08X\n", pc);
				HW_CommandUnsetBreakpoint(command, command->breakpoints[i].address, command->breakpoints[i].orgins);
				found = 1;
				break;
			}
		}
		
		if (!found)
			printf("> Exception triggered at 0x%08X (%d)\n", pc, vector);
	}	
	else
	{
		printf("> Unknown command response 0x%02X received.\n", cmdid);
	}
}


static void* HW_CommandParseThread(void* arg)
{
	int oldstate;
	HWCommand* command = (HWCommand*)arg;
	
	while(command->running)
	{
		HW_CommandParse(command);
	}
}

