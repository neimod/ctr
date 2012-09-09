#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctrclient.h"


void hexdump(void *ptr, int buflen)
{
	unsigned char *buf = (unsigned char*)ptr;
	int i, j;

	for (i=0; i<buflen; i+=16)
	{
		printf("%06x: ", i);
		for (j=0; j<16; j++)
		{ 
			if (i+j < buflen)
			{
				printf("%02x ", buf[i+j]);
			}
			else
			{
				printf("   ");
			}
		}

		printf(" ");

		for (j=0; j<16; j++) 
		{
			if (i+j < buflen)
			{
				printf("%c", (buf[i+j] >= 0x20 && buf[i+j] <= 0x7e) ? buf[i+j] : '.');
			}
		}
		printf("\n");
	}
}



int main(int argc, char *argv[])
{
	ctrclient client;
	unsigned char buffer[4096];
	unsigned char key[16];
	unsigned char ctr[16];

	memset(buffer, 0, sizeof(buffer));
	memset(key, 0, 0x10);
	memset(ctr, 0, 0x10);

	ctrclient_init();

	if (0 == ctrclient_connect(&client, "192.168.1.130", "8333"))
		exit(1);

	
	if (!ctrclient_aes_set_key(&client, 0, key))
		return 1;
	if (!ctrclient_aes_set_ctr(&client, ctr))
		return 1;
	if (!ctrclient_aes_ctr_crypt(&client, buffer, 16))
		return 1;
	hexdump(buffer, 16);

	ctrclient_disconnect(&client);

    return 0;
}
