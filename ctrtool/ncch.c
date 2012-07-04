#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "ncch.h"
#include "utils.h"
#include "ctr.h"
#include "settings.h"


void ncch_init(ncch_context* ctx)
{
	memset(ctx, 0, sizeof(ncch_context));
	exefs_init(&ctx->exefs);
}

void ncch_set_usersettings(ncch_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void ncch_set_offset(ncch_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void ncch_set_size(ncch_context* ctx, u32 size)
{
	ctx->size = size;
}

void ncch_set_file(ncch_context* ctx, FILE* file)
{
	ctx->file = file;
}

int ncch_encrypted(ncch_context* ctx, u32 flags)
{
	if (flags & PlainFlag)
		return 0;
		
	if (ctx->header.flags[7] & 4)
		return 0;
	
	return 1;
}

void ncch_get_counter(ncch_context* ctx, u8 counter[16], u8 type)
{
	u32 version = getle16(ctx->header.version);
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	u8* partitionid = ctx->header.partitionid;
	u32 i;
	u32 x = 0;

	memset(counter, 0, 16);

	if (version == 2)
	{
		for(i=0; i<8; i++)
			counter[i] = partitionid[7-i];
		counter[8] = type;
	}
	else if (version == 1)
	{
		if (type == NCCHTYPE_EXHEADER)
			x = 0x200;
		else if (type == NCCHTYPE_EXEFS)
			x = getle32(ctx->header.exefsoffset) * mediaunitsize;
		else if (type == NCCHTYPE_ROMFS)
			x = getle32(ctx->header.romfsoffset) * mediaunitsize;

		for(i=0; i<8; i++)
			counter[i] = partitionid[i];
		for(i=0; i<4; i++)
			counter[12+i] = x>>((3-i)*8);
	}
}



int ncch_extract_prepare(ncch_context* ctx, u32 type, u32 flags)
{
	u32 offset = 0;
	u32 size = 0;
	u8 counter[16];


	switch(type)
	{	
		case NCCHTYPE_EXEFS:
		{
			offset = ncch_get_exefs_offset(ctx);
			size = ncch_get_exefs_size(ctx);
		}
		break;

		case NCCHTYPE_ROMFS:
		{
			offset = ncch_get_romfs_offset(ctx);
			size = ncch_get_romfs_size(ctx);
		}
		break;

		case NCCHTYPE_EXHEADER:
		{
			offset = ncch_get_exheader_offset(ctx);
			size = ncch_get_exheader_size(ctx);
		}
		break;
	
		default:
		{
			fprintf(stderr, "Error invalid NCCH type\n");
			goto clean;
		}
		break;
	}

	ctx->extractsize = size;
	ctx->extractflags = flags;
	fseek(ctx->file, offset, SEEK_SET);
	ncch_get_counter(ctx, counter, type);
	ctr_init_counter(&ctx->aes, ctx->key, counter);

	return 1;

clean:
	return 0;
}

int ncch_extract_buffer(ncch_context* ctx, u8* buffer, u32 buffersize, u32* outsize)
{
	u32 max = buffersize;

	if (max > ctx->extractsize)
		max = ctx->extractsize;

	*outsize = max;

	if (ctx->extractsize)
	{
		if (max != fread(buffer, 1, max, ctx->file))
		{
			fprintf(stdout, "Error reading input file\n");
			goto clean;
		}

		if (ncch_encrypted(ctx, ctx->extractflags))
			ctr_crypt_counter(&ctx->aes, buffer, buffer, max);

		ctx->extractsize -= max;
	}

	return 1;

clean:
	return 0;
}

void ncch_save(ncch_context* ctx, u32 type, u32 flags)
{
	FILE* fout = 0;
	filepath* path = 0;
	u8 buffer[16*1024];


	if (0 == ncch_extract_prepare(ctx, type, flags))
		goto clean;

	switch(type)
	{	
		case NCCHTYPE_EXEFS: path = settings_get_exefs_path(ctx->usersettings); break;
		case NCCHTYPE_ROMFS: path = settings_get_romfs_path(ctx->usersettings); break;
		case NCCHTYPE_EXHEADER: path = settings_get_exheader_path(ctx->usersettings); break;
	}

	if (path == 0 || path->valid == 0)
		goto clean;

	fout = fopen(path->pathname, "wb");
	if (0 == fout)
	{
		fprintf(stdout, "Error opening out file %s\n", path->pathname);
		goto clean;
	}

	switch(type)
	{
		case NCCHTYPE_EXEFS: fprintf(stdout, "Saving ExeFS...\n"); break;
		case NCCHTYPE_ROMFS: fprintf(stdout, "Saving RomFS...\n"); break;
		case NCCHTYPE_EXHEADER: fprintf(stdout, "Saving Extended Header...\n"); break;
	}

	while(1)
	{
		u32 max;

		if (0 == ncch_extract_buffer(ctx, buffer, sizeof(buffer), &max))
			goto clean;

		if (max == 0)
			break;

		if (max != fwrite(buffer, 1, max, fout))
		{
			fprintf(stdout, "Error writing output file\n");
			goto clean;
		}
	}
clean:
	if (fout)
		fclose(fout);
	return;
}

void ncch_verify_hashes(ncch_context* ctx, u32 flags)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	u32 exefshashregionsize = getle32(ctx->header.exefshashregionsize) * mediaunitsize;
	u32 romfshashregionsize = getle32(ctx->header.romfshashregionsize) * mediaunitsize;
	u32 exheaderhashregionsize = getle32(ctx->header.extendedheadersize) * mediaunitsize;
	u8* exefshashregion = malloc(exefshashregionsize);
	u8* romfshashregion = malloc(romfshashregionsize);
	u8* exheaderhashregion = malloc(exheaderhashregionsize);

	if (exefshashregionsize)
	{
		if (0 == ncch_extract_prepare(ctx, NCCHTYPE_EXEFS, flags))
			goto clean;
		if (0 == ncch_extract_buffer(ctx, exefshashregion, exefshashregionsize, &exefshashregionsize))
			goto clean;
		ctx->exefshashcheck = ctr_sha_256_verify(exefshashregion, exefshashregionsize, ctx->header.exefssuperblockhash);
	}
	if (romfshashregionsize)
	{
		if (0 == ncch_extract_prepare(ctx, NCCHTYPE_ROMFS, flags))
			goto clean;
		if (0 == ncch_extract_buffer(ctx, romfshashregion, romfshashregionsize, &romfshashregionsize))
			goto clean;
		ctx->romfshashcheck = ctr_sha_256_verify(romfshashregion, romfshashregionsize, ctx->header.romfssuperblockhash);
	}
	if (exheaderhashregionsize)
	{
		if (0 == ncch_extract_prepare(ctx, NCCHTYPE_EXHEADER, flags))
			goto clean;
		if (0 == ncch_extract_buffer(ctx, exheaderhashregion, exheaderhashregionsize, &exheaderhashregionsize))
			goto clean;
		ctx->exheaderhashcheck = ctr_sha_256_verify(exheaderhashregion, exheaderhashregionsize, ctx->header.extendedheaderhash);
	}

	free(exefshashregion);
	free(romfshashregion);
	free(exheaderhashregion);
clean:
	return;
}


void ncch_process(ncch_context* ctx, u32 actions)
{
	u8 exheadercounter[16];
	u8 exefscounter[16];


	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, 0x200, ctx->file);

	ncch_get_counter(ctx, exheadercounter, NCCHTYPE_EXHEADER);
	ncch_get_counter(ctx, exefscounter, NCCHTYPE_EXEFS);


	exheader_set_file(&ctx->exheader, ctx->file);
	exheader_set_offset(&ctx->exheader, ncch_get_exheader_offset(ctx) );
	exheader_set_size(&ctx->exheader, ncch_get_exheader_size(ctx) );
	exheader_set_usersettings(&ctx->exheader, ctx->usersettings);
	exheader_set_partitionid(&ctx->exheader, ctx->header.partitionid);
	exheader_set_programid(&ctx->exheader, ctx->header.programid);
	exheader_set_counter(&ctx->exheader, exheadercounter);
	exheader_set_cryptoflag(&ctx->exheader, ctx->header.flags[7]);

	exefs_set_file(&ctx->exefs, ctx->file);
	exefs_set_offset(&ctx->exefs, ncch_get_exefs_offset(ctx) );
	exefs_set_size(&ctx->exefs, ncch_get_exefs_size(ctx) );
	exefs_set_partitionid(&ctx->exefs, ctx->header.partitionid);
	exefs_set_usersettings(&ctx->exefs, ctx->usersettings);
	exefs_set_counter(&ctx->exefs, exefscounter);
	exefs_set_cryptoflag(&ctx->exefs, ctx->header.flags[7]);

	if (actions & VerifyFlag)
	{
		if (ctx->usersettings)
			ctx->headersigcheck = ncch_signature_verify(&ctx->header, &ctx->usersettings->keys.ncchrsakey);

		ncch_verify_hashes(ctx, actions);
	}

	if (actions & InfoFlag)
		ncch_print(ctx);		

	if (actions & ExtractFlag)
	{
		ncch_save(ctx, NCCHTYPE_EXEFS, actions);
		ncch_save(ctx, NCCHTYPE_ROMFS, actions);
		ncch_save(ctx, NCCHTYPE_EXHEADER, actions);
	}


	if (exheader_process(&ctx->exheader, actions))
	{
		exefs_set_compressedflag(&ctx->exefs, exheader_get_compressedflag(&ctx->exheader));
		exefs_process(&ctx->exefs, actions);
	}
}

