#ifndef __STREAM_H__
#define __STREAM_H__

#include <stdio.h>
#include "types.h"

typedef struct
{
	FILE* infile;
	u32 infileposition;
	u8* inbuffer;
	u32 inbuffersize;
	u32 inbufferavailable;
	u32 inbufferpos;
} stream_in_context;

typedef struct
{
	FILE* outfile;
	u8* outbuffer;
	u32 outbuffersize;
	u32 outbufferpos;
} stream_out_context;

// create/destroy
void stream_in_init(stream_in_context* ctx);
void stream_in_allocate(stream_in_context* ctx, u32 buffersize, FILE* file);
void stream_in_destroy(stream_in_context* ctx);
void stream_out_init(stream_out_context* ctx);
void stream_out_allocate(stream_out_context* ctx, u32 buffersize, FILE* file);
void stream_out_destroy(stream_out_context* ctx);

// read/write operations
int  stream_in_byte(stream_in_context* ctx, u8* byte);
void stream_in_seek(stream_in_context* ctx, u32 position);
void stream_in_reseek(stream_in_context* ctx);

int  stream_out_byte(stream_out_context* ctx, u8 byte);
int  stream_out_buffer(stream_out_context* ctx, const void* buffer, u32 size);
int  stream_out_flush(stream_out_context* ctx);
void stream_out_seek(stream_out_context* ctx, u32 position);
void stream_out_skip(stream_out_context* ctx, u32 size);
void stream_out_position(stream_out_context* ctx, u32* position);

#endif // __STREAM_H__
