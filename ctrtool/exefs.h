#ifndef _EXEFS_H_
#define _EXEFS_H_

#include "types.h"
#include "info.h"
#include "ctr.h"
#include "filepath.h"
#include "settings.h"


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
	u8 hashes[8][0x20];
} exefs_header;

typedef struct
{
	FILE* file;
	settings* usersettings;
	u8 partitionid[8];
	u8 counter[16];
	u8 key[16];
	u32 offset;
	u32 size;
	exefs_header header;
	ctr_aes_context aes;
	ctr_sha256_context sha;
	int hashcheck[8];
	int compressedflag;
	int encrypted;
} exefs_context;

void exefs_init(exefs_context* ctx);
void exefs_set_file(exefs_context* ctx, FILE* file);
void exefs_set_offset(exefs_context* ctx, u32 offset);
void exefs_set_size(exefs_context* ctx, u32 size);
void exefs_set_usersettings(exefs_context* ctx, settings* usersettings);
void exefs_set_partitionid(exefs_context* ctx, u8 partitionid[8]);
void exefs_set_counter(exefs_context* ctx, u8 counter[16]);
void exefs_set_compressedflag(exefs_context* ctx, int compressedflag);
void exefs_set_key(exefs_context* ctx, u8 key[16]);
void exefs_set_encrypted(exefs_context* ctx, u32 encrypted);
void exefs_read_header(exefs_context* ctx, u32 flags);
void exefs_calculate_hash(exefs_context* ctx, u8 hash[32]);
void exefs_process(exefs_context* ctx, u32 actions);
void exefs_print(exefs_context* ctx);
void exefs_save(exefs_context* ctx, u32 index, u32 flags);
int exefs_verify(exefs_context* ctx, u32 index, u32 flags);
void exefs_determine_key(exefs_context* ctx, u32 actions);
#endif // _EXEFS_H_
