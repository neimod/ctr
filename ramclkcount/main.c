#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define SAMPLESIZE 2


typedef struct
{
	unsigned int sampleindex;
	unsigned int samplerestsize;
	unsigned char samplerestdata[SAMPLESIZE];
} ramcontext;


int processsample(ramcontext* context, unsigned char* sampledata)
{
	unsigned int sampleindex = context->sampleindex;

	unsigned int cycles = sampledata[0] | (sampledata[1]<<8);

	if (cycles != 5 && cycles != 4)
		printf("%08d: cycles = %d\n", sampleindex, cycles);

	return 1;
}

int processbuffer(ramcontext* context, unsigned char* buffer, unsigned int size)
{
	unsigned int samplecount;
	unsigned int samplecopysize;
	unsigned int i;


	if (context->samplerestsize)
	{
		samplecopysize = SAMPLESIZE - context->samplerestsize;
		if (samplecopysize > size)
			samplecopysize = size;

		memcpy(context->samplerestdata + context->samplerestsize, buffer, samplecopysize);

		context->samplerestsize += samplecopysize;
		buffer += samplecopysize;
		size -= samplecopysize;

		if (context->samplerestsize == SAMPLESIZE)
		{
			if (0 == processsample(context, context->samplerestdata))
				return 0;
			context->sampleindex++;
			context->samplerestsize = 0;
		}
	}

	if (size)
	{
		samplecount = size / SAMPLESIZE;

		for(i=0; i<samplecount; i++)
		{
			if (0 == processsample(context, buffer + i*SAMPLESIZE))
				return 0;
			context->sampleindex++;
		}

		context->samplerestsize = size - (samplecount * SAMPLESIZE);
		memcpy(context->samplerestdata, buffer + samplecount*SAMPLESIZE, context->samplerestsize);
	}

	return 1;
}

int traceram(ramcontext* context, FILE* f)
{
	unsigned int inbuffersize = 64 * 1024;
	unsigned int outbuffersize = 64 * 1024;
	unsigned char* inbuffer = 0;
	unsigned char* outbuffer = 0;
	z_stream stream;
	int result;
	unsigned int have;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
	result = inflateInit(&stream);
    if (result != Z_OK)
		goto clean;

	inbuffer = malloc(inbuffersize);
	outbuffer = malloc(outbuffersize);

	do
	{
		stream.avail_in = fread(inbuffer, 1, inbuffersize, f);
		stream.next_in = inbuffer;
		if (stream.avail_in == 0)
			break;

		do
		{
			stream.avail_out = outbuffersize;
			stream.next_out = outbuffer;

			result = inflate(&stream, Z_NO_FLUSH);

			switch(result)
			{
				case Z_NEED_DICT:
					result = Z_DATA_ERROR;
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					goto clean;
			}

			have = outbuffersize - stream.avail_out;
			if (0 == processbuffer(context, outbuffer, have))
			{
				result = Z_OK;
				goto clean;
			}
		} while(stream.avail_out == 0);

	} while(result != Z_STREAM_END);

	if (result == Z_STREAM_END)
		result = Z_OK;

clean:
	inflateEnd(&stream);
	free(outbuffer);
	free(inbuffer);
	return result;
}

static void usage(const char *argv0)
{
	fprintf(stderr,
		   "ramtracer -- neimod\n"
		   "Usage: %s [options...] <trace file>\n"
           "\n"
           "Options:\n"
           "  -h, --help              Print this help info.\n"
           "\n",
		   argv0);
   exit(1);
}

typedef enum opts
{
	OPT_HELP = 'h',
};

int main(int argc, char* argv[])
{
	char* tracefname = 0;
	FILE* ftrace = 0;
	ramcontext context;
	
	context.sampleindex = 0;
	context.samplerestsize = 0;
	
	while (1) 
	{
		int option_index;
		int c;
		static struct option long_options[] = 
		{
			{"help", 0, NULL, OPT_HELP},
			{NULL},
		};

		c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{
			case OPT_HELP:
				usage(argv[0]);
			break;

			default:
				usage(argv[0]);
		}
	}
	if ( optind == argc - 1)
	{
		// Exactly one extra argument -- the trace file
		tracefname = argv[optind];
	}
	else if ( (optind < argc) || (argc == 1) )
	{
		// Too many extra args
		usage(argv[0]);
	}

	if (tracefname == 0)
	{
		printf("error expected trace file\n");
		goto clean;
	}

	ftrace = fopen(tracefname, "rb");
	if (ftrace == 0)
	{
		printf("error opening file\n");
		goto clean;
	}

	if (Z_OK != traceram(&context, ftrace))
	{
		printf("error processing file\n");
		goto clean;
	}



clean:
	if (ftrace)
		fclose(ftrace);
	return 0;
}
