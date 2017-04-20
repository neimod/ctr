#ifndef _FIRM_H_
#define _FIRM_H_

#include "types.h"
#include "info.h"
#include "ctr.h"
#include "filepath.h"
#include "settings.h"


typedef struct
{
	u8 offset[4];
	u8 address[4];
	u8 size[4];
	u8 type[4];
	u8 hash[32];
} firm_sectionheader;




typedef struct
{
	u8 magic[4];
	u8 reserved1[4];
	u8 entrypointarm11[4];
	u8 entrypointarm9[4];
	u8 reserved2[0x30];
	firm_sectionheader section[4];
	u8 signature[0x100];
} firm_header;

typedef struct
{
	FILE* file;
	settings* usersettings;
	u32 offset;
	u32 size;
	firm_header header;
	ctr_sha256_context sha;
	int hashcheck[4];
	int headersigcheck;
} firm_context;

void firm_init(firm_context* ctx);
void firm_set_file(firm_context* ctx, FILE* file);
void firm_set_offset(firm_context* ctx, u32 offset);
void firm_set_size(firm_context* ctx, u32 size);
void firm_set_usersettings(firm_context* ctx, settings* usersettings);
void firm_process(firm_context* ctx, u32 actions);
void firm_print(firm_context* ctx);
void firm_save(firm_context* ctx, u32 index, u32 flags);
int firm_verify(firm_context* ctx, u32 flags);
void firm_signature_verify(firm_context* ctx);

#endif // _FIRM_H_
