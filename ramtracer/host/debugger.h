/*
 * debugger.h - GDB remote debugging functionality.
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

#ifndef __DEBUGGER_H_
#define __DEBUGGER_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "hw_buffer.h"
#include "hw_command.h"

#define DEBUGGER_MIN_CMD_ID	0x1000
#define DEBUGGER_MAX_BUFFERSIZE 		(1 * 1024 * 1024)

/*
 * Debugger -- Worker structure for the debugger.
 */


typedef struct
{
	unsigned int running;
   unsigned int done;
	pthread_t listenthread;
	pthread_t receivethread;
	pthread_mutex_t mutex;
	struct sockaddr_storage remoteaddr;
	int remoteaddrlen;
	int remotesockfd;
	int remoteflush;
	char remoteIP[INET6_ADDRSTRLEN];
	
	unsigned int curcmdpos;
	unsigned int curcmdsize;
	unsigned int curcmdactive;

	int shutdownpipe[2];
	pthread_mutex_t receivemutex;
	HWBuffer receivebuffer;
	HWBuffer gdbrequestbuffer;
	HWBuffer gdbreplybuffer;
	HWBuffer cmdbuffer;
	HWBuffer fifobuffer;
} Debugger;

/*
 * Public functions
 */
int  DebuggerIsEnabled();
void DebuggerSetEnabled(int enabled);
void DebuggerBegin(Debugger* debugger);
void DebuggerFinish(Debugger* debugger);
unsigned int DebuggerRead(Debugger* debugger, HWBuffer* buffer);
void DebuggerWrite(Debugger* debugger, unsigned int cmdid, unsigned char* buffer, unsigned int buffersize) ;
void DebuggerSend(Debugger* debugger, unsigned char* buffer, unsigned int buffersize);
void DebuggerSendByte(Debugger* debugger, char byte);
void DebuggerSendHexByte(Debugger* debugger, unsigned char byte);
int DebuggerParsePacket(Debugger* debugger, HWBuffer* packet);

static void DebuggerCommandLock(Debugger* debugger);
static void DebuggerCommandUnlock(Debugger* debugger);
static int DebuggerCommandTryLock(Debugger* debugger);

static void DebuggerProcessPacket(Debugger* debugger, unsigned char* buffer, unsigned int size);

static void DebuggerReplyClear(Debugger* debugger);
static void DebuggerReplyBuffer(Debugger* debugger, unsigned char* buffer, unsigned int buffersize);
static void DebuggerReplyByte(Debugger* debugger, unsigned char byte);
static void DebuggerReplyHexByte(Debugger* debugger, unsigned char byte);
static void DebuggerReplyHexWord(Debugger* debugger, unsigned int word);
static void DebuggerReplySend(Debugger* debugger);
static void DebuggerReplyError(Debugger* debugger, unsigned int err);
static void DebuggerReplyOk(Debugger* debugger);
static void DebuggerReplyNop(Debugger* debugger);

#endif // __DEBUGGER_H_
