#ifndef _LZSS_H_
#define _LZSS_H_

#include "types.h"
#include "settings.h"

typedef struct
{
	FILE* file;
	u32 offset;
	u32 size;
	settings* usersettings;
} lzss_context;

void lzss_init(lzss_context* ctx);
void lzss_process(lzss_context* ctx, u32 actions);
void lzss_set_offset(lzss_context* ctx, u32 offset);
void lzss_set_size(lzss_context* ctx, u32 size);
void lzss_set_file(lzss_context* ctx, FILE* file);
void lzss_set_usersettings(lzss_context* ctx, settings* usersettings);

u32 lzss_get_decompressed_size(u8* compressed, u32 compressedsize);
int lzss_decompress(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize);


#endif // _LZSS_H_
