#ifndef _LZSS_H_
#define _LZSS_H_

#include "types.h"

u32 lzss_get_decompressed_size(u8* compressed, u32 compressedsize);
int lzss_decompress(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize);


#endif // _LZSS_H_
