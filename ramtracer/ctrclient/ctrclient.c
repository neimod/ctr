#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <io.h>

	#pragma comment(lib, "Ws2_32.lib")
#else
	#include <unistd.h>
	#include <errno.h>
	#include <netdb.h>
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
#endif

#include "ctrclient.h"

static void putle32(unsigned char* p, unsigned int value)
{
	*p++ = value>>0;
	*p++ = value>>8;
	*p++ = value>>16;
	*p++ = value>>24;
}



static void* get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void ctrclient_init()
{
#ifdef _WIN32
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) 
	{
		fprintf(stderr, "WSAStartup failed: %d\n", iResult);
	}
#endif
}

int ctrclient_connect(ctrclient* client, const char* hostname, const char* port)
{
    struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
	unsigned int authsize = 0;
	unsigned char auth[MAX_CHALLENGESIZE];

	FILE* authfile = fopen("auth.txt", "rb");

	if (authfile == 0)
	{
		fprintf(stderr, "Could not open auth.txt file\n");
		return 0;
	}
	else
	{
		fseek(authfile, 0, SEEK_END);
		authsize = ftell(authfile);
		if (authsize >= MAX_CHALLENGESIZE)
			authsize = MAX_CHALLENGESIZE;
		fseek(authfile, 0, SEEK_SET);
		fread(auth, 1, authsize, authfile);
		fclose(authfile);
	}


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) 
	{
        fprintf(stderr, "getaddrinfo: %d, %s\n", rv, gai_strerror(rv));
        return 0;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
	{
        if ((client->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{
            continue;
        }

        if (connect(client->sockfd, p->ai_addr, p->ai_addrlen) == -1) 
		{
            ctrclient_disconnect(client);
            continue;
        }

        break;
    }

    if (p == NULL) 
	{
		fprintf(stderr, "ctrclient: failed to connect\n");
        return 0;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    

    freeaddrinfo(servinfo);

	if (!ctrclient_sendbuffer(client, auth, authsize))
		return 0;

	fprintf(stdout, "ctrclient: connected to %s\n", s);
	return 1;
}

void ctrclient_disconnect(ctrclient* client)
{
#ifdef _WIN32
	closesocket(client->sockfd);
#else
	close(client->sockfd);
#endif

	client->sockfd = 0;
}

int ctrclient_sendbuffer(ctrclient* client, const void* buffer, unsigned int size)
{
	unsigned char* bytebuffer = (unsigned char*)buffer;

	while(size)
	{
		int nbytes = send(client->sockfd, bytebuffer, size, 0);
		
		if (nbytes <= 0)
		{
			perror("send");
			return 0;
		}

		size -= nbytes;
		bytebuffer += nbytes;
	}

	return 1;
}


int ctrclient_recvbuffer(ctrclient* client, void* buffer, unsigned int size)
{
	unsigned char* bytebuffer = (unsigned char*)buffer;

	while(size)
	{
		int nbytes = recv(client->sockfd, bytebuffer, size, 0);
		
		if (nbytes <= 0)
		{
			perror("recv");
			return 0;
		}

		size -= nbytes;
		bytebuffer += nbytes;
	}

	return 1;
}

int ctrclient_sendlong(ctrclient* client, unsigned int value)
{
	unsigned char buffer[4];


	buffer[0] = value>>0;
	buffer[1] = value>>8;
	buffer[2] = value>>16;
	buffer[3] = value>>24;

	return ctrclient_sendbuffer(client, buffer, 4);
}

int ctrclient_aes_ctr_crypt(ctrclient* client, unsigned char* buffer, unsigned int size)
{
	unsigned char header[8];

	while(size)
	{
		unsigned int maxsize = CHUNKMAXSIZE;
		if (maxsize > size)
			maxsize = size;

		if (!ctrclient_sendlong(client, 0x80))
			return 0;
		if (!ctrclient_sendlong(client, maxsize))
			return 0;
		if (!ctrclient_sendbuffer(client, buffer, maxsize))
			return 0;
		if (!ctrclient_recvbuffer(client, header, 8))
			return 0;
		if (!ctrclient_recvbuffer(client, buffer, maxsize))
			return 0;

		buffer += maxsize;
		size -= maxsize;
	}

	return 1;
}

int ctrclient_aes_control(ctrclient* client, aescontrol* control)
{
	unsigned char header[8];
	unsigned int size = sizeof(aescontrol);

	if (size != 40)
		return 0;

	if (!ctrclient_sendlong(client, 0x81))
		return 0;
	if (!ctrclient_sendlong(client, size))
		return 0;
	if (!ctrclient_sendbuffer(client, control, size))
		return 0;
	if (!ctrclient_recvbuffer(client, header, 8))
		return 0;

	return 1;
}

int ctrclient_aes_set_key(ctrclient* client, unsigned int keyslot, unsigned char key[16])
{
	aescontrol control;

	memset(&control, 0, sizeof(aescontrol));
	putle32(control.flags, AES_FLAGS_SET_KEY | AES_FLAGS_SELECT_KEY);
	putle32(control.keyslot, keyslot);
	memcpy(control.key, key, 16);

	return ctrclient_aes_control(client, &control);
}

int ctrclient_aes_set_ykey(ctrclient* client, unsigned int keyslot, unsigned char key[16])
{
	aescontrol control;

	memset(&control, 0, sizeof(aescontrol));
	putle32(control.flags, AES_FLAGS_SET_YKEY | AES_FLAGS_SELECT_KEY);
	putle32(control.keyslot, keyslot);
	memcpy(control.key, key, 16);

	return ctrclient_aes_control(client, &control);
}

int ctrclient_aes_select_key(ctrclient* client, unsigned int keyslot)
{
	aescontrol control;

	memset(&control, 0, sizeof(aescontrol));
	putle32(control.flags, AES_FLAGS_SELECT_KEY);
	putle32(control.keyslot, keyslot);

	return ctrclient_aes_control(client, &control);
}


int ctrclient_aes_set_iv(ctrclient* client, unsigned char iv[16])
{
	aescontrol control;

	memset(&control, 0, sizeof(aescontrol));
	putle32(control.flags, AES_FLAGS_SET_IV);
	memcpy(control.iv, iv, 16);

	return ctrclient_aes_control(client, &control);
}

int ctrclient_aes_set_ctr(ctrclient* client, unsigned char ctr[16])
{
	return ctrclient_aes_set_iv(client, ctr);
}
