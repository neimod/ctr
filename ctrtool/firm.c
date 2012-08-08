#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "firm.h"
#include "utils.h"

void firm_init(firm_context* ctx)
{
	memset(ctx, 0, sizeof(firm_context));
}

void firm_set_file(firm_context* ctx, FILE* file)
{
	ctx->file = file;
}

void firm_set_offset(firm_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void firm_set_size(firm_context* ctx, u32 size)
{
	ctx->size = size;
}

void firm_set_usersettings(firm_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void firm_save(firm_context* ctx, u32 index, u32 flags)
{
	firm_sectionheader* section = (firm_sectionheader*)(ctx->header.section + index);
	u32 offset;
	u32 size;
	u32 address;
	FILE* fout;
	filepath outpath;
	u8 buffer[16 * 1024];
	
	
	offset = getle32(section->offset);
	size = getle32(section->size);
	address = getle32(section->address);
	filepath_copy(&outpath, settings_get_firm_dir_path(ctx->usersettings));
	filepath_append(&outpath, "firm_%d_%08X.bin", index, address);

	if (size == 0 || outpath.valid == 0)
		return;

	if (size >= ctx->size)
	{
		fprintf(stderr, "Error, firm section %d size invalid\n", index);
		return;
	}

	fout = fopen(outpath.pathname, "wb");
	if (fout == 0)
	{
		fprintf(stderr, "Error, failed to create file %s\n", outpath.pathname);
		goto clean;
	}
	
	

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	fprintf(stdout, "Saving section %d to %s...\n", index, outpath.pathname);

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


void firm_process(firm_context* ctx, u32 actions)
{
	u32 i;

	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(firm_header), ctx->file);

	if (getle32(ctx->header.magic) != MAGIC_FIRM)
	{
		fprintf(stdout, "Error, FIRM segment corrupted\n");
		return;
	}


	if (actions & VerifyFlag)
	{
		firm_verify(ctx, actions);
		firm_signature_verify(ctx);
	}

	if (actions & InfoFlag)
	{
		firm_print(ctx);
	}

	if (actions & ExtractFlag)
	{
		filepath* dirpath = settings_get_firm_dir_path(ctx->usersettings);

		if (dirpath && dirpath->valid)
		{
			makedir(dirpath->pathname);
			for(i=0; i<4; i++)
				firm_save(ctx, i, actions);
		}
	}
}

int firm_verify(firm_context* ctx, u32 flags)
{
	unsigned int i;
	u32 offset;
	u32 size;
	u8 buffer[16 * 1024];
	u8 hash[0x20];


	for(i=0; i<4; i++)
	{
		firm_sectionheader* section = (firm_sectionheader*)(ctx->header.section + i);
	

		offset = getle32(section->offset);
		size = getle32(section->size);

		if (size == 0)
			return 0;

		fseek(ctx->file, ctx->offset + offset, SEEK_SET);

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

			ctr_sha_256_update(&ctx->sha, buffer, max);

			size -= max;
		}	

		ctr_sha_256_finish(&ctx->sha, hash);


		if (memcmp(hash, section->hash, 0x20) == 0)
			ctx->hashcheck[i] = Good;
		else
			ctx->hashcheck[i] = Fail;
	}


clean:
	return 0;
}


void firm_signature_verify(firm_context* ctx)
{
	u8 hash[0x20];

	if (ctx->usersettings)
	{
		ctr_sha_256(ctx->header.magic, 0x100, hash);
		ctx->headersigcheck = ctr_rsa_verify_hash(ctx->header.signature, hash, &ctx->usersettings->keys.firmrsakey);
	}
}


void firm_print(firm_context* ctx)
{
	u32 i;
	u32 address;
	u32 type;
	u32 offset;
	u32 size;
	u32 entrypointarm11 = getle32(ctx->header.entrypointarm11);
	u32 entrypointarm9 = getle32(ctx->header.entrypointarm9);

	fprintf(stdout, "\nFIRM:\n");
	if (ctx->headersigcheck == Unchecked)
		memdump(stdout, "Signature:              ", ctx->header.signature, 0x100);
	else if (ctx->headersigcheck == Good)
		memdump(stdout, "Signature (GOOD):       ", ctx->header.signature, 0x100);
	else
		memdump(stdout, "Signature (FAIL):       ", ctx->header.signature, 0x100);

	fprintf(stdout, "\n");
	fprintf(stdout, "Entrypoint ARM9:        0x%08X\n", entrypointarm9);
	fprintf(stdout, "Entrypoint ARM11:       0x%08X\n", entrypointarm11);
	fprintf(stdout, "\n");


	for(i=0; i<4; i++)
	{
		firm_sectionheader* section = (firm_sectionheader*)(ctx->header.section + i);


		offset = getle32(section->offset);
		size = getle32(section->size);
		address = getle32(section->address);
		type = getle32(section->type);

		if (size)
		{
			fprintf(stdout, "Section %d              \n", i);
			fprintf(stdout, " Type:                  %s\n", type==0? "ARM9" : type==1? "ARM11" : "UNKNOWN");
			fprintf(stdout, " Address:               0x%08X\n", address);
			fprintf(stdout, " Offset:                0x%08X\n", offset);
			fprintf(stdout, " Size:                  0x%08X\n", size);			
			if (ctx->hashcheck[i] == Good)
				memdump(stdout, " Hash (GOOD):           ", section->hash, 0x20);
			else if (ctx->hashcheck[i] == Fail)
				memdump(stdout, " Hash (FAIL):           ", section->hash, 0x20);
			else
				memdump(stdout, " Hash:                  ", section->hash, 0x20);
		}
	}
}
