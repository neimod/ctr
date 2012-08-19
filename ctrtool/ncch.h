#ifndef _NCCH_H_
#define _NCCH_H_

#include <stdio.h>
#include "types.h"
#include "keyset.h"
#include "filepath.h"
#include "ctr.h"
#include "exefs.h"
#include "exheader.h"
#include "settings.h"

typedef enum
{
	NCCHTYPE_EXHEADER = 1,
	NCCHTYPE_EXEFS = 2,
	NCCHTYPE_ROMFS = 3,
} ctr_ncchtypes;

typedef struct
{
	u8 signature[0x100];
	u8 magic[4];
	u8 contentsize[4];
	u8 partitionid[8];
	u8 makercode[2];
	u8 version[2];
	u8 reserved0[4];
	u8 programid[8];
	u8 tempflag;
	u8 reserved1[0x2f];
	u8 productcode[0x10];
	u8 extendedheaderhash[0x20];
	u8 extendedheadersize[4];
	u8 reserved2[4];
	u8 flags[8];
	u8 plainregionoffset[4];
	u8 plainregionsize[4];
	u8 reserved3[8];
	u8 exefsoffset[4];
	u8 exefssize[4];
	u8 exefshashregionsize[4];
	u8 reserved4[4];
	u8 romfsoffset[4];
	u8 romfssize[4];
	u8 romfshashregionsize[4];
	u8 reserved5[4];
	u8 exefssuperblockhash[0x20];
	u8 romfssuperblockhash[0x20];
} ctr_ncchheader;


typedef struct
{
	FILE* file;
	u8 key[16];
	u32 encrypted;
	u32 offset;
	u32 size;
	settings* usersettings;
	ctr_ncchheader header;
	ctr_aes_context aes;
	exefs_context exefs;
	exheader_context exheader;
	int exefshashcheck;
	int romfshashcheck;
	int exheaderhashcheck;
	int headersigcheck;
	u32 extractsize;
	u32 extractflags;
} ncch_context;

void ncch_init(ncch_context* ctx);
void ncch_process(ncch_context* ctx, u32 actions);
void ncch_set_offset(ncch_context* ctx, u32 offset);
void ncch_set_size(ncch_context* ctx, u32 size);
void ncch_set_file(ncch_context* ctx, FILE* file);
void ncch_set_usersettings(ncch_context* ctx, settings* usersettings);
u32 ncch_get_exefs_offset(ncch_context* ctx);
u32 ncch_get_exefs_size(ncch_context* ctx);
u32 ncch_get_romfs_offset(ncch_context* ctx);
u32 ncch_get_romfs_size(ncch_context* ctx);
u32 ncch_get_exheader_offset(ncch_context* ctx);
u32 ncch_get_exheader_size(ncch_context* ctx);
void ncch_print(ncch_context* ctx);
int ncch_signature_verify(ncch_context* ctx, rsakey2048* key);
void ncch_verify(ncch_context* ctx, u32 flags);
void ncch_save(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_prepare(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_buffer(ncch_context* ctx, u8* buffer, u32 buffersize, u32* outsize);
u32 ncch_get_mediaunit_size(ncch_context* ctx);
void ncch_get_counter(ncch_context* ctx, u8 counter[16], u8 type);
void ncch_determine_key(ncch_context* ctx, u32 actions);
#endif // _NCCH_H_
