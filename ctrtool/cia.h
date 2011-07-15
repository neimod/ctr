#ifndef _CIA_H_
#define _CIA_H_

#include "types.h"
#include "filepath.h"
#include "tik.h"
#include "tmd.h"
#include "ctr.h"

typedef enum
{
	CIATYPE_CERTS,
	CIATYPE_TMD,
	CIATYPE_TIK,
	CIATYPE_CONTENT,
	CIATYPE_META,
} cia_types;

typedef struct
{
	u8 headersize[4];
	u8 type[2];
	u8 version[2];
	u8 certsize[4];
	u8 ticketsize[4];
	u8 tmdsize[4];
	u8 metasize[4];
	u8 contentsize[8];
	u8 contentindex[0x2000];
} ctr_ciaheader;

typedef struct
{
	FILE* file;
	u32 offset;
	u8 key[16];
	u8 titlekey[16];
	u8 iv[16];
	int keyvalid;
	ctr_ciaheader header;
	filepath certspath;
	filepath contentpath;
	filepath tikpath;
	filepath tmdpath;
	filepath metapath;
	ctr_aes_context aes;

	tik_context tik;
	tmd_context tmd;

	u32 sizeheader;
	u32 sizecert;
	u32 sizetik;
	u32 sizetmd;
	u32 sizecontent;
	u32 sizemeta;
	
	u32 offsetcerts;
	u32 offsettik;
	u32 offsettmd;
	u32 offsetcontent;
	u32 offsetmeta;
} cia_context;

void cia_init(cia_context* ctx);
void cia_set_file(cia_context* ctx, FILE* file);
void cia_set_offset(cia_context* ctx, u32 offset);
void cia_set_commonkey(cia_context* ctx, u8 key[16]);
void cia_set_certspath(cia_context* ctx, const char* path);
void cia_set_contentpath(cia_context* ctx, const char* path);
void cia_set_tikpath(cia_context* ctx, const char* path);
void cia_set_tmdpath(cia_context* ctx, const char* path);
void cia_set_metapath(cia_context* ctx, const char* path);
void cia_print(cia_context* ctx);
void cia_save(cia_context* ctx, u32 type, u32 flags);
void cia_process(cia_context* ctx, u32 actions);

#endif // _CIA_H_
