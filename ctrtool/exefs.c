#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "exefs.h"
#include "utils.h"
#include "ncch.h"
#include "lzss.h"

void exefs_init(exefs_context* ctx)
{
	memset(ctx, 0, sizeof(exefs_context));
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

void exefs_set_usersettings(exefs_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void exefs_set_partitionid(exefs_context* ctx, u8 partitionid[8])
{
	memcpy(ctx->partitionid, partitionid, 8);
}

void exefs_set_compressedflag(exefs_context* ctx, int compressedflag)
{
	ctx->compressedflag = compressedflag;
}

void exefs_set_encrypted(exefs_context* ctx, u32 encrypted)
{
	ctx->encrypted = encrypted;
}

void exefs_set_key(exefs_context* ctx, u8 key[16])
{
	memcpy(ctx->key, key, 16);
}

void exefs_set_counter(exefs_context* ctx, u8 counter[16])
{
	memcpy(ctx->counter, counter, 16);
}

void exefs_determine_key(exefs_context* ctx, u32 actions)
{
	u8* key = settings_get_ncch_key(ctx->usersettings);

	if (actions & PlainFlag)
		ctx->encrypted = 0;
	else
	{
		if (key)
		{
			ctx->encrypted = 1;
			memcpy(ctx->key, key, 0x10);
		}
	}
}

void exefs_save(exefs_context* ctx, u32 index, u32 flags)
{
	exefs_sectionheader* section = (exefs_sectionheader*)(ctx->header.section + index);
	char outfname[MAX_PATH];
	char name[64];
	u32 offset;
	u32 size;
	FILE* fout;
	u32 compressedsize = 0;
	u32 decompressedsize = 0;
	u8* compressedbuffer = 0;
	u8* decompressedbuffer = 0;
	filepath* dirpath = 0;
	
	
	offset = getle32(section->offset) + sizeof(exefs_header);
	size = getle32(section->size);
	dirpath = settings_get_exefs_dir_path(ctx->usersettings);

	if (size == 0 || dirpath == 0 || dirpath->valid == 0)
		return;

	if (size >= ctx->size)
	{
		fprintf(stderr, "Error, ExeFS section %d size invalid\n", index);
		return;
	}

	memset(name, 0, sizeof(name));
	memcpy(name, section->name, 8);

	
	memcpy(outfname, dirpath->pathname, MAX_PATH);
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
	
	

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	ctr_init_counter(&ctx->aes, ctx->key, ctx->counter);
	ctr_add_counter(&ctx->aes, offset / 0x10);

	if (index == 0 && ctx->compressedflag && ((flags & RawFlag) == 0))
	{
		fprintf(stdout, "Decompressing section %s to %s...\n", name, outfname);

		compressedsize = size;
		compressedbuffer = malloc(compressedsize);

		if (compressedbuffer == 0)
		{
			fprintf(stdout, "Error allocating memory\n");
			goto clean;
		}
		if (compressedsize != fread(compressedbuffer, 1, compressedsize, ctx->file))
		{
			fprintf(stdout, "Error reading input file\n");
			goto clean;
		}

		if (ctx->encrypted)
			ctr_crypt_counter(&ctx->aes, compressedbuffer, compressedbuffer, compressedsize);


		decompressedsize = lzss_get_decompressed_size(compressedbuffer, compressedsize);
		decompressedbuffer = malloc(decompressedsize);
		if (decompressedbuffer == 0)
		{
			fprintf(stdout, "Error allocating memory\n");
			goto clean;
		}

		if (0 == lzss_decompress(compressedbuffer, compressedsize, decompressedbuffer, decompressedsize))
			goto clean;

		if (decompressedsize != fwrite(decompressedbuffer, 1, decompressedsize, fout))
		{
			fprintf(stdout, "Error writing output file\n");
			goto clean;
		}		
	}
	else
	{
		u8 buffer[16 * 1024];

		fprintf(stdout, "Saving section %s to %s...\n", name, outfname);

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

			if (ctx->encrypted)
				ctr_crypt_counter(&ctx->aes, buffer, buffer, max);

			if (max != fwrite(buffer, 1, max, fout))
			{
				fprintf(stdout, "Error writing output file\n");
				goto clean;
			}

			size -= max;
		}
	}

clean:
	free(compressedbuffer);
	free(decompressedbuffer);
	return;
}

void exefs_read_header(exefs_context* ctx, u32 flags)
{
	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(exefs_header), ctx->file);

	ctr_init_counter(&ctx->aes, ctx->key, ctx->counter);

	if (ctx->encrypted)
		ctr_crypt_counter(&ctx->aes, (u8*)&ctx->header, (u8*)&ctx->header, sizeof(exefs_header));
}

void exefs_calculate_hash(exefs_context* ctx, u8 hash[32])
{
	ctr_sha_256((const u8*)&ctx->header, sizeof(exefs_header), hash);
}

void exefs_process(exefs_context* ctx, u32 actions)
{
	u32 i;

	exefs_determine_key(ctx, actions);

	exefs_read_header(ctx, actions);

	if (actions & VerifyFlag)
	{	
		for(i=0; i<8; i++)
			ctx->hashcheck[i] = exefs_verify(ctx, i, actions)? Good : Fail;
	}

	if (actions & InfoFlag)
	{
		exefs_print(ctx);
	}

	if (actions & ExtractFlag)
	{
		filepath* dirpath = settings_get_exefs_dir_path(ctx->usersettings);

		if (dirpath && dirpath->valid)
		{
			makedir(dirpath->pathname);
			for(i=0; i<8; i++)
				exefs_save(ctx, i, actions);
		}
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
	ctr_init_counter(&ctx->aes, ctx->key, ctx->counter);
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

		if (ctx->encrypted)
			ctr_crypt_counter(&ctx->aes, buffer, buffer, max);

		ctr_sha_256_update(&ctx->sha, buffer, max);

		size -= max;
	}	

	ctr_sha_256_finish(&ctx->sha, hash);

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
			if (ctx->hashcheck[i] == Good)
				memdump(stdout, "Section hash (GOOD):    ", ctx->header.hashes[7-i], 0x20);
			else if (ctx->hashcheck[i] == Fail)
				memdump(stdout, "Section hash (FAIL):    ", ctx->header.hashes[7-i], 0x20);
			else
				memdump(stdout, "Section hash:           ", ctx->header.hashes[7-i], 0x20);
		}
	}
}
