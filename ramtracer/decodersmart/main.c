#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define SAMPLESIZE 13

#define VERBOSE_SDRAM (1<<0)
#define VERBOSE_LEQ (1<<1)
#define VERBOSE_GEQ (1<<2)
#define VERBOSE_READS (1<<3)
#define VERBOSE_WRITES (1<<4)
#define VERBOSE_VERIFY (1<<5)
#define VERBOSE_ADDRESS (1<<6)
#define VERBOSE_COMPACT (1<<7)

typedef struct
{
	unsigned int type;
	unsigned int curaddress;
	unsigned int orgaddress;
	unsigned int size;
	unsigned int capacity;
	unsigned char* buffer;
} rwaccumulator;

typedef struct
{
	unsigned int active;
	unsigned int row;
	unsigned int column;
	unsigned int bank;
	unsigned int cs;
	unsigned int address;
} entryrwqueue;

typedef struct
{
	unsigned char* memory;
	unsigned int state;
	rwaccumulator acc;
	entryrwqueue readqueue[7];
	entryrwqueue writequeue[1];
	unsigned int lastcs;
	unsigned int lastrow;
	unsigned int lastcolumn;
	unsigned int lastbank;
	unsigned int bankrowtable[8];
	unsigned int sampleindex;
	unsigned int samplerestsize;
	unsigned char samplerestdata[SAMPLESIZE];
	unsigned int verbose;
	unsigned int verboseleq;
	unsigned int verbosegeq;
	unsigned int verboseaddress;
	unsigned long long stopindex;
} ramcontext;

typedef enum commands
{
	UNKNOWN = 0,
	NOP,
	MSR,
	ACTIVATE,
	PRECHARGE,
	WRITE,
	BST,
	READ
};

typedef enum states
{
	IDLING,
	READING,
	WRITING,
};

unsigned char shuffle(unsigned int order, unsigned char c)
{
	unsigned char out = 0;
	int i;
	for (i = 0; i < 8; i++) {
		out |= ((c >> ((order >> (i*4)) & 0xF)) & 1) << i;
	}
	return out;
}

void fix_data_order(unsigned int *mask, unsigned char *data)
{
	unsigned char input_data[8];

	*mask = ((*mask << 2) | (*mask >> 6)) & 0xFF;

	memcpy(input_data, data, 8);
	data[0] = input_data[6];
	data[1] = input_data[7];
	data[2] = input_data[0];
	data[3] = input_data[1];
	data[4] = input_data[2];
	data[5] = input_data[3];
	data[6] = input_data[4];
	data[7] = input_data[5];
}

unsigned int getle32(unsigned char* p)
{
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}


void rwacc_init(rwaccumulator* acc)
{
	acc->type = ~0;
	acc->curaddress = 0;
	acc->orgaddress = 0;
	acc->size = 0;
	acc->capacity = 0;
	acc->buffer = 0;
}

void rwacc_destroy(rwaccumulator* acc)
{
	free(acc->buffer);
}

int rwacc_sametransaction(rwaccumulator* acc, unsigned int type, unsigned int address)
{
	return (acc->type == type && address == acc->curaddress);
}

#define DUMPWIDTH 32
void rwacc_printascii(unsigned char c)
{
	if (c >= 0x20 && c <= 0x7e)
		printf("%c", c);
	else
		printf(".");
}
void rwacc_dump(rwaccumulator* acc, unsigned int sampleindex)
{
	unsigned int i,j;
	unsigned char data[DUMPWIDTH];
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int address = acc->orgaddress;

	if (acc->size == 0)
		return;



	for(i=0; i<acc->size; i++)
	{
		if (x == 0)
		{
			//if (y == 0)
			{
				if (acc->type == READ)
					printf("RD ");
				else if (acc->type == WRITE)
					printf("WR ");
				printf("%08X", address);
			}
			//else
			//{
			//	printf("   ");
			//	printf("%08X", address);
			//}
		}
		
		data[x] = acc->buffer[i];
		address++;
		printf(" %02X", data[x]);
		x++;
		if (x >= DUMPWIDTH)
		{
			printf("  ");
			for(j=0; j<DUMPWIDTH; j++)
				rwacc_printascii(data[j]);

			printf("  %08x", sampleindex);
			x = 0;
			y++;
			printf("\n");
		}
	}

	if (x)
	{
		for(j=x; j<DUMPWIDTH; j++)
			printf("   ");
		printf("  ");
		for(j=0; j<x; j++)
			rwacc_printascii(data[j]);

		printf("  %08x", sampleindex);
		printf("\n");
	}
}

void rwacc_settransaction(rwaccumulator* acc, unsigned int type, unsigned int address)
{
	acc->type = type;
	acc->orgaddress = address;
	acc->curaddress = address;
	acc->size = 0;
}

