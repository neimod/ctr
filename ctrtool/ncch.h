#ifndef _NCCH_H_
#define _NCCH_H_

#include <stdio.h>
#include "types.h"
#include "keyset.h"
#include "filepath.h"
#include "ctr.h"
#include "exefs.h"
#include "exheader.h"


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
	u32 offset;
	filepath exefspath;
	filepath romfspath;
	filepath exheaderpath;
	ctr_ncchheader header;
	ctr_aes_context aes;
	exefs_context exefs;
	exheader_context exheader;
	rsakey2048 ncsdrsakey;
	rsakey2048 ncchrsakey;
	rsakey2048 nccholdrsakey;
	rsakey2048 ncchdlprsakey;
	rsakey2048 crrrsakey;
	int exefshashcheck;
	int romfshashcheck;
	int exheaderhashcheck;
	u32 extractsize;
	u32 extractflags;
} ncch_context;

void ncch_init(ncch_context* ctx);
void ncch_process(ncch_context* ctx, u32 actions);
void ncch_set_offset(ncch_context* ctx, u32 ncchoffset);
void ncch_set_file(ncch_context* ctx, FILE* file);
void ncch_load_keys(ncch_context* ctx, keyset* keys);
void ncch_set_key(ncch_context* ctx, u8 key[16]);
void ncch_set_exefspath(ncch_context* ctx, const char* path);
void ncch_set_exefsdirpath(ncch_context* ctx, const char* path);
void ncch_set_romfspath(ncch_context* ctx, const char* path);
void ncch_set_exheaderpath(ncch_context* ctx, const char* path);
u32 ncch_get_exefs_offset(ncch_context* ctx);
u32 ncch_get_exefs_size(ncch_context* ctx);
u32 ncch_get_romfs_offset(ncch_context* ctx);
u32 ncch_get_romfs_size(ncch_context* ctx);
u32 ncch_get_exheader_offset(ncch_context* ctx);
u32 ncch_get_exheader_size(ncch_context* ctx);
void ncch_print(ncch_context* ctx);
int ncch_signature_verify(const ctr_ncchheader* header, rsakey2048* key);
void ncch_verify_hashes(ncch_context* ctx, u32 flags);
void ncch_save(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_prepare(ncch_context* ctx, u32 type, u32 flags);
int ncch_extract_buffer(ncch_context* ctx, u8* buffer, u32 buffersize, u32* outsize);
#endif // _NCCH_H_
