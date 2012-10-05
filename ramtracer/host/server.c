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
#include "server.h"
#include "utils.h"

#define PORT "8333" 
#define BACKLOG 10     // how many pending connections queue will hold

static int serverEnabled = 0;

// Private functions
static void* ServerListenThread(void* arg);
static void* ServerReceiveThread(void* arg);
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

int ServerIsEnabled()
{
	return serverEnabled;
}

void ServerSetEnabled(int enabled)
{
	serverEnabled = enabled;
}

void ServerBegin(Server* server)
{
	int err;
	FILE* authfile = fopen("auth.txt", "rb");

	if (authfile == 0)
	{
		fprintf(stderr, "Could not find auth.txt file.\n");
		exit(1);
	}
	
	fseek(authfile, 0, SEEK_END);
	server->authsize = ftell(authfile);
	if (server->authsize >= SERVER_MAX_CHALLENGESIZE)
		server->authsize = SERVER_MAX_CHALLENGESIZE;
	fseek(authfile, 0, SEEK_SET);
	
	fread(server->auth, 1, server->authsize, authfile);
	fclose(authfile);
	
	fprintf(stdout, "Server authentication: %d bytes.\n", server->authsize);

	server->running = 1;
	server->remoteaddrlen = 0;
	server->remotesockfd = 0;
	server->remoteflush = 0;
	
	// Setup a pipe that will receive the shutdown command
	if (pipe(server->shutdownpipe) == -1) 
	{
		perror("pipe");
		exit(1);
	}	
	
	HW_BufferInit(&server->receivebuffer, SERVER_MAX_BUFFERSIZE);
	pthread_mutex_init(&server->receivemutex, 0);

	err = pthread_create(&server->listenthread, NULL, ServerListenThread, server);

	if (err != 0)
	{
		perror("error starting server listener thread");
		exit(1);
	}

	err = pthread_create(&server->receivethread, NULL, ServerReceiveThread, server);

	if (err != 0)
	{
		perror("error starting server receive thread");
		exit(1);
	}
	
}



void ServerFinish(Server* server)
{
	int err;
	

	server->running = 0;
	write(server->shutdownpipe[1], "Y", 1);
	
	err = pthread_join(server->listenthread, 0);
	
	if (err != 0)
	{
		perror("error stopping listen thread");
		exit(1);
	}
	
	err = pthread_join(server->receivethread, 0);
	
	if (err != 0)
	{
		perror("error stopping receive thread");
		exit(1);
	}
	
	HW_BufferDestroy(&server->receivebuffer);	
	pthread_mutex_destroy(&server->receivemutex);
}
	

static void* ServerListenThread(void* arg)
{
	Server* server = (Server*)arg;
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
	
	shutdownfd = server->shutdownpipe[0];
	
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
	fprintf(stdout, "Server: Listening on port %s\n", PORT);
	
	fdmax = listensockfd;
	if (fdmax < shutdownfd)
		fdmax = shutdownfd;
	
	while(server->running)
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
				fprintf(stdout, "> New connection from %s\n", inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN));
				
				if (server->remotesockfd == 0 || sockaddrequal((struct sockaddr*)&remoteaddr, (struct sockaddr*)&server->remoteaddr))
				{
					if (server->remotesockfd != 0)
						close(server->remotesockfd);
					memcpy(server->remoteIP, remoteIP, INET6_ADDRSTRLEN);
					memcpy(&server->remoteaddr, &remoteaddr, addrlen);
					server->remoteaddrlen = addrlen;		
					server->remotesockfd = sockfd;
					server->remoteflush = 1;					
				}
			}
		}
	
	}
   
  

	return 0;
}

