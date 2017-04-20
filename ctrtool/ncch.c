#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "ncch.h"
#include "utils.h"
#include "ctr.h"
#include "settings.h"

static int programid_is_system(u8 programid[8])
{
	u32 hiprogramid = getle32(programid+4);
	
	if ( ((hiprogramid >> 14) == 0x10) && (hiprogramid & 0x10) )
		return 1;
	else
		return 0;
}


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

void ncch_get_counter(ncch_context* ctx, u8 counter[16], u8 type)
{
	u32 version = getle16(ctx->header.version);
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	u8* partitionid = ctx->header.partitionid;
	u32 i;
	u32 x = 0;

	memset(counter, 0, 16);

	if (version == 2 || version == 0)
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
			size = ncch_get_exheader_size(ctx) * 2;
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

		if (ctx->encrypted)
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

void ncch_verify(ncch_context* ctx, u32 flags)
{
	u32 mediaunitsize = ncch_get_mediaunit_size(ctx);
	u32 exefshashregionsize = getle32(ctx->header.exefshashregionsize) * mediaunitsize;
	u32 romfshashregionsize = getle32(ctx->header.romfshashregionsize) * mediaunitsize;
	u32 exheaderhashregionsize = getle32(ctx->header.extendedheadersize);
	u8* exefshashregion = 0;
	u8* romfshashregion = 0;
	u8* exheaderhashregion = 0;
	rsakey2048 ncchrsakey;

	if (exefshashregionsize >= SIZE_128MB || romfshashregionsize >= SIZE_128MB || exheaderhashregionsize >= SIZE_128MB)
		goto clean;

	exefshashregion = malloc(exefshashregionsize);
	romfshashregion = malloc(romfshashregionsize);
	exheaderhashregion = malloc(exheaderhashregionsize);


	if (ctx->usersettings)
	{
		if ( (ctx->header.flags[5] & 3) == 1)
			ctx->headersigcheck = ncch_signature_verify(ctx, &ctx->usersettings->keys.ncchrsakey);
		else 
		{
			ctr_rsa_init_key_pubmodulus(&ncchrsakey, ctx->exheader.header.accessdesc.ncchpubkeymodulus);
			ctx->headersigcheck =  ncch_signature_verify(ctx, &ncchrsakey);
		}
	}

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
	int result = 1;


	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, 0x200, ctx->file);

	if (getle32(ctx->header.magic) != MAGIC_NCCH)
	{
		fprintf(stdout, "Error, NCCH segment corrupted\n");
		return;
	}

	ncch_determine_key(ctx, actions);

	ncch_get_counter(ctx, exheadercounter, NCCHTYPE_EXHEADER);
	ncch_get_counter(ctx, exefscounter, NCCHTYPE_EXEFS);


	exheader_set_file(&ctx->exheader, ctx->file);
	exheader_set_offset(&ctx->exheader, ncch_get_exheader_offset(ctx) );
	exheader_set_size(&ctx->exheader, ncch_get_exheader_size(ctx) );
	exheader_set_usersettings(&ctx->exheader, ctx->usersettings);
	exheader_set_partitionid(&ctx->exheader, ctx->header.partitionid);
	exheader_set_programid(&ctx->exheader, ctx->header.programid);
	exheader_set_counter(&ctx->exheader, exheadercounter);
	exheader_set_key(&ctx->exheader, ctx->key);
	exheader_set_encrypted(&ctx->exheader, ctx->encrypted);

	exefs_set_file(&ctx->exefs, ctx->file);
	exefs_set_offset(&ctx->exefs, ncch_get_exefs_offset(ctx) );
	exefs_set_size(&ctx->exefs, ncch_get_exefs_size(ctx) );
	exefs_set_partitionid(&ctx->exefs, ctx->header.partitionid);
	exefs_set_usersettings(&ctx->exefs, ctx->usersettings);
	exefs_set_counter(&ctx->exefs, exefscounter);
	exefs_set_key(&ctx->exefs, ctx->key);
	exefs_set_encrypted(&ctx->exefs, ctx->encrypted);

	exheader_read(&ctx->exheader, actions);


	if (actions & VerifyFlag)
		ncch_verify(ctx, actions);

	if (actions & InfoFlag)
		ncch_print(ctx);		

	if (actions & ExtractFlag)
	{
		ncch_save(ctx, NCCHTYPE_EXEFS, actions);
		ncch_save(ctx, NCCHTYPE_ROMFS, actions);
		ncch_save(ctx, NCCHTYPE_EXHEADER, actions);
	}


	if (result && ncch_get_exheader_size(ctx))
	{
		if (!exheader_programid_valid(&ctx->exheader))
			return;

		result = exheader_process(&ctx->exheader, actions);
	} 

	if (result && ncch_get_exheader_size(ctx))
	{
		exefs_set_compressedflag(&ctx->exefs, exheader_get_compressedflag(&ctx->exheader));
		exefs_process(&ctx->exefs, actions);
	}
}

int ncch_signature_verify(ncch_context* ctx, rsakey2048* key)
{
	u8 hash[0x20];

	ctr_sha_256(ctx->header.magic, 0x100, hash);
	return ctr_rsa_verify_hash(ctx->header.signature, hash, key);
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
		else if (version == 2 || version == 0)
			mediaunitsize = 1 << (ctx->header.flags[6] + 9);
	}

	return mediaunitsize;
}