int ncch_signature_verify(const ctr_ncchheader* header, rsakey2048* key)
{
	u8 hash[0x20];

#if 0
	u8 output[0x100];

	ctr_rsa_public(header->signature, output, key);
	memdump(stdout, "RSA decrypted:      ", output, 0x100);
#endif

	

	ctr_sha_256(header->magic, 0x100, hash);

#if 0
	ctr_rsa_sign_hash(hash, output, key);
	memdump(stdout, "RSA signed:    ", output, 0x100);
#endif
	return ctr_rsa_verify_hash(header->signature, hash, key);
}

u32 ncch_get_exefs_offset(ncch_context* ctx)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	return ctx->offset + getle32(ctx->header.exefsoffset) * mediaunitsize;
}

u32 ncch_get_exefs_size(ncch_context* ctx)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	return getle32(ctx->header.exefssize) * mediaunitsize;
}

u32 ncch_get_romfs_offset(ncch_context* ctx)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	return ctx->offset + getle32(ctx->header.romfsoffset) * mediaunitsize;
}

u32 ncch_get_romfs_size(ncch_context* ctx)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	return getle32(ctx->header.romfssize) * mediaunitsize;
}

u32 ncch_get_exheader_offset(ncch_context* ctx)
{
	return ctx->offset + 0x200;
}

