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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "debugger.h"
#include "utils.h"

#define PORT "8555" 
#define BACKLOG 10     // how many pending connections queue will hold

#define VEC_UND 1
#define VEC_SWI 2
#define VEC_PABT 3
#define VEC_DABT 4

typedef struct
{
	unsigned char* posend;
	unsigned char* posbegin;
	unsigned char* pos;
} buffercursor;

static int debuggerEnabled = 0;

// Private functions
static void* DebuggerListenThread(void* arg);
static void* DebuggerReceiveThread(void* arg);
static void *get_in_addr(struct sockaddr *sa);

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int sockaddrequal(struct sockaddr* a, struct sockaddr* b)
{
    if ((!a) || (!b))
        return 0;

    if (a->sa_family == AF_INET6)
    {
        if (b->sa_family != AF_INET6)
            return 0;
        
        return !memcmp (&((struct sockaddr_in6 *) a)->sin6_addr,
                        &((struct sockaddr_in6 *) b)->sin6_addr,
                        sizeof (struct in6_addr));
    }

    if (a->sa_family == AF_INET)
    {
        if (b->sa_family != AF_INET)
            return 0;
        
        return !memcmp (&((struct sockaddr_in *) a)->sin_addr,
                        &((struct sockaddr_in *) b)->sin_addr,
                        sizeof (struct in_addr));
    }

    return 0;
}


static int getbyte(buffercursor* cursor, unsigned char* byte)
{
	if (cursor->pos >= cursor->posend)
		return 0;
	
	*byte = *cursor->pos++;
	return 1;
}

static int asciitohex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 0xA;
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0' + 0;
    if ((ch >= 'A') && (ch <= 'F'))
        return ch - 'A' + 0xA;
		
    return -1;
}

static char hextoascii(unsigned char byte)
{
	byte &= 0xF;
	
	if (byte <= 9)
		return '0' + byte;
	else
		return 'a' + byte - 0xA;
}

static unsigned int gethexbyte(char* text)
{
	return (asciitohex(text[0]) << 4) + asciitohex(text[1]);
}

static unsigned int gethexword(char* text)
{
    int i;
    unsigned long r = 0;
	
    for (i = 3; i >= 0; i--)
        r = (r << 8) + gethexbyte(text + i * 2);

	return r;
}

int DebuggerIsEnabled()
{
	return debuggerEnabled;
}

void DebuggerSetEnabled(int enabled)
{
	debuggerEnabled = enabled;
}

void DebuggerBegin(Debugger* debugger)
{
	int err;
	
	debugger->curcmdactive = 0;
	debugger->running = 1;
	debugger->remoteaddrlen = 0;
	debugger->remotesockfd = 0;
	debugger->remoteflush = 0;
	
	// Setup a pipe that will receive the shutdown command
	if (pipe(debugger->shutdownpipe) == -1) 
	{
		perror("pipe");
		exit(1);
	}
	
	HW_BufferInit(&debugger->receivebuffer, DEBUGGER_MAX_BUFFERSIZE);
	HW_BufferInit(&debugger->cmdbuffer, DEBUGGER_MAX_BUFFERSIZE);
	HW_BufferInit(&debugger->gdbrequestbuffer, DEBUGGER_MAX_BUFFERSIZE);
	HW_BufferInit(&debugger->gdbreplybuffer, DEBUGGER_MAX_BUFFERSIZE);
	HW_BufferInit(&debugger->fifobuffer, DEBUGGER_MAX_BUFFERSIZE);
	pthread_mutex_init(&debugger->receivemutex, 0);
	pthread_mutex_init(&debugger->mutex, 0);

	err = pthread_create(&debugger->listenthread, NULL, DebuggerListenThread, debugger);

	if (err != 0)
	{
		perror("error starting debugger listener thread");
		exit(1);
	}

	err = pthread_create(&debugger->receivethread, NULL, DebuggerReceiveThread, debugger);

	if (err != 0)
	{
		perror("error starting debugger receive thread");
		exit(1);
	}
	
}