void rwacc_addbuffer(rwaccumulator* acc, unsigned char* buffer, unsigned int size)
{
	unsigned int available = acc->capacity - acc->size;
	

	if (available < size)
	{
		unsigned char* newbuffer = 0;
		unsigned int newcapacity = acc->capacity * 2;

		if (newcapacity <= 0)
			newcapacity = 2;

		while( (newcapacity - acc->size) < size )
			newcapacity *= 2;

		
		newbuffer = malloc(newcapacity);

		if (acc->buffer)
		{
			memcpy(newbuffer, acc->buffer, acc->capacity);

			free(acc->buffer);
		}

		acc->capacity = newcapacity;
		acc->buffer = newbuffer;
	}

	memcpy(acc->buffer + acc->size, buffer, size);

	acc->size += size;
	acc->curaddress += size;
}

void rwacc_addsample(rwaccumulator* acc, unsigned int sampleindex, unsigned int type, unsigned int address, unsigned char* data, unsigned int size)
{
	if (!rwacc_sametransaction(acc, type, address))
	{
		rwacc_dump(acc, sampleindex);
		rwacc_settransaction(acc, type, address);
	}

	rwacc_addbuffer(acc, data, size);
}

int processsample(ramcontext* context, unsigned char* sampledata)
{
	unsigned int sampleindex = context->sampleindex;

	unsigned int i;
	unsigned int header;
	unsigned int address;
	unsigned char* data = 0;
	unsigned int mask;
	unsigned int verbose = 0;
	unsigned int isread = 0;
	unsigned int iswrite = 0;
	


	if (context->sampleindex >= context->stopindex)
		return 0;

	verbose = context->verbose;

	if ( (context->verbose & VERBOSE_LEQ) && (context->sampleindex > context->verboseleq))
		verbose = 0;
	if ( (context->verbose & VERBOSE_GEQ) && (context->sampleindex < context->verbosegeq))
		verbose = 0;

	header = sampledata[0];
	address = sampledata[1] | (sampledata[2]<<8) | (sampledata[3]<<16);
	data = sampledata + 4;
	mask = sampledata[12];

	iswrite = (header == 0);
	isread = (header == 1);

	if (header >= 2)
		printf("ERROR @ %d\n", sampleindex);



	fix_data_order(&mask, data);

	if (isread)
	{
		if (verbose & VERBOSE_COMPACT)
			rwacc_addsample(&context->acc, sampleindex, READ, address*8, data, 8);

		if (verbose & VERBOSE_READS)
		{
			printf("% 9d: ", sampleindex);
			printf("READ ");
			for(i=0; i<8; i++)
			{
				if (i != 0)
					printf(".");
				printf("%02X", data[i]);
			}
			printf(" mask=");
			for(i=0; i<8; i++)
				printf("%d", (mask>>(7-i))&1);
			printf(" address=%08x\n", address);
		}

		if (verbose & VERBOSE_VERIFY)
		{
			if (0 != memcmp(context->memory + address*8, data, 8))
			{
				printf("Verify mismatch sample %d, address %08x:\n", sampleindex, address);
				for(i=0; i<8; i++)
					printf("%02X ", context->memory[address*8 + i]);
				printf("vs ");
				for(i=0; i<8; i++)
					printf("%02X ", data[i]);
				printf("\n");
			}
		}
	}
	

	if (iswrite)
	{

		if (verbose & VERBOSE_COMPACT)
		{
			for(i=0; i<8; i++)
			{
				if ((mask & (1<<i)) == 0)
					rwacc_addsample(&context->acc, sampleindex, WRITE, address*8+i, data+i, 1);
			}
		}

		if (verbose & VERBOSE_WRITES)
		{
			printf("% 9d: ", sampleindex);
			printf("WRITE ");
			for(i=0; i<8; i++)
			{
				if (i != 0)
					printf(".");
				printf("%02X", data[i]);
			}
			printf(" mask=");
			for(i=0; i<8; i++)
				printf("%d", (mask>>(7-i))&1);
			printf(" address=%08x\n", address);
		}

		for(i=0; i<8; i++)
		{
			if ((mask & (1<<i)) == 0)
				memcpy(context->memory + address*8 + i, data+i, 1);
		}
	}

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
		   "  -v, --verbose           Be verbose.\n"
		   "      --verbose-leq=x     Sample index must be <= x to be verbose.\n"
		   "      --verbose-geq=x     Sample index must be >= x to be verbose.\n"
		   "      --verbose-address=x Sample must have an address related to reads/writes to be verbose.\n"
		   "      --verbose-verify    Be verbose about verification.\n"
		   "      --verbose-reads     Be verbose about reads.\n"
		   "      --verbose-writes    Be verbose about writes.\n"
		   "      --verbose-sdram     Be verbose about SDRAM commands.\n"
		   "      --verbose-compact   Be verbose about the compact form.\n"
		   "      --stop=x            Stop at sample index x.\n"
		   "  -o, --out=file          Output final memory contents to file\n"
           "\n",
		   argv0);
   exit(1);
}