void ncch_determine_key(ncch_context* ctx, u32 actions)
{
	exheader_header exheader;
	u8* key = settings_get_ncch_key(ctx->usersettings);
	ctr_ncchheader* header = &ctx->header;

	ctx->encrypted = 0;
	memset(ctx->key, 0, 0x10);

	if (actions & PlainFlag)
	{
		ctx->encrypted = 0;
	} 
	else if (key != 0)
	{
		ctx->encrypted = 1;
		memcpy(ctx->key, key, 0x10);
	}
	else
	{
		// No explicit NCCH key defined, so we try to decide
		

		// Firstly, check if the NCCH is already decrypted, by reading the programid in the exheader
		// Otherwise, use determination rules
		fseek(ctx->file, ncch_get_exheader_offset(ctx), SEEK_SET);
		memset(&exheader, 0, sizeof(exheader));
		fread(&exheader, 1, sizeof(exheader), ctx->file);

		if (!memcmp(exheader.arm11systemlocalcaps.programid, ctx->header.programid, 8))
		{
			// program id's match, so it's probably not encrypted
			ctx->encrypted = 0;
		}
		else if (header->flags[7] & 4)
		{
			ctx->encrypted = 0; // not encrypted
		}
		else if (header->flags[7] & 1)
		{
			if (programid_is_system(header->programid))
			{
				// fixed system key
				ctx->encrypted = 1;
				key = settings_get_ncch_fixedsystemkey(ctx->usersettings);
				if (!key)
					fprintf(stdout, "Warning, could not read system fixed key.\n");
				else
					memcpy(ctx->key, key, 0x10);
			}
			else
			{
				// null key
				ctx->encrypted = 1;
				memset(ctx->key, 0, 0x10);
			}
		}
		else
		{
			// secure key (cannot decrypt!)
			fprintf(stdout, "Warning, could not read secure key.\n");
			ctx->encrypted = 1;
			memset(ctx->key, 0, 0x10);
		}
	}
}

static const char* formtypetostring(unsigned char flags)
{
	unsigned char formtype = flags & 3;

	switch(formtype)
	{
	case 0: return "Not assigned";
	case 1: return "Simple content";
	case 2: return "Executable content without RomFS";
	case 3: return "Executable content";
	default: return "Unknown";
	}
}

static const char* contenttypetostring(unsigned char flags)
{
	unsigned char contenttype = flags>>2;

	switch(contenttype)
	{
	case 0: return "Application";
	case 1: return "System Update";
	case 2: return "Manual";
	case 3: return "Child";
	case 4: return "Trial";
	default: return "Unknown";
	}
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
	if (ctx->headersigcheck == Unchecked)
		memdump(stdout, "Signature:              ", header->signature, 0x100);
	else if (ctx->headersigcheck == Good)
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
	if (ctx->exheaderhashcheck == Unchecked)
		memdump(stdout, "Exheader hash:          ", header->extendedheaderhash, 0x20);
	else if (ctx->exheaderhashcheck == Good)
		memdump(stdout, "Exheader hash (GOOD):   ", header->extendedheaderhash, 0x20);
	else
		memdump(stdout, "Exheader hash (FAIL):   ", header->extendedheaderhash, 0x20);
	fprintf(stdout, "Flags:                  %016llx\n", getle64(header->flags));
	fprintf(stdout, " > Mediaunit size:      0x%x\n", mediaunitsize);
	if (header->flags[7] & 4)
		fprintf(stdout, " > Crypto key:          None\n");
	else if (header->flags[7] & 1)
		fprintf(stdout, " > Crypto key:          %s\n", programid_is_system(header->programid)? "Fixed":"Zeros");
	else
		fprintf(stdout, " > Crypto key:          Secure\n");
	fprintf(stdout, " > Form type:           %s\n", formtypetostring(header->flags[5]));
	fprintf(stdout, " > Content type:        %s\n", contenttypetostring(header->flags[5]));
	if (header->flags[4] & 1)
		fprintf(stdout, " > Content platform:    CTR\n");
	if (header->flags[7] & 2)
		fprintf(stdout, " > No RomFS mount\n");


	fprintf(stdout, "Plain region offset:    0x%08x\n", getle32(header->plainregionsize)? offset+getle32(header->plainregionoffset)*mediaunitsize : 0);
	fprintf(stdout, "Plain region size:      0x%08x\n", getle32(header->plainregionsize)*mediaunitsize);
	fprintf(stdout, "ExeFS offset:           0x%08x\n", getle32(header->exefssize)? offset+getle32(header->exefsoffset)*mediaunitsize : 0);
	fprintf(stdout, "ExeFS size:             0x%08x\n", getle32(header->exefssize)*mediaunitsize);
	fprintf(stdout, "ExeFS hash region size: 0x%08x\n", getle32(header->exefshashregionsize)*mediaunitsize);
	fprintf(stdout, "RomFS offset:           0x%08x\n", getle32(header->romfssize)? offset+getle32(header->romfsoffset)*mediaunitsize : 0);
	fprintf(stdout, "RomFS size:             0x%08x\n", getle32(header->romfssize)*mediaunitsize);
	fprintf(stdout, "RomFS hash region size: 0x%08x\n", getle32(header->romfshashregionsize)*mediaunitsize);
	if (ctx->exefshashcheck == Unchecked)
		memdump(stdout, "ExeFS Hash:             ", header->exefssuperblockhash, 0x20);
	else if (ctx->exefshashcheck == Good)
		memdump(stdout, "ExeFS Hash (GOOD):      ", header->exefssuperblockhash, 0x20);
	else
		memdump(stdout, "ExeFS Hash (FAIL):      ", header->exefssuperblockhash, 0x20);
	if (ctx->romfshashcheck == Unchecked)
		memdump(stdout, "RomFS Hash:             ", header->romfssuperblockhash, 0x20);
	else if (ctx->romfshashcheck == Good)
		memdump(stdout, "RomFS Hash (GOOD):      ", header->romfssuperblockhash, 0x20);
	else
		memdump(stdout, "RomFS Hash (FAIL):      ", header->romfssuperblockhash, 0x20);
}
