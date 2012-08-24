#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "types.h"
#include "stream.h"


void stream_in_init(stream_in_context* ctx)
{
	memset(ctx, 0, sizeof(stream_in_context));
}

void stream_out_init(stream_out_context* ctx)
{
	memset(ctx, 0, sizeof(stream_out_context));
}

void stream_in_allocate(stream_in_context* ctx, u32 buffersize, FILE* file)
{
	ctx->inbuffer = malloc(buffersize);
	ctx->inbuffersize = buffersize;
	ctx->infile = file;
	ctx->infileposition = 0;
}

void stream_out_allocate(stream_out_context* ctx, u32 buffersize, FILE* file)
{
	ctx->outbuffer = malloc(buffersize);
	ctx->outbuffersize = buffersize;
	ctx->outfile = file;
}

void stream_in_destroy(stream_in_context* ctx)
{
	free(ctx->inbuffer);
	ctx->inbuffer = 0;
}

void stream_out_destroy(stream_out_context* ctx)
{
	free(ctx->outbuffer);
	ctx->outbuffer = 0;
}

int stream_in_byte(stream_in_context* ctx, u8* byte)
{
	if (ctx->inbufferpos >= ctx->inbufferavailable)
	{
		size_t readbytes = fread(ctx->inbuffer, 1, ctx->inbuffersize, ctx->infile);
		if (readbytes <= 0)
			return 0;

		ctx->inbufferavailable = readbytes;
		ctx->inbufferpos = 0;
		ctx->infileposition += readbytes;
	}

	*byte = ctx->inbuffer[ctx->inbufferpos++];
	return 1;
}

void stream_in_seek(stream_in_context* ctx, u32 position)
{
	fseek(ctx->infile, position, SEEK_SET);
	ctx->infileposition = position;
	ctx->inbufferpos = 0;
	ctx->inbufferavailable = 0;
}

void stream_in_reseek(stream_in_context* ctx)
{
	fseek(ctx->infile, ctx->infileposition, SEEK_SET);
}


void stream_out_seek(stream_out_context* ctx, u32 position)
{
	stream_out_flush(ctx);

	fseek(ctx->outfile, position, SEEK_SET);
}

void stream_out_skip(stream_out_context* ctx, u32 size)
{
	stream_out_flush(ctx);

	fseek(ctx->outfile, size, SEEK_CUR);
}

int stream_out_byte(stream_out_context* ctx, u8 byte)
{
	if (ctx->outbufferpos >= ctx->outbuffersize)
	{
		if (stream_out_flush(ctx) == 0)
			return 0;
	}

	ctx->outbuffer[ctx->outbufferpos++] = byte;
	return 1;
}

int stream_out_buffer(stream_out_context* ctx, const void* buffer, u32 size)
{
	u32 i;

	for(i=0; i<size; i++)
	{
		if (!stream_out_byte(ctx, ((u8*)buffer)[i]))
			return 0;
	}

	return 1;
}

int stream_out_flush(stream_out_context* ctx)
{
	if (ctx->outbufferpos > 0)
	{
		size_t writtenbytes = fwrite(ctx->outbuffer, 1, ctx->outbufferpos, ctx->outfile);
		if (writtenbytes < 0)
			return 0;


		ctx->outbufferpos = 0;
	}
	return 1;
}

void stream_out_position(stream_out_context* ctx, u32* position)
{
	stream_out_flush(ctx);

	*position = ftell(ctx->outfile);
}