u32 ncch_get_exheader_size(ncch_context* ctx)
{
	return getle32(ctx->header.extendedheadersize);
}

u32 ncch_get_mediaunit_size(ncch_context* ctx)
{
	unsigned int mediaunitsize = settings_get_mediaunit_size(ctx->usersettings);

	if (mediaunitsize == 0)
	{
		unsigned short version = getle16(ctx->header.version);
		if (version == 1)
			mediaunitsize = 1;
		else if (version == 2)
			mediaunitsize = 1 << (ctx->header.flags[6] + 9);
	}

	return mediaunitsize;
}

void ncch_print(ncch_context* ctx)
{
	char magic[5];
	char productcode[0x11];
	ctr_ncchheader *header = &ctx->header;
	u32 offset = ctx->offset;
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);


	fprintf(stdout, "\nNCCH:\n");
	memcpy(magic, header->magic, 4);
	magic[4] = 0;
	memcpy(productcode, header->productcode, 0x10);
	productcode[0x10] = 0;

	fprintf(stdout, "Header:                 %s\n", magic);
	if (ctx->headersigcheck == HashUnchecked)
		memdump(stdout, "Signature:              ", header->signature, 0x100);
	else if (ctx->headersigcheck == HashGood)
		memdump(stdout, "Signature (GOOD):       ", header->signature, 0x100);
	else
		memdump(stdout, "Signature (FAIL):       ", header->signature, 0x100);
	fprintf(stdout, "Content size:           0x%08x\n", getle32(header->contentsize)*mediaunitsize);
	fprintf(stdout, "Partition id:           %016llx\n", getle64(header->partitionid));
	fprintf(stdout, "Maker code:             %04x\n", getle16(header->makercode));
	fprintf(stdout, "Version:                %04x\n", getle16(header->version));
	fprintf(stdout, "Program id:             %016llx\n", getle64(header->programid));
	fprintf(stdout, "Temp flag:              %02x\n", header->tempflag);
	fprintf(stdout, "Product code:           %s\n", productcode);
	fprintf(stdout, "Exheader size:          %08x\n", getle32(header->extendedheadersize));
	if (ctx->exheaderhashcheck == HashUnchecked)
		memdump(stdout, "Exheader hash:          ", header->extendedheaderhash, 0x20);
	else if (ctx->exheaderhashcheck == HashGood)
		memdump(stdout, "Exheader hash (GOOD):   ", header->extendedheaderhash, 0x20);
	else
		memdump(stdout, "Exheader hash (FAIL):   ", header->extendedheaderhash, 0x20);
	fprintf(stdout, "Flags:                  %016llx\n", getle64(header->flags));
	fprintf(stdout, " > Mediaunit size:      0x%x\n", mediaunitsize);
	if (header->flags[7] & 4)
		fprintf(stdout, " > Crypto key:          None\n");
	else if (header->flags[7] & 1)
		fprintf(stdout, " > Crypto key:          Zeros\n");
	else
		fprintf(stdout, " > Crypto key:          Secure\n");

	fprintf(stdout, "Plain region offset:    0x%08x\n", getle32(header->plainregionsize)? offset+getle32(header->plainregionoffset)*mediaunitsize : 0);
	fprintf(stdout, "Plain region size:      0x%08x\n", getle32(header->plainregionsize)*mediaunitsize);
	fprintf(stdout, "ExeFS offset:           0x%08x\n", getle32(header->exefssize)? offset+getle32(header->exefsoffset)*mediaunitsize : 0);
	fprintf(stdout, "ExeFS size:             0x%08x\n", getle32(header->exefssize)*mediaunitsize);
	fprintf(stdout, "ExeFS hash region size: 0x%08x\n", getle32(header->exefshashregionsize)*mediaunitsize);
	fprintf(stdout, "RomFS offset:           0x%08x\n", getle32(header->romfssize)? offset+getle32(header->romfsoffset)*mediaunitsize : 0);
	fprintf(stdout, "RomFS size:             0x%08x\n", getle32(header->romfssize)*mediaunitsize);
	fprintf(stdout, "RomFS hash region size: 0x%08x\n", getle32(header->romfshashregionsize)*mediaunitsize);
	if (ctx->exefshashcheck == HashUnchecked)
		memdump(stdout, "ExeFS Hash:             ", header->exefssuperblockhash, 0x20);
	else if (ctx->exefshashcheck == HashGood)
		memdump(stdout, "ExeFS Hash (GOOD):      ", header->exefssuperblockhash, 0x20);
	else
		memdump(stdout, "ExeFS Hash (FAIL):      ", header->exefssuperblockhash, 0x20);
	if (ctx->romfshashcheck == HashUnchecked)
		memdump(stdout, "RomFS Hash:             ", header->romfssuperblockhash, 0x20);
	else if (ctx->romfshashcheck == HashGood)
		memdump(stdout, "RomFS Hash (GOOD):      ", header->romfssuperblockhash, 0x20);
	else
		memdump(stdout, "RomFS Hash (FAIL):      ", header->romfssuperblockhash, 0x20);
}