void ServerWriteBuffer(Server* server, unsigned char* buffer, unsigned int buffersize)
{
	int remotesockfd = server->remotesockfd;
	
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


void ServerWrite(Server* server, unsigned int command, unsigned char* buffer, unsigned int buffersize)
{
	unsigned char header[8];
	int remotesockfd = server->remotesockfd;
	
	
	header[0] = command>>0;
	header[1] = command>>8;
	header[2] = command>>16;
	header[3] = command>>24;
	header[4] = buffersize>>0;
	header[5] = buffersize>>8;
	header[6] = buffersize>>16;
	header[7] = buffersize>>24;

	ServerWriteBuffer(server, header, 8);
	ServerWriteBuffer(server, buffer, buffersize);
}

/**
	Reads a command from the receive buffer, and appends it to the HWBuffer.
	
	Returns 1 if a command was read, 0 otherwise.
 */
unsigned int ServerRead(Server* server, HWBuffer* buffer)
{
	unsigned int result = 0;
	unsigned char dummybuffer[4] = {0, 0, 0, 0};
	unsigned int alignedsize, restsize;
	

	if (0 == pthread_mutex_trylock(&server->receivemutex))
	{
		// move first command to buffer
		if (server->receivebuffer.size >= 8)
		{
			unsigned char* cmdbuf = server->receivebuffer.buffer;
			unsigned int cmdid = (cmdbuf[0]<<0) | (cmdbuf[1]<<8) | (cmdbuf[2]<<16) | (cmdbuf[3]<<24);
			unsigned int cmdsize = (cmdbuf[4]<<0) | (cmdbuf[5]<<8) | (cmdbuf[6]<<16) | (cmdbuf[7]<<24);
			unsigned int totalsize = 8+cmdsize;
			unsigned int valid = 0;
			
			
			if (cmdid == CMD_AESCONTROL && cmdsize == CMD_AESCONTROL_SIZE)
				valid = 1;
			else if (cmdid == CMD_AESCTR)
				valid = 1;
			else if (cmdid == CMD_AESCBCDEC)
				valid = 1;
			else if (cmdid == CMD_AESCBCENC)
				valid = 1;
			else if (cmdid == CMD_AESCCMENC || cmdid == CMD_AESCCMDEC)
			{
				if (server->receivebuffer.size >= 16)
				{
					unsigned int payloadblockcount = (cmdbuf[8]<<0) | (cmdbuf[9]<<8) | (cmdbuf[10]<<16) | (cmdbuf[11]<<24);
					unsigned int assocblockcount = (cmdbuf[12]<<0) | (cmdbuf[13]<<8) | (cmdbuf[14]<<16) | (cmdbuf[15]<<24);
					unsigned int expectedsize = payloadblockcount * 16 + assocblockcount * 16 + 28;
					
					if (payloadblockcount <= 0xFFFF && assocblockcount <= 0xFFFF && cmdsize == expectedsize)
						valid = 1;
				}
			}
			
			
			if (totalsize <= SERVER_MAX_BUFFERSIZE && server->receivebuffer.size >= totalsize)
			{
				if (valid && cmdid >= SERVER_MIN_CMD_ID)
				{					
					HW_BufferAppend(buffer, cmdbuf, totalsize);
					
					alignedsize = (cmdsize+3) & (~3);
					restsize = alignedsize - cmdsize;
					if (restsize)
						HW_BufferAppend(buffer, dummybuffer, restsize);
						
					result = 1;
				}
					
				HW_BufferRemoveFront(&server->receivebuffer, totalsize);
			}
		}
		
		pthread_mutex_unlock(&server->receivemutex);
	}
	
	return result;
}


static void* ServerReceiveThread(void* arg)
{
	Server* server = (Server*)arg;
	int shutdownfd = server->shutdownpipe[0];
	int remotesockfd;
	int fdmax;
	fd_set readfds;
	int authenticated = 0;
	int challengepos = 0;
	unsigned char challenge[SERVER_MAX_CHALLENGESIZE];
	
	while(server->running)
	{
		unsigned char buf[4096];
		int nbytes;
		struct timeval timeout;
		unsigned int maxsize = sizeof(buf);
		unsigned int availablesize = server->receivebuffer.capacity - server->receivebuffer.size;
		
		if (maxsize > availablesize)
			maxsize = availablesize;
			
		remotesockfd = server->remotesockfd;
		
		if (server->remoteflush)
		{
			authenticated = 0;
			challengepos = 0;
			
			server->remoteflush = 0;
			
			pthread_mutex_lock(&server->receivemutex);
			HW_BufferClear(&server->receivebuffer);
			pthread_mutex_unlock(&server->receivemutex);			
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
			
			fprintf(stdout, "> Server: Connection closed by %s\n", server->remoteIP);
			close(remotesockfd);
			server->remotesockfd = 0;			
			
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
					fprintf(stdout, "> Server: Connection closed by %s\n", server->remoteIP);
				} else {
					perror("recv");
				}
				close(remotesockfd);
				
				server->remotesockfd = 0;
			}
			else
			{
				if (!authenticated)
				{
					unsigned int i;
					unsigned int maxsize = server->authsize - challengepos;
					
					if (maxsize > nbytes)
						maxsize = nbytes;
					
					memcpy(challenge + challengepos, buf, maxsize);					
					memmove(buf, buf + maxsize, nbytes-maxsize);
					challengepos += maxsize;
					nbytes -= maxsize;
					
					
					if (challengepos == server->authsize)
					{
						authenticated = 1;
					
						for(i=0; i<challengepos; i++)
						{
							if (challenge[i] != server->auth[i])
								authenticated = 0;
						}
						
						if (!authenticated)
						{
							fprintf(stderr, "> Server: Incorrect authentication detected.\n");
						}
					}
				}
				
				pthread_mutex_lock(&server->receivemutex);
				HW_BufferFill(&server->receivebuffer, buf, nbytes);
				pthread_mutex_unlock(&server->receivemutex);
			}
		}
	}
   
	return 0;
}

