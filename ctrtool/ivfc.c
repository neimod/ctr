#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "utils.h"
#include "ivfc.h"
#include "ctr.h"

void ivfc_init(ivfc_context* ctx)
{
	memset(ctx, 0, sizeof(ivfc_context));
}

void ivfc_set_usersettings(ivfc_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void ivfc_set_offset(ivfc_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void ivfc_set_size(ivfc_context* ctx, u32 size)
{
	ctx->size = size;
}

void ivfc_set_file(ivfc_context* ctx, FILE* file)
{
	ctx->file = file;
}


void ivfc_process(ivfc_context* ctx, u32 actions)
{


	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(ivfc_header), ctx->file);

	if (getle32(ctx->header.magic) != MAGIC_IVFC)
	{
		fprintf(stdout, "Error, IVFC segment corrupted\n");
		return;
	}

	if (getle32(ctx->header.id) == 0x10000)
	{
		fread(&ctx->romfsheader, 1, sizeof(ivfc_header_romfs), ctx->file);

		ctx->levelcount = 3;

		ctx->level[2].hashblocksize = 1 << getle32(ctx->romfsheader.level3.blocksize);
		ctx->level[1].hashblocksize = 1 << getle32(ctx->romfsheader.level2.blocksize);
		ctx->level[0].hashblocksize = 1 << getle32(ctx->romfsheader.level1.blocksize);
		ctx->level[0].hashoffset = 0x60;

		ctx->bodyoffset = align64(ctx->level[0].hashoffset + getle32(ctx->romfsheader.masterhashsize), ctx->level[2].hashblocksize);
		ctx->bodysize = getle64(ctx->romfsheader.level3.hashdatasize);
		
		ctx->level[2].dataoffset = ctx->bodyoffset;
		ctx->level[2].datasize = align64(ctx->bodysize, ctx->level[2].hashblocksize);

		ctx->level[1].hashoffset = align64(ctx->bodyoffset + ctx->bodysize, ctx->level[2].hashblocksize);
		ctx->level[2].hashoffset = ctx->level[1].hashoffset + getle64(ctx->romfsheader.level2.logicaloffset) - getle64(ctx->romfsheader.level1.logicaloffset);

		ctx->level[1].dataoffset = ctx->level[2].hashoffset;
		ctx->level[1].datasize = align64(getle64(ctx->romfsheader.level2.hashdatasize), ctx->level[1].hashblocksize);

		ctx->level[0].dataoffset = ctx->level[1].hashoffset;
		ctx->level[0].datasize = align64(getle64(ctx->romfsheader.level1.hashdatasize), ctx->level[0].hashblocksize);
	}

	if (actions & VerifyFlag)
		ivfc_verify(ctx, actions);

	if (actions & InfoFlag)
		ivfc_print(ctx);		

}

void ivfc_verify(ivfc_context* ctx, u32 flags)
{
	u32 i, j;
	u32 blockcount;

	for(i=0; i<ctx->levelcount; i++)
	{
		ivfc_level* level = ctx->level + i;

		level->hashcheck = Fail;
	}

	for(i=0; i<ctx->levelcount; i++)
	{
		ivfc_level* level = ctx->level + i;

		blockcount = level->datasize / level->hashblocksize;
		if (blockcount * level->hashblocksize != level->datasize)
		{
			fprintf(stderr, "Error, IVFC block size mismatch\n");
			return;
		}

		level->hashcheck = Good;

		for(j=0; j<blockcount; j++)
		{
			u8 calchash[32];
			u8 testhash[32];

			
			ivfc_hash(ctx, level->dataoffset + level->hashblocksize * j, level->hashblocksize, calchash);
			ivfc_read(ctx, level->hashoffset + 0x20 * j, 0x20, testhash);

			if (memcmp(calchash, testhash, 0x20) != 0)
				level->hashcheck = Fail;
		}
	}
}

void ivfc_read(ivfc_context* ctx, u32 offset, u32 size, u8* buffer)
{
	if ( (offset > ctx->size) || (offset+size > ctx->size) )
	{
		fprintf(stderr, "Error, IVFC offset out of range (offset=0x%08x, size=0x%08x)\n", offset, size);
		return;
	}

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	if (size != fread(buffer, 1, size, ctx->file))
	{
		fprintf(stderr, "Error, IVFC could not read file\n");
		return;
	}
}

void ivfc_hash(ivfc_context* ctx, u32 offset, u32 size, u8* hash)
{
	if (size > IVFC_MAX_BUFFERSIZE)
	{
		fprintf(stderr, "Error, IVFC hash block size too big.\n");
		return;
	}

	ivfc_read(ctx, offset, size, ctx->buffer);

	ctr_sha_256(ctx->buffer, size, hash);
}

void ivfc_print(ivfc_context* ctx)
{
	u32 i;
	ivfc_header* header = &ctx->header;

	fprintf(stdout, "\nIVFC:\n");

	fprintf(stdout, "Header:                 %c%c%c%c\n", header->magic[0], header->magic[1], header->magic[2], header->magic[3]);
	fprintf(stdout, "Id:                     %08x\n", getle32(header->id));

	for(i=0; i<ctx->levelcount; i++)
	{
		ivfc_level* level = ctx->level + i;

		fprintf(stdout, "\n");
		if (level->hashcheck == Unchecked)
			fprintf(stdout, "Level %d:               \n", i);
		else
			fprintf(stdout, "Level %d (%s):          \n", i, level->hashcheck == Good? "GOOD" : "FAIL");
		fprintf(stdout, " Data offset:           0x%016llx\n", ctx->offset + level->dataoffset);
		fprintf(stdout, " Data size:             0x%016llx\n", level->datasize);
		fprintf(stdout, " Hash offset:           0x%016llx\n", ctx->offset + level->hashoffset);
		fprintf(stdout, " Hash block size:       0x%08x\n", level->hashblocksize);
	}
}

u64 ivfc_get_body_offset(ivfc_context* ctx)
{
	return ctx->bodyoffset;
}

u64 ivfc_get_body_size(ivfc_context* ctx)
{
	return ctx->bodysize;
}

