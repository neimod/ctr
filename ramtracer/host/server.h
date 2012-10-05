/*
 * server.h - Server functionality.
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

#ifndef __SERVER_H_
#define __SERVER_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "hw_buffer.h"
#include "hw_command.h"

#define SERVER_MIN_CMD_ID	0x80
#define SERVER_MAX_BUFFERSIZE 		(4 * 1024 * 1024)
#define SERVER_MAX_CHALLENGESIZE	0x100

/*
 * Server -- Worker structure for the server.
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
	
	unsigned int authsize;
	unsigned char auth[SERVER_MAX_CHALLENGESIZE];
	int shutdownpipe[2];
	pthread_mutex_t receivemutex;
	HWBuffer receivebuffer;
} Server;

/*
 * Public functions
 */
int  ServerIsEnabled();
void ServerSetEnabled(int enabled);
void ServerBegin(Server* server);
void ServerFinish(Server* server);
unsigned int ServerRead(Server* server, HWBuffer* buffer);
void ServerWriteBuffer(Server* server, unsigned char* buffer, unsigned int buffersize);
void ServerWrite(Server* server, unsigned int command, unsigned char* buffer, unsigned int buffersize);

#endif // __SERVER_H_