void DebuggerFinish(Debugger* debugger)
{
	int err;
	

	debugger->running = 0;
	write(debugger->shutdownpipe[1], "Y", 1);
	
	err = pthread_join(debugger->listenthread, 0);
	
	if (err != 0)
	{
		perror("error stopping listen thread");
		exit(1);
	}
	
	err = pthread_join(debugger->receivethread, 0);
	
	if (err != 0)
	{
		perror("error stopping receive thread");
		exit(1);
	}
	
	HW_BufferDestroy(&debugger->receivebuffer);	
	HW_BufferDestroy(&debugger->cmdbuffer);	
	HW_BufferDestroy(&debugger->gdbrequestbuffer);
	HW_BufferDestroy(&debugger->gdbreplybuffer);
	HW_BufferDestroy(&debugger->fifobuffer);
	pthread_mutex_destroy(&debugger->receivemutex);
	pthread_mutex_destroy(&debugger->mutex);
}
	

static void* DebuggerListenThread(void* arg)
{
	Debugger* debugger = (Debugger*)arg;
	struct addrinfo hints;
	struct addrinfo* servinfo;
	struct addrinfo* p;
	int rv;
	int listensockfd;
	int shutdownfd;
	int fdmax;
	int yes = 1;
	fd_set master;
	fd_set readfds;
	
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	
	shutdownfd = debugger->shutdownpipe[0];
	
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
	
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "error getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }	
	
	
	// loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((listensockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("error creating socket");
            continue;
        }

        if (setsockopt(listensockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("error setting socket option");
            exit(1);
        }

        if (bind(listensockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(listensockfd);
            perror("error binding socket");
            continue;
        }

        break;
    }
	
	if (p == NULL)  
	{
        fprintf(stderr, "failed to bind\n");
        exit(1);
    }
	
	freeaddrinfo(servinfo);
	
	if (listen(listensockfd, BACKLOG) == -1) 
	{
        perror("listen");
        exit(1);
    }
	
	
	FD_SET(listensockfd, &master);
	FD_SET(shutdownfd, &master);
	fprintf(stdout, "Debugger: Listening on port %s\n", PORT);
	
	fdmax = listensockfd;
	if (fdmax < shutdownfd)
		fdmax = shutdownfd;
	
	while(debugger->running)
	{
		struct timeval timeout;
		
		
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;			

		readfds = master;
		if (select(fdmax+1, &readfds, NULL, NULL, &timeout) == -1) 
		{
            perror("select");
            exit(1);
        }		
		
		if (FD_ISSET(shutdownfd, &readfds))
		{
			break;
		}
		
		if (FD_ISSET(listensockfd, &readfds))
		{
			char remoteIP[INET6_ADDRSTRLEN];
			struct sockaddr_storage remoteaddr;
			socklen_t addrlen = sizeof(remoteaddr);
			int sockfd = accept(listensockfd, (struct sockaddr *)&remoteaddr, &addrlen);

			if (sockfd == -1) 
			{
				perror("accept");
			} else {
				fprintf(stdout, "> New debugger connection from %s\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN));
				
				if (debugger->remotesockfd == 0 || sockaddrequal((struct sockaddr*)&remoteaddr, (struct sockaddr*)&debugger->remoteaddr))
				{
					if (debugger->remotesockfd != 0)
						close(debugger->remotesockfd);
					memcpy(debugger->remoteIP, remoteIP, INET6_ADDRSTRLEN);
					memcpy(&debugger->remoteaddr, &remoteaddr, addrlen);
					debugger->remoteaddrlen = addrlen;		
					debugger->remotesockfd = sockfd;
					debugger->remoteflush = 1;					
				}
			}
		}
	
	}
   
  

	return 0;
}

void DebuggerSendByte(Debugger* debugger, char byte)
{
	DebuggerSend(debugger, &byte, 1);
}

void DebuggerSendHexByte(Debugger* debugger, unsigned char byte)
{
	DebuggerSendByte(debugger, hextoascii(byte>>4));
	DebuggerSendByte(debugger, hextoascii(byte>>0));
}

void DebuggerSend(Debugger* debugger, unsigned char* buffer, unsigned int buffersize)
{
	int remotesockfd = debugger->remotesockfd;
	
	if (remotesockfd != 0)
	{
		while(buffersize)
		{
			int nbytes = send(remotesockfd, buffer, buffersize, 0);
			
			if (nbytes <= 0)
			{
				perror("send");
				break;
			}			
			
			buffersize -= nbytes;
			buffer += nbytes;
		}
	}
}



/*
void DebuggerWrite(Debugger* debugger, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
	unsigned char header[8];
	int remotesockfd = debugger->remotesockfd;
	
	
	header[0] = command>>0;
	header[1] = command>>8;
	header[2] = command>>16;
	header[3] = command>>24;
	header[4] = buffersize>>0;
	header[5] = buffersize>>8;
	header[6] = buffersize>>16;
	header[7] = buffersize>>24;

	DebuggerWriteBuffer(debugger, header, 8);
	DebuggerWriteBuffer(debugger, buffer, buffersize);
}
*/

/**
	Reads a command, and appends it to the HWBuffer.
	
	Returns 1 if a command was read, 0 otherwise.
 */
unsigned int DebuggerRead(Debugger* debugger, HWBuffer* buffer)
{
	unsigned int result = 0;
	unsigned char dummybuffer[4] = {0, 0, 0, 0};
	unsigned int alignedsize, restsize;
	

	if (0 == DebuggerCommandTryLock(debugger))
	{
		// move first command to buffer
		if (debugger->cmdbuffer.size >= 8)
		{
			unsigned char* cmdbuf = debugger->cmdbuffer.buffer;
			unsigned int cmdid = (cmdbuf[0]<<0) | (cmdbuf[1]<<8) | (cmdbuf[2]<<16) | (cmdbuf[3]<<24);
			unsigned int cmdsize = (cmdbuf[4]<<0) | (cmdbuf[5]<<8) | (cmdbuf[6]<<16) | (cmdbuf[7]<<24);
			unsigned int totalsize = 8+cmdsize;

			
			if (totalsize <= DEBUGGER_MAX_BUFFERSIZE && debugger->cmdbuffer.size >= totalsize)
			{
				if (cmdid >= DEBUGGER_MIN_CMD_ID)
				{					
					HW_BufferAppend(buffer, cmdbuf, totalsize);
					
					alignedsize = (cmdsize+3) & (~3);
					restsize = alignedsize - cmdsize;
					if (restsize)
						HW_BufferAppend(buffer, dummybuffer, restsize);
						
					result = 1;
				}
					
				HW_BufferRemoveFront(&debugger->cmdbuffer, totalsize);
			}
		}
		
		DebuggerCommandUnlock(debugger);
	}
	
	return result;
}


static void DebuggerReplyClear(Debugger* debugger)
{
	HW_BufferClear(&debugger->gdbreplybuffer);
}


static void DebuggerReplyBuffer(Debugger* debugger, unsigned char* buffer, unsigned int buffersize)
{
	HW_BufferAppend(&debugger->gdbreplybuffer, buffer, buffersize);
}

static void DebuggerReplyByte(Debugger* debugger, unsigned char byte)
{
	DebuggerReplyBuffer(debugger, &byte, 1);
}

static void DebuggerReplyHexByte(Debugger* debugger, unsigned char byte)
{
	char asciibuffer[2];
	
	asciibuffer[0] = hextoascii(byte>>4);
	asciibuffer[1] = hextoascii(byte>>0);
	
	DebuggerReplyBuffer(debugger, asciibuffer, 2);
}

static void DebuggerReplyHexWord(Debugger* debugger, unsigned int word)
{
	DebuggerReplyHexByte(debugger, word >> 0);
	DebuggerReplyHexByte(debugger, word >> 8);
	DebuggerReplyHexByte(debugger, word >> 16);
	DebuggerReplyHexByte(debugger, word >> 24);
}

static void DebuggerReplySend(Debugger* debugger)
{
	unsigned int i;
	int checksum = 0;
	unsigned char* buffer = debugger->gdbreplybuffer.buffer;
	unsigned int buffersize = debugger->gdbreplybuffer.size;
	
	DebuggerSendByte(debugger, '$');
	
	for(i=0; i<buffersize; i++)
		checksum += buffer[i];
		
	DebuggerSend(debugger, buffer, buffersize);
		
	
	DebuggerSendByte(debugger, '#');
	DebuggerSendHexByte(debugger, checksum);
}

static void DebuggerReplyError(Debugger* debugger, unsigned int err)
{
	DebuggerReplyClear(debugger);
	DebuggerReplyByte(debugger, 'E');
	DebuggerReplyHexByte(debugger, err);
	DebuggerReplySend(debugger);
}


static void DebuggerReplyOk(Debugger* debugger)
{
	DebuggerReplyClear(debugger);
	DebuggerReplyByte(debugger, 'O');
	DebuggerReplyByte(debugger, 'K');
	DebuggerReplySend(debugger);
}


static void DebuggerReplyNop(Debugger* debugger)
{
	DebuggerReplyClear(debugger);
	DebuggerReplySend(debugger);
}

int DebuggerParsePacket(Debugger* debugger, HWBuffer* packet)
{
	int checksum, escaped, rchksum;
	unsigned char ch, hi, lo;
	unsigned char nullterminator = 0;

	buffercursor cursor;
	
	HW_BufferClear(packet);
	
	cursor.pos = debugger->receivebuffer.buffer;
	cursor.posend = cursor.pos + debugger->receivebuffer.size;
	cursor.posbegin = cursor.pos;
		
	while(1)
	{
		if (!getbyte(&cursor, &ch))
			return 0;
			
		if (ch == '$')
			break;
	}
	

	checksum = 0;
	escaped = 0;
	
	while (1) 
	{
		if (!getbyte(&cursor, &ch))
			return 0;
			
		if (!escaped) 
		{
			if (ch == '$') 
			{
				checksum = 0;
			} 
			else if (ch == '#')
				break;
			else if (ch == 0x7d) 
			{
				escaped = 1;
				checksum += ch;
			} 
			else 
			{
				checksum += ch;
				HW_BufferAppend(packet, &ch, 1);
			}
		} 
		else 
		{
			escaped = 0;
			checksum += ch;
			ch ^= 0x20;
			HW_BufferAppend(packet, &ch, 1);
		}
	}
	
	if (ch != '#') 
		return 0;
		

	if (!getbyte(&cursor, &hi))
		return 0;

	if (!getbyte(&cursor, &lo))
		return 0;
		
	rchksum = asciitohex(hi) << 4;
	rchksum += asciitohex(lo);

	if ((checksum & 0xff) != rchksum)
	{
		return -1;
	}
	
	// Append null-terminator, for easy scanning
	HW_BufferAppend(packet, &nullterminator, 1);
	HW_BufferRemoveFront(&debugger->receivebuffer, cursor.pos - cursor.posbegin);
	
	int i;
	for(i=0; i<packet->size; i++)
		printf("%c", packet->buffer[i]);
		
	return 1;
}

void DebuggerWrite(Debugger* debugger, unsigned int cmdid, unsigned char* buffer, unsigned int buffersize) 
{
	int signal;
	unsigned int bufferpos = 0;	
	unsigned int val, n, i;
	unsigned int regs[17];
	
	DebuggerReplyClear(debugger);	

	switch(cmdid)
	{
		case CMD_GDB_LASTSIG:
		{
			n = buffer_readle32(buffer, &bufferpos, buffersize);	
			
			switch (n)
			{
				case VEC_UND:
					signal = 4;
					break;
				case VEC_PABT:
				case VEC_DABT:
					signal = 7;
					break;
				default:
					signal = 5;
					break;
			}

			printf("CMD_GDB_LASTSIG: %d\n", signal);
			DebuggerReplyByte(debugger, 'S');
			DebuggerReplyHexByte(debugger, signal);
			DebuggerReplySend(debugger);
			break;
		}
		
		case CMD_GDB_READREG:
		{
			val = buffer_readle32(buffer, &bufferpos, buffersize);	

			printf("CMD_GDB_READREG: 0x%08X\n", val);

			DebuggerReplyHexWord(debugger, val);
			DebuggerReplySend(debugger);
			break;
		}
		
		case CMD_GDB_READREGS:
		{
			printf("CMD_GDB_READREGS:\n");
			for(i=0; i<17; i++)
				regs[i] = buffer_readle32(buffer, &bufferpos, buffersize);
				
			for(i=0; i<16; i++)
				DebuggerReplyHexWord(debugger, regs[i]);

			for(i=0; i<17; i++)
				DebuggerReplyHexWord(debugger, 0);
				
			for(i=0; i<17; i++)
				printf("[%d] = 0x%08X\n", i, regs[i]);

			DebuggerReplyHexWord(debugger, regs[16]);
			DebuggerReplySend(debugger);
			break;
		}
		
		case CMD_GDB_WRITEREG:
		{
			printf("CMD_GDB_WRITEREG: OK\n");
			DebuggerReplyOk(debugger);
			break;
		}
		
		case CMD_GDB_WRITEREGS:
		{
			printf("CMD_GDB_WRITEREGS: OK\n");
			DebuggerReplyOk(debugger);
			break;
		}
		
		case CMD_GDB_READMEM:
		{
			printf("CMD_GDB_READMEM: 0x%x\n", buffersize);
			for(i=0; i<buffersize; i++)
				printf("%02X ", buffer[i]);
			
			for(i=0; i<buffersize; i++)
				DebuggerReplyHexByte(debugger, buffer[i]);
				
			DebuggerReplySend(debugger);
			break;
		}
		
		case CMD_GDB_WRITEMEM:
		{
			printf("CMD_GDB_WRITEMEM: OK\n");
			DebuggerReplyOk(debugger);
			break;
		}
		
		default: 
		{
			DebuggerReplyError(debugger, 0); 
			break;
		}
	}
}


static void DebuggerCommandLock(Debugger* debugger)
{
	pthread_mutex_lock(&debugger->mutex);
}

static void DebuggerCommandUnlock(Debugger* debugger)
{
	pthread_mutex_unlock(&debugger->mutex);
}

static int DebuggerCommandTryLock(Debugger* debugger)
{
	return pthread_mutex_trylock(&debugger->mutex);
}

static void DebuggerCommandAddBegin(Debugger* debugger, unsigned int cmdid)
{
	unsigned char buffer[4];
	
	if (debugger->curcmdactive)
		return;
		
	DebuggerCommandLock(debugger);
	
	debugger->curcmdpos = debugger->cmdbuffer.size;
	debugger->curcmdsize = 0;
	
	buffer[0] = cmdid>>0;
	buffer[1] = cmdid>>8;
	buffer[2] = cmdid>>16;
	buffer[3] = cmdid>>24;
	HW_BufferAppend(&debugger->cmdbuffer, buffer, 4);
	
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;
	HW_BufferAppend(&debugger->cmdbuffer, buffer, 4);
	
	debugger->curcmdactive = 1;
}

static void DebuggerCommandAddBuffer(Debugger* debugger, unsigned char* buffer, unsigned int size)
{
	if (!debugger->curcmdactive)
		return;

	debugger->curcmdsize += size;
	
	HW_BufferAppend(&debugger->cmdbuffer, buffer, size);	
}

static void DebuggerCommandAddLong(Debugger* debugger, unsigned int data)
{
	unsigned char buffer[4];
	
	buffer[0] = data>>0;
	buffer[1] = data>>8;
	buffer[2] = data>>16;
	buffer[3] = data>>24;
	
	DebuggerCommandAddBuffer(debugger, buffer, 4);
}


static void DebuggerCommandAddEnd(Debugger* debugger)
{
	unsigned char buffer[4];

	if (!debugger->curcmdactive)
		return;
		
		
	buffer[0] = debugger->curcmdsize >> 0;
	buffer[1] = debugger->curcmdsize >> 8;
	buffer[2] = debugger->curcmdsize >> 16;
	buffer[3] = debugger->curcmdsize >> 24;
	
	memcpy(debugger->cmdbuffer.buffer + debugger->curcmdpos + 4, buffer, 4);
	
	DebuggerCommandUnlock(debugger);
	
	debugger->curcmdactive = 0;
}

static void DebuggerCommandAdd(Debugger* debugger, unsigned int cmdid)
{
	DebuggerCommandAddBegin(debugger, cmdid);
	DebuggerCommandAddEnd(debugger);
}

static void DebuggerProcessPacket(Debugger* debugger, unsigned char* buffer, unsigned int buffersize)
{
	unsigned int v, i, r, p = -1;
	unsigned char command, byte;
	unsigned int sig;
	unsigned long addr;
	unsigned long len;
	unsigned int valid;
	unsigned int mask = 0;
	unsigned int regs[17];
	unsigned int regcount = 0;
	
	
	if (buffersize <= 0)
		return;
		
	command = *buffer++;	
	buffersize--;
	
	printf("|");
	for(i=0; i<buffersize; i++)
	printf("%c", buffer[i]);
	printf("|\n");


		
	switch(command)
	{
		case '?':
		{
			printf("CMD_GDB_LASTSIG\n");
			DebuggerCommandAdd(debugger, CMD_GDB_LASTSIG);
			break;
		}
		
		case 's':
		{
			DebuggerReplyError(debugger, 0);
			break;
		}
		
		case 'c':
		{
			if (sscanf(buffer, "%x;%lx", &sig, &addr) == 2) 
			{
				mask |= 1;
				regs[15] = addr;
			} 
			else if (sscanf(buffer, "%lx", &addr) == 1) 
			{
				mask |= 1;
				regs[15] = addr;
			}
			
			printf("CMD_GDB_CONTINUE: 0x%08X 0x%08X\n", mask, regs[15]);
			DebuggerCommandAddBegin(debugger, CMD_GDB_CONTINUE);
			DebuggerCommandAddLong(debugger, mask);
			DebuggerCommandAddLong(debugger, regs[15]);
			DebuggerCommandAddEnd(debugger);
			break;
		}
				
		
		case 'p':
		{
			if (sscanf(buffer, "%x", &r) != 1) 
			{
				DebuggerReplyError(debugger, 0);
				break;
			}
			
			valid = 0;
			if (r >= 0 && r <= 15)
				valid = 1;
			if (r == 25)
			{
				r = 16;
				valid = 1;
			}
				
			if (!valid)
			{
				DebuggerReplyError(debugger, 0);
				break;
			}
			
			printf("CMD_GDB_READREG: R[%d]\n", r);
			DebuggerCommandAddBegin(debugger, CMD_GDB_READREG);
			DebuggerCommandAddLong(debugger, r);
			DebuggerCommandAddEnd(debugger);
			break;
		}
		
		case 'P':
		{
			sscanf(buffer, "%x=%n", &r, &p);

			if (p == -1) 
			{
				DebuggerReplyError(debugger, 0);
				break;
			}

			v = gethexword(buffer + p);
			
			printf("CMD_GDB_WRITEREG: R[%d] = 0x%08X\n", r, v);
			DebuggerCommandAddBegin(debugger, CMD_GDB_WRITEREG);
			DebuggerCommandAddLong(debugger, r);
			DebuggerCommandAddLong(debugger, v);
			DebuggerCommandAddEnd(debugger);
			break;
		}
		
		case 'g':
		{
			printf("CMD_GDB_READREGS\n");
			DebuggerCommandAdd(debugger, CMD_GDB_READREGS);
			break;
		}
		
		case 'G':
		{
			printf("CMD_GDB_WRITEREGS\n");
			for (i = 0; i < 16 && buffersize >= (i + 1) * 8; i++) 
			{
				regs[regcount] = gethexword(buffer);
				buffer += 8;
				
				regcount++;
			}

			if (buffersize >= 16 * 8 + 8 * 16 + 2 * 8)
			{
				buffer += 8 * 16 + 8;
				regs[16] = gethexword(buffer);
				regcount++;
			}
			
			for(i=0; i<regcount; i++)
			{
				printf("[%d] = %08X\n", i, regs[i]);
			}			
			
			DebuggerCommandAddBegin(debugger, CMD_GDB_WRITEREGS);
			DebuggerCommandAddLong(debugger, regcount);
			for(i=0; i<17; i++)
			{
				DebuggerCommandAddLong(debugger, regs[i]);
			}
			DebuggerCommandAddEnd(debugger);
			break;
		}	

		case 'm':
		{
			if (sscanf(buffer, "%lx,%lx", &addr, &len) != 2) 
			{
				DebuggerReplyError(debugger, 0);
				break;
			}
			
			printf("CMD_GDB_READMEM: 0x%08lx 0x%08lx\n", addr, len);
			DebuggerCommandAddBegin(debugger, CMD_GDB_READMEM);
			DebuggerCommandAddLong(debugger, addr);
			DebuggerCommandAddLong(debugger, len);
			DebuggerCommandAddEnd(debugger);			
			break;
		}
		
		case 'M':
		{
			p = -1;
			sscanf(buffer, "%lx,%lx:%n", &addr, &len, &p);
			
			if (p == -1) 
			{
				DebuggerReplyError(debugger, 0);
				return;
			}
			
			printf("CMD_GDB_WRITEMEM: 0x%08lx 0x%08lx\n", addr, len);
			DebuggerCommandAddBegin(debugger, CMD_GDB_WRITEMEM);
			DebuggerCommandAddLong(debugger, addr);
			DebuggerCommandAddLong(debugger, len);
			for(i=0; i<len; i++)
			{
				byte = gethexbyte(buffer + p + i * 2);
				DebuggerCommandAddBuffer(debugger, &byte, 1);
				
				printf("0x%02X ", byte);
			}
			
			DebuggerCommandAddEnd(debugger);			
			break;
		}
		
		default:
			DebuggerReplyNop(debugger);
		break;
	}
}

static void* DebuggerReceiveThread(void* arg)
{
	Debugger* debugger = (Debugger*)arg;
	int shutdownfd = debugger->shutdownpipe[0];
	int remotesockfd;
	int fdmax;
	fd_set readfds;
	int parseresult;
	
	while(debugger->running)
	{
		unsigned char buf[4096];
		int nbytes;
		struct timeval timeout;
		unsigned int maxsize = sizeof(buf);
		unsigned int availablesize = debugger->receivebuffer.capacity - debugger->receivebuffer.size;
		
		if (maxsize > availablesize)
			maxsize = availablesize;
			
		remotesockfd = debugger->remotesockfd;
		
		if (debugger->remoteflush)
		{
			debugger->remoteflush = 0;
			
			HW_BufferClear(&debugger->receivebuffer);
		}

		if (remotesockfd == 0 || maxsize == 0)
		{
			mssleep(1);
			continue;	
		}
		
	
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;			

		FD_ZERO(&readfds);
		FD_SET(remotesockfd, &readfds);
		FD_SET(shutdownfd, &readfds);
		
		fdmax = remotesockfd;
		if (fdmax < shutdownfd)
			fdmax = shutdownfd;
		if (select(fdmax+1, &readfds, NULL, NULL, &timeout) == -1) 
		{
			perror("select");
			
			fprintf(stdout, "> Debugger: Connection closed by %s\n", debugger->remoteIP);
			close(remotesockfd);
			debugger->remotesockfd = 0;			
			
			continue;
		} 
		
		if (FD_ISSET(shutdownfd, &readfds))
		{
			break;
		}
		if (FD_ISSET(remotesockfd, &readfds))
		{
			if ((nbytes = recv(remotesockfd, buf, maxsize, 0)) <= 0) 
			{
				if (nbytes == 0) {
					fprintf(stdout, "> Debugger: Connection closed by %s\n", debugger->remoteIP);
				} else {
					perror("recv");
				}
				close(remotesockfd);
				
				debugger->remotesockfd = 0;
			}
			else
			{
				HW_BufferFill(&debugger->receivebuffer, buf, nbytes);
				
				while(1)
				{
					parseresult = DebuggerParsePacket(debugger, &debugger->gdbrequestbuffer);
					
					if (parseresult == 1)
					{
						DebuggerSendByte(debugger, '+');
						
						DebuggerProcessPacket(debugger, debugger->gdbrequestbuffer.buffer, debugger->gdbrequestbuffer.size);					
					}
					else if (parseresult == -1)
					{
						DebuggerSendByte(debugger, '-');
					}
					else
					{
						break;
					}
				}
			}
		}
	}
   
	return 0;
}

