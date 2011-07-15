#include <stdio.h>
#include <string.h>

#include "types.h"
#include "exefs.h"
#include "utils.h"
#include "ncch.h"

void exefs_init(exefs_context* ctx)
{
	memset(ctx, 0, sizeof(exefs_context));
	filepath_init(&ctx->dirpath);
}

void exefs_set_file(exefs_context* ctx, FILE* file)
{
	ctx->file = file;
}

void exefs_set_offset(exefs_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void exefs_set_size(exefs_context* ctx, u32 size)
{
	ctx->size = size;
}

void exefs_set_key(exefs_context* ctx, u8 key[16])
{
	memcpy(ctx->key, key, 16);
}

void exefs_set_dirpath(exefs_context* ctx, const char* path)
{
	filepath_set(&ctx->dirpath, path);
}

void exefs_set_partitionid(exefs_context* ctx, u8 partitionid[8])
{
	memcpy(ctx->partitionid, partitionid, 8);
}

void exefs_save(exefs_context* ctx, u32 index, u32 flags)
{
	exefs_sectionheader* section = (exefs_sectionheader*)(ctx->header.section + index);
	char outfname[MAX_PATH];
	char name[64];
	u32 offset;
	u32 size;
	FILE* fout;
	u8 buffer[16 * 1024];

	offset = getle32(section->offset) + sizeof(exefs_header);
	size = getle32(section->size);

	if (size == 0)
		return;

	if (size >= ctx->size)
	{
		fprintf(stderr, "Error, ExeFS section %d size invalid\n", index);
		return;
	}

	memset(name, 0, sizeof(name));
	memcpy(name, section->name, 8);

	memcpy(outfname, ctx->dirpath.pathname, MAX_PATH);
	strcat(outfname, "/");

	if (name[0] == '.')
		strcat(outfname, name+1);
	else
		strcat(outfname, name);
	strcat(outfname, ".bin");

	fout = fopen(outfname, "wb");

	if (fout == 0)
	{
		fprintf(stderr, "Error, failed to create file %s\n", outfname);
		goto clean;
	}
	

	fprintf(stdout, "Saving section %s to %s...\n", name, outfname);
	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	ctr_init_ncch(&ctx->aes, ctx->key, ctx->partitionid, NCCHTYPE_EXEFS);
	ctr_add_counter(&ctx->aes, offset / 0x10);


	while(size)
	{
		u32 max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->file))
		{
			fprintf(stdout, "Error reading input file\n");
			goto clean;
		}

		if (0 == (flags & PlainFlag))
			ctr_crypt_counter(&ctx->aes, buffer, buffer, max);

		if (max != fwrite(buffer, 1, max, fout))
		{
			fprintf(stdout, "Error writing output file\n");
			goto clean;
		}

		size -= max;
	}	

clean:
	return;
}

void exefs_process(exefs_context* ctx, u32 actions)
{
	u32 i;


	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(exefs_header), ctx->file);

	ctr_init_ncch(&ctx->aes, ctx->key, ctx->partitionid, NCCHTYPE_EXEFS);
	ctr_crypt_counter(&ctx->aes, (u8*)&ctx->header, (u8*)&ctx->header, sizeof(exefs_header));

	if (actions & VerifyFlag)
	{
		for(i=0; i<8; i++)
			ctx->hashcheck[i] = exefs_verify(ctx, i, actions)? 1 : 2;
	}

	if (actions & InfoFlag)
	{
		exefs_print(ctx);
	}

	if (actions & ExtractFlag && ctx->dirpath.valid)
	{
		makedir(ctx->dirpath.pathname);
		for(i=0; i<8; i++)
			exefs_save(ctx, i, actions);
	}
}

int exefs_verify(exefs_context* ctx, u32 index, u32 flags)
{
	exefs_sectionheader* section = (exefs_sectionheader*)(ctx->header.section + index);
	u32 offset;
	u32 size;
	u8 buffer[16 * 1024];
	u8 hash[0x20];
	

	offset = getle32(section->offset) + sizeof(exefs_header);
	size = getle32(section->size);

	if (size == 0)
		return 0;

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	ctr_init_ncch(&ctx->aes, ctx->key, ctx->partitionid, NCCHTYPE_EXEFS);
	ctr_add_counter(&ctx->aes, offset / 0x10);

	ctr_sha_256_init(&ctx->sha);

	while(size)
	{
		u32 max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->file))
		{
			fprintf(stdout, "Error reading input file\n");
			goto clean;
		}

		if (0 == (flags & PlainFlag))
			ctr_crypt_counter(&ctx->aes, buffer, buffer, max);

		ctr_sha_256_update(&ctx->sha, buffer, max);

		size -= max;
	}	

	ctr_sha_256_finish(&ctx->sha, hash);

	memdump(stdout, "Hash: ", hash, 32);
	if (memcmp(hash, ctx->header.hashes[7-index], 0x20) == 0)
		return 1;
clean:
	return 0;
}

void exefs_print(exefs_context* ctx)
{
	u32 i;
	char sectname[9];
	u32 sectoffset;
	u32 sectsize;

	fprintf(stdout, "\nExeFS:\n");
	for(i=0; i<8; i++)
	{
		exefs_sectionheader* section = (exefs_sectionheader*)(ctx->header.section + i);


		memset(sectname, 0, sizeof(sectname));
		memcpy(sectname, section->name, 8);

		sectoffset = getle32(section->offset);
		sectsize = getle32(section->size);

		if (sectsize)
		{
			fprintf(stdout, "Section name:           %s\n", sectname);
			fprintf(stdout, "Section offset:         0x%08x\n", sectoffset + 0x200);
			fprintf(stdout, "Section size:           0x%08x\n", sectsize);
			if (ctx->hashcheck[i] == HashGood)
				memdump(stdout, "Section hash (GOOD):    ", ctx->header.hashes[7-i], 0x20);
			else if (ctx->hashcheck[i] == HashFail)
				memdump(stdout, "Section hash (FAIL):    ", ctx->header.hashes[7-i], 0x20);
			else
				memdump(stdout, "Section hash:           ", ctx->header.hashes[7-i], 0x20);
		}
	}
}
