#ifndef _EXEFS_H_
#define _EXEFS_H_

#include "types.h"

typedef struct
{
	u8 name[8];
	u8 offset[4];
	u8 size[4];
} exefs_sectionheader;


typedef struct
{
	exefs_sectionheader section[8];
	u8 reserved[0x80];
	u8 hashes[0x20 * 8];
} exefs_superblock;

void exefs_print(const u8 *blob, u32 size);


#endif // _EXEFS_H_
