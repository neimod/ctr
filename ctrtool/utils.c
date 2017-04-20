
#include <stdio.h>
#include <string.h>
#include "utils.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif


u32 align(u32 offset, u32 alignment)
{
	u32 mask = ~(alignment-1);

	return (offset + (alignment-1)) & mask;
}

u64 align64(u64 offset, u32 alignment)
{
	u64 mask = ~(alignment-1);

	return (offset + (alignment-1)) & mask;
}

u64 getle64(const u8* p)
{
	u64 n = p[0];

	n |= (u64)p[1]<<8;
	n |= (u64)p[2]<<16;
	n |= (u64)p[3]<<24;
	n |= (u64)p[4]<<32;
	n |= (u64)p[5]<<40;
	n |= (u64)p[6]<<48;
	n |= (u64)p[7]<<56;
	return n;
}

u64 getbe64(const u8* p)
{
	u64 n = 0;

	n |= (u64)p[0]<<56;
	n |= (u64)p[1]<<48;
	n |= (u64)p[2]<<40;
	n |= (u64)p[3]<<32;
	n |= (u64)p[4]<<24;
	n |= (u64)p[5]<<16;
	n |= (u64)p[6]<<8;
	n |= (u64)p[7]<<0;
	return n;
}

u32 getle32(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

u32 getbe32(const u8* p)
{
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | (p[3]<<0);
}

u32 getle16(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8);
}

u32 getbe16(const u8* p)
{
	return (p[0]<<8) | (p[1]<<0);
}

void putle16(u8* p, u16 n)
{
	p[0] = n;
	p[1] = n>>8;
}

void putle32(u8* p, u32 n)
{
	p[0] = n;
	p[1] = n>>8;
	p[2] = n>>16;
	p[3] = n>>24;
}


void readkeyfile(u8* key, const char* keyfname)
{
	FILE* f = fopen(keyfname, "rb");
	u32 keysize = 0;

	if (0 == f)
	{
		fprintf(stdout, "Error opening key file\n");
		goto clean;
	}

	fseek(f, 0, SEEK_END);
	keysize = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (keysize != 16)
	{
		fprintf(stdout, "Error key size mismatch, got %d, expected %d\n", keysize, 16);
		goto clean;
	}

	if (16 != fread(key, 1, 16, f))
	{
		fprintf(stdout, "Error reading key file\n");
		goto clean;
	}

clean:
	if (f)
		fclose(f);
}

void hexdump(void *ptr, int buflen)
{
	u8 *buf = (u8*)ptr;
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

void memdump(FILE* fout, const char* prefix, const u8* data, u32 size)
{
	u32 i;
	u32 prefixlen = strlen(prefix);
	u32 offs = 0;
	u32 line = 0;
	while(size)
	{
		u32 max = 32;

		if (max > size)
			max = size;

		if (line==0)
			fprintf(fout, "%s", prefix);
		else
			fprintf(fout, "%*s", prefixlen, "");


		for(i=0; i<max; i++)
			fprintf(fout, "%02X", data[offs+i]);
		fprintf(fout, "\n");
		line++;
		size -= max;
		offs += max;
	}
}

int makedir(const char* dir)
{
#ifdef _WIN32
	return _mkdir(dir);
#else
	return mkdir(dir, 0777);
#endif
}
