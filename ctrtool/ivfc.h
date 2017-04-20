#ifndef __IVFC_H__
#define __IVFC_H__

#include "types.h"
#include "settings.h"

#define IVFC_MAX_LEVEL 4
#define IVFC_MAX_BUFFERSIZE 0x4000

typedef struct
{
	u8 magic[4];
	u8 id[4];
} ivfc_header;

typedef struct
{
	u8 logicaloffset[8];
	u8 hashdatasize[8];
	u8 blocksize[4];
	u8 reserved[4];
} ivfc_levelheader;

typedef struct
{
	u64 dataoffset;
	u64 datasize;
	u64 hashoffset;
	u32 hashblocksize;
	int hashcheck;
} ivfc_level;

typedef struct
{
	u8 masterhashsize[4];
	ivfc_levelheader level1;
	ivfc_levelheader level2;
	ivfc_levelheader level3;
	u8 reserved[4];
	u8 optionalsize[4];
} ivfc_header_romfs;

typedef struct
{
	FILE* file;
	u32 offset;
	u32 size;
	settings* usersettings;

	ivfc_header header;
	ivfc_header_romfs romfsheader;

	u32 levelcount;
	ivfc_level level[IVFC_MAX_LEVEL];
	u64 bodyoffset;
	u64 bodysize;
	u8 buffer[IVFC_MAX_BUFFERSIZE];
} ivfc_context;

void ivfc_init(ivfc_context* ctx);
void ivfc_process(ivfc_context* ctx, u32 actions);
void ivfc_set_offset(ivfc_context* ctx, u32 offset);
void ivfc_set_size(ivfc_context* ctx, u32 size);
void ivfc_set_file(ivfc_context* ctx, FILE* file);
void ivfc_set_usersettings(ivfc_context* ctx, settings* usersettings);
void ivfc_verify(ivfc_context* ctx, u32 flags);
void ivfc_print(ivfc_context* ctx);

void ivfc_read(ivfc_context* ctx, u32 offset, u32 size, u8* buffer);
void ivfc_hash(ivfc_context* ctx, u32 offset, u32 size, u8* hash);

#endif // __IVFC_H__
