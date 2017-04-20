#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tmd.h"
#include "utils.h"


void tmd_init(tmd_context* ctx)
{
	memset(ctx, 0, sizeof(tmd_context));
}

void tmd_set_file(tmd_context* ctx, FILE* file)
{
	ctx->file = file;
}

void tmd_set_offset(tmd_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void tmd_set_size(tmd_context* ctx, u32 size)
{
	ctx->size = size;
}

void tmd_set_usersettings(tmd_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void tmd_process(tmd_context* ctx, u32 actions)
{
	if (ctx->buffer == 0)
		ctx->buffer = malloc(ctx->size);

	if (ctx->buffer)
	{
		fseek(ctx->file, ctx->offset, SEEK_SET);
		fread(ctx->buffer, 1, ctx->size, ctx->file);

		if (actions & InfoFlag)
		{
			tmd_print(ctx);
		}
	}
}

ctr_tmd_body *tmd_get_body(tmd_context *ctx) 
{
	unsigned int type = getbe32(ctx->buffer);
	ctr_tmd_body *body = NULL;

	if (type == TMD_RSA_2048_SHA256 || type == TMD_RSA_2048_SHA1)
	{
		body = (ctr_tmd_body*)(ctx->buffer + sizeof(ctr_tmd_header_2048));
	}
	else if (type == TMD_RSA_4096_SHA256 || type == TMD_RSA_4096_SHA1)
	{
		body = (ctr_tmd_body*)(ctx->buffer + sizeof(ctr_tmd_header_4096));
	}

	return body;
}

const char* tmd_get_type_string(unsigned int type)
{
	switch(type)
	{
	case TMD_RSA_2048_SHA256: return "RSA 2048 - SHA256";
	case TMD_RSA_4096_SHA256: return "RSA 4096 - SHA256";
	case TMD_RSA_2048_SHA1: return "RSA 2048 - SHA1";
	case TMD_RSA_4096_SHA1: return "RSA 4096 - SHA1";
	default:
		return "unknown";
	}
}

void tmd_print(tmd_context* ctx)
{
	unsigned int type = getbe32(ctx->buffer);
	ctr_tmd_header_4096* header4096 = 0;
	ctr_tmd_header_2048* header2048 = 0;
	ctr_tmd_body* body = 0;
	unsigned int contentcount = 0;
	unsigned int i;

	if (type == TMD_RSA_2048_SHA256 || type == TMD_RSA_2048_SHA1)
	{
		header2048 = (ctr_tmd_header_2048*)ctx->buffer;
	}
	else if (type == TMD_RSA_4096_SHA256 || type == TMD_RSA_4096_SHA1)
	{
		header4096 = (ctr_tmd_header_4096*)ctx->buffer;
	}
	else
	{
		return;
	}

	body = tmd_get_body(ctx);

	contentcount = getbe16(body->contentcount);

	fprintf(stdout, "\nTMD header:\n");
	fprintf(stdout, "Signature type:         %s\n", tmd_get_type_string(type));
	fprintf(stdout, "Issuer:                 %s\n", body->issuer);
	fprintf(stdout, "Version:                %d\n", body->version);
	fprintf(stdout, "CA CRL version:         %d\n", body->ca_crl_version);
	fprintf(stdout, "Signer CRL version:     %d\n", body->signer_crl_version);
	memdump(stdout, "System version:         ", body->systemversion, 8);
	memdump(stdout, "Title id:               ", body->titleid, 8);
	fprintf(stdout, "Title type:             %08x\n", getbe32(body->titletype));
	fprintf(stdout, "Group id:               %04x\n", getbe16(body->groupid));
	fprintf(stdout, "Access rights:          %08x\n", getbe32(body->accessrights));
	fprintf(stdout, "Title version:          %04x\n", getbe16(body->titleversion));
	fprintf(stdout, "Content count:          %04x\n", getbe16(body->contentcount));
	fprintf(stdout, "Boot content:           %04x\n", getbe16(body->bootcontent));
	memdump(stdout, "Hash:                   ", body->hash, 32);

	fprintf(stdout, "\nTMD content info:\n");
	for(i = 0; i < TMD_MAX_CONTENTS; i++)
	{
		ctr_tmd_contentinfo* info = (ctr_tmd_contentinfo*)(body->contentinfo + sizeof(ctr_tmd_contentinfo)*i);

		if (getbe16(info->commandcount) == 0)
			continue;

		fprintf(stdout, "Content index:          %04x\n", getbe16(info->index));
		fprintf(stdout, "Command count:          %04x\n", getbe16(info->commandcount));
		memdump(stdout, "Unknown:                ", info->unk, 32);
	}
	fprintf(stdout, "\nTMD contents:\n");
	for(i = 0; i < contentcount; i++)
	{
		ctr_tmd_contentchunk* chunk = (ctr_tmd_contentchunk*)(body->contentinfo + 36*64 + i*48);
		unsigned short type = getbe16(chunk->type);

		fprintf(stdout, "Content id:             %08x\n", getbe32(chunk->id));
		fprintf(stdout, "Content index:          %04x\n", getbe16(chunk->index));
		fprintf(stdout, "Content type:           %04x", getbe16(chunk->type));
		if (type)
		{
			fprintf(stdout, " ");
			if (type & 1)
				fprintf(stdout, "[encrypted]");
			if (type & 2)
				fprintf(stdout, "[disc]");
			if (type & 4)
				fprintf(stdout, "[cfm]");
			if (type & 0x4000)
				fprintf(stdout, "[optional]");
			if (type & 0x8000)
				fprintf(stdout, "[shared]");
		}
		fprintf(stdout, "\n");
		fprintf(stdout, "Content size:           %016llx\n", getbe64(chunk->size));

		switch(ctx->content_hash_stat[getbe16(chunk->index)]) {
			case 1:  memdump(stdout, "Content hash [OK]:      ", chunk->hash, 32); break;
			case 2:  memdump(stdout, "Content hash [FAIL]:    ", chunk->hash, 32); break;
			default: memdump(stdout, "Content hash:           ", chunk->hash, 32); break; 
		}

		fprintf(stdout, "\n");
	}
}