typedef enum opts
{
	OPT_VERBOSE = 'v',
	OPT_HELP = 'h',
	OPT_OUTPUT_MEMORY = 'o',
	OPT_VERBOSE_LEQ = 256,
	OPT_VERBOSE_GEQ = 257,
	OPT_STOP = 258,
	OPT_VERBOSE_ADDRESS = 259,
	OPT_VERBOSE_SDRAM = 260,
	OPT_VERBOSE_VERIFY = 261,
	OPT_VERBOSE_READS = 262,
	OPT_VERBOSE_WRITES = 263,
	OPT_VERBOSE_COMPACT = 264,
};

int main(int argc, char* argv[])
{
	char* tracefname = 0;
	FILE* ftrace = 0;
	unsigned int i;
	ramcontext context;
	FILE* fmem = 0;
	
	rwacc_init(&context.acc);
	context.memory = malloc(128 * 1024 * 1024);
	if (context.memory == 0)
	{
		printf("error allocating RAM memory\n");
		goto clean;
	}
	context.verbose = 0;
	context.sampleindex = 0;
	context.samplerestsize = 0;
	context.stopindex = ~0;
	context.state = IDLING;
	for(i=0; i<8; i++)
		context.bankrowtable[i] = 0;
	for(i=0; i<7; i++)
	{
		context.readqueue[i].active = 0;
		context.readqueue[i].address = 0;
		context.readqueue[i].row = 0;
		context.readqueue[i].column = 0;
		context.readqueue[i].bank = 0;
	}
	for(i=0; i<1; i++)
	{
		context.writequeue[i].active = 0;
		context.writequeue[i].address = 0;
		context.writequeue[i].row = 0;
		context.writequeue[i].column = 0;
		context.writequeue[i].bank = 0;
	}

	
	while (1) 
	{
		int option_index;
		int c;
		static struct option long_options[] = 
		{
			{"verbose-leq", 1, NULL, OPT_VERBOSE_LEQ},
			{"verbose-geq", 1, NULL, OPT_VERBOSE_GEQ},
			{"verbose-address", 1, NULL, OPT_VERBOSE_ADDRESS},
			{"verbose-sdram", 0, NULL, OPT_VERBOSE_SDRAM},
			{"verbose-verify", 0, NULL, OPT_VERBOSE_VERIFY},
			{"verbose-reads", 0, NULL, OPT_VERBOSE_READS},
			{"verbose-writes", 0, NULL, OPT_VERBOSE_WRITES},
			{"verbose-compact", 0, NULL, OPT_VERBOSE_COMPACT},
			{"stop", 1, NULL, OPT_STOP},
			{"verbose", 0, NULL, OPT_VERBOSE},
			{"help", 0, NULL, OPT_HELP},
			{"out", 1, NULL, OPT_OUTPUT_MEMORY},
			{NULL},
		};

		c = getopt_long(argc, argv, "vho:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{
			case OPT_OUTPUT_MEMORY:
				fmem = fopen(optarg, "wb");
				if (fmem == 0)
					printf("error opening output file\n");
			break;

			case OPT_VERBOSE:
				context.verbose |= VERBOSE_SDRAM | VERBOSE_READS | VERBOSE_WRITES | VERBOSE_VERIFY;
			break;

			case OPT_VERBOSE_SDRAM:
				context.verbose |= VERBOSE_SDRAM;
			break;

			case OPT_VERBOSE_COMPACT:
				context.verbose |= VERBOSE_COMPACT;
			break;

			case OPT_VERBOSE_READS:
				context.verbose |= VERBOSE_READS;
			break;

			case OPT_VERBOSE_WRITES:
				context.verbose |= VERBOSE_WRITES;
			break;

			case OPT_VERBOSE_ADDRESS:
				context.verboseaddress = strtoul(optarg, 0, 0);
				context.verbose |= VERBOSE_ADDRESS;
			break;

			case OPT_VERBOSE_VERIFY:
				context.verbose |= VERBOSE_VERIFY;
			break;

			case OPT_VERBOSE_LEQ:
				context.verboseleq = strtoul(optarg, 0, 0);
				context.verbose |= VERBOSE_LEQ;
			break;

			case OPT_VERBOSE_GEQ:
				context.verbosegeq = strtoul(optarg, 0, 0);
				context.verbose |= VERBOSE_GEQ;
			break;

			case OPT_STOP:
				context.stopindex = strtoul(optarg, 0, 0);
			break;


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


	if (fmem)
	{
		if (fwrite(context.memory, 128 * 1024 * 1024, 1, fmem) != 1)
		{
			printf("error writing memory file\n");
			goto clean;
		}
	}

clean:
	if (ftrace)
		fclose(ftrace);
	if (fmem)
		fclose(fmem);
	free(context.memory);
	rwacc_destroy(&context.acc);
	return 0;
}
