/*
 * hw_command.h - Input processing.
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

#ifndef __HW_COMMAND_H_
#define __HW_COMMAND_H_

#include <stdio.h>
#include <pthread.h>
#include "hw_buffer.h"
#include "hw_commandbuffer.h"
#include "hw_commandtype.h"
#include "hw_mempool.h"

#define CMD_WRITEBYTE		0x01
#define CMD_WRITESHORT		0x02
#define CMD_WRITELONG		0x03
#define CMD_WRITEMEM		0x04
#define CMD_READBYTE		0x05
#define CMD_READSHORT		0x06
#define CMD_READLONG		0x07
#define CMD_READMEM			0x08
#define CMD_WRITESCATTER	0x09
#define CMD_PXI				0x0A
#define CMD_IDLE			0x0B
#define CMD_PXIREPLY		0x0C
#define CMD_READMEMTOFILE	0x0D
#define CMD_MEMSET			0x0E
#define CMD_SETEXCEPTION	0x0F

#define CMD_SETBREAKPOINT	0x20
#define CMD_UNSETBREAKPOINT	0x21
#define CMD_CONTINUE		0x22
#define CMD_EXCEPTION		0x23

#define CMD_AESCTR			0x80
#define CMD_AESCONTROL		0x81
#define CMD_AESCBCDEC		0x82
#define CMD_AESCBCENC		0x83
#define CMD_AESCCMENC		0x84
#define CMD_AESCCMDEC		0x85


#define CMD_GDB_LASTSIG		0x1000
#define CMD_GDB_READREG		0x1001
#define CMD_GDB_READREGS	0x1002
#define CMD_GDB_WRITEREG	0x1003
#define CMD_GDB_WRITEREGS	0x1004
#define CMD_GDB_READMEM		0x1005
#define CMD_GDB_WRITEMEM	0x1006
#define CMD_GDB_CONTINUE	0x1007


#define CMD_AESCONTROL_SIZE		40

#define EXCEPTION_DATAABORT	0
/*
 * HWCommand -- Worker structure for input command processing. 
 */

#define BREAKPOINTMAX 1024

typedef struct
{
	unsigned int orgins;
	unsigned int address;
	unsigned int used;
} breakpoint;


typedef struct
{
   pthread_mutex_t mutex;
   pthread_t thread;
   HWBuffer buffer;
   unsigned int curcmdpos;
   unsigned int curcmdsize;
   unsigned int curcmdactive;
   HWMemoryPool mempool;
   int running;
   breakpoint breakpoints[BREAKPOINTMAX];
	
} HWCommand;




/*
 * Public functions
 */
void HW_CommandInit(HWCommand* command, int enabled);
void HW_CommandDestroy(HWCommand* command);
unsigned int HW_CommandRead(HWCommand* command, HWBuffer* buffer);
void HW_CommandWriteByte(HWCommand* command, unsigned int address, unsigned int data);
void HW_CommandWriteShort(HWCommand* command, unsigned int address, unsigned int data);
void HW_CommandWriteLong(HWCommand* command, unsigned int address, unsigned int data);
void HW_CommandWriteMem(HWCommand* command, unsigned int address, HWCommandBuffer* cmdbuf);
void HW_CommandWriteFile(HWCommand* command, unsigned int address, HWCommandString* filepath);
void HW_CommandWriteScatterTable(HWCommand* command, unsigned int scatteraddress, unsigned int targetaddress, unsigned int scatterentrycount);
void HW_CommandMemset(HWCommand* command, unsigned int address, unsigned int value, unsigned int size);
void HW_CommandSetExceptionDataAbort(HWCommand* command, unsigned int value);
void HW_CommandReadByte(HWCommand* command, unsigned int address);
void HW_CommandReadShort(HWCommand* command, unsigned int address);
void HW_CommandReadLong(HWCommand* command, unsigned int address);
void HW_CommandReadMem(HWCommand* command, unsigned int address, unsigned int size);

void HW_CommandBreakpoint(HWCommand* command, unsigned int address);
void HW_CommandContinue(HWCommand* command);

void HW_CommandParse(HWCommand* command);
void HW_CommandProcessResponse(HWCommand* command, unsigned int cmdid, unsigned char* buffer, unsigned int buffersize);

#endif // __HW_COMMAND_H_
