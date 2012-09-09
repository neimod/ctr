#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include "hw_config.h"
#endif

#include "utils.h"

unsigned int buffer_readle32(unsigned char* buffer, unsigned int* bufferpos, unsigned int buffersize)
{
	unsigned char* p = buffer + *bufferpos;
	
	if (*bufferpos+4 > buffersize)
	{
		perror("buffer out of bounds");
		exit(1);
	}
	*bufferpos += 4;
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

unsigned int buffer_readle16(unsigned char* buffer, unsigned int* bufferpos, unsigned int buffersize)
{
	unsigned char* p = buffer + *bufferpos;
	
	if (*bufferpos+2 > buffersize)
	{
		perror("buffer out of bounds");
		exit(1);
	}
	*bufferpos += 2;
	return p[0] | (p[1] << 8);
}

unsigned int buffer_readbyte(unsigned char* buffer, unsigned int* bufferpos, unsigned int buffersize)
{
	unsigned char* p = buffer + *bufferpos;
	
	if (*bufferpos+1 > buffersize)
	{
		perror("buffer out of bounds");
		exit(1);
	}
	*bufferpos += 1;
	return p[0];
}

unsigned char* buffer_readdata(unsigned char* buffer, unsigned int* bufferpos, unsigned int buffersize, unsigned int datasize)
{
	unsigned char* p = buffer + *bufferpos;
	if (*bufferpos+datasize > buffersize)
	{
		perror("buffer out of bounds");
		exit(1);
	}
	*bufferpos += datasize;
	return p;
}


void mssleep(unsigned int millisecs)
{
#ifdef _WIN32
   Sleep(millisecs);
#else
	usleep(millisecs * 1000);
#endif
}

#ifdef _WIN32

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 

 
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
// Define a structure to receive the current Windows filetime
  FILETIME ft;
 
// Initialize the present time to 0 and the timezone to UTC
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;
 
  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);
 
// The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
// intervals since Jan 1, 1601 in a structure. Copy the high bits to 
// the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
// Convert to microseconds by dividing by 10
    tmpres /= 10;
 
// The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
// in seconds from Jan 1 1601.
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
 
// Finally change microseconds to seconds and place in the seconds value. 
// The modulus picks up the microseconds.
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

#if 0
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
  
// Adjust for the timezone west of Greenwich
      tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
#endif
  return 0;
}
#endif


int kbhit (void)
{
   struct timeval tv;
	fd_set rdfs;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	FD_ZERO(&rdfs);
	FD_SET (STDIN_FILENO, &rdfs);
	
	select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &rdfs);	
}


void changeterminal(int dir)
{
	static struct termios oldt, newt;
	
	if ( dir == 1 )
	{
		tcgetattr( STDIN_FILENO, &oldt);
		//newt = oldt;
		//newt.c_lflag &= ~( ICANON | ECHO );
		//tcsetattr( STDIN_FILENO, TCSANOW, &newt);
	}
	else
		tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
}

void memdump(FILE* fout, const char* prefix, const unsigned char* data, unsigned int size)
{
	unsigned int i;
	unsigned int prefixlen = strlen(prefix);
	unsigned int offs = 0;
	unsigned int line = 0;
	while(size)
	{
		unsigned int max = 32;

		if (max > size)
			max = size;

		if (line==0)
			fprintf(fout, "%s", prefix);
		else
			fprintf(fout, "%*s", prefixlen, "");


		for(i=0; i<max; i++)
			fprintf(fout, "%02X ", data[offs+i]);
		fprintf(fout, "\n");
		line++;
		size -= max;
		offs += max;
	}
}

void hexdump(const void *ptr, int buflen)
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
				printf("%02X ", buf[i+j]);
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