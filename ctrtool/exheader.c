#include <stdio.h>
#include <string.h>

#include "types.h"
#include "exheader.h"
#include "utils.h"
#include "ncch.h"

void exheader_init(exheader_context* ctx)
{
	memset(ctx, 0, sizeof(exheader_context));
}

void exheader_set_file(exheader_context* ctx, FILE* file)
{
	ctx->file = file;
}

void exheader_set_offset(exheader_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void exheader_set_size(exheader_context* ctx, u32 size)
{
	ctx->size = size;
}

void exheader_set_key(exheader_context* ctx, u8 key[16])
{
	memcpy(ctx->key, key, 16);
}

void exheader_set_partitionid(exheader_context* ctx, u8 partitionid[8])
{
	memcpy(ctx->partitionid, partitionid, 8);
}

void exheader_set_programid(exheader_context* ctx, u8 programid[8])
{
	memcpy(ctx->programid, programid, 8);
}


int exheader_process(exheader_context* ctx, u32 actions)
{
	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(exheader_header), ctx->file);

	ctr_init_ncch(&ctx->aes, ctx->key, ctx->partitionid, NCCHTYPE_EXTHEADER);
	ctr_crypt_counter(&ctx->aes, (u8*)&ctx->header, (u8*)&ctx->header, sizeof(exheader_header));

	if (memcmp(ctx->header.programid, ctx->programid, 8))
	{
		fprintf(stderr, "Error, program id mismatch. Wrong key?\n");
		return 0;
	}

	if (actions & InfoFlag)
	{
		exheader_print(ctx);		
	}

	return 1;
}

void exheader_print(exheader_context* ctx)
{
}
