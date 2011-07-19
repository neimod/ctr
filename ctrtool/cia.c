#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "cia.h"


void cia_init(cia_context* ctx)
{
	memset(ctx, 0, sizeof(cia_context));

	tik_init(&ctx->tik);
	tmd_init(&ctx->tmd);
	filepath_init(&ctx->certspath);
	filepath_init(&ctx->contentpath);
	filepath_init(&ctx->metapath);
	filepath_init(&ctx->tikpath);
	filepath_init(&ctx->tmdpath);
}

void cia_set_file(cia_context* ctx, FILE* file)
{
	ctx->file = file;
}

void cia_set_offset(cia_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void cia_set_commonkey(cia_context* ctx, u8 key[16])
{
	memcpy(ctx->key, key, 16);
	ctx->keyvalid = 1;
}

void cia_set_certspath(cia_context* ctx, const char* path)
{
	filepath_set(&ctx->certspath, path);
}

void cia_set_contentpath(cia_context* ctx, const char* path)
{
	filepath_set(&ctx->contentpath, path);
}

void cia_set_tikpath(cia_context* ctx, const char* path)
{
	filepath_set(&ctx->tikpath, path);
}

void cia_set_tmdpath(cia_context* ctx, const char* path)
{
	filepath_set(&ctx->tmdpath, path);
}

void cia_set_metapath(cia_context* ctx, const char* path)
{
	filepath_set(&ctx->metapath, path);
}


void cia_save(cia_context* ctx, u32 type, u32 flags)
{
	u32 offset;
	u32 size;
	filepath* path = 0;
	ctr_tmd_body *body;
	ctr_tmd_contentchunk *chunk;
	int i;
	char tmpname[255];

	switch(type)
	{
		case CIATYPE_CERTS:
			offset = ctx->offsetcerts;
			size = ctx->sizecert;
			path = &ctx->certspath;
		break;

		case CIATYPE_TIK:
			offset = ctx->offsettik;
			size = ctx->sizetik;
			path = &ctx->tikpath;
		break;

		case CIATYPE_TMD:
			offset = ctx->offsettmd;
			size = ctx->sizetmd;
			path = &ctx->tmdpath;
		break;
		
		case CIATYPE_CONTENT:
			offset = ctx->offsetcontent;
			size = ctx->sizecontent;
			path = &ctx->contentpath;
			
			ctr_init_cbc_decrypt(&ctx->aes, ctx->titlekey, ctx->iv);
		break;

		case CIATYPE_META:
			offset = ctx->offsetmeta;
			size = ctx->sizemeta;
			path = &ctx->metapath;
		break;

		default:
			fprintf(stderr, "Error, unknown CIA type specified\n");
			return;	
		break;
	}

	if (path == 0 || path->valid == 0)
		return;

	switch(type)
	{
		case CIATYPE_CERTS: fprintf(stdout, "Saving certs to %s\n", path->pathname); break;
		case CIATYPE_TIK: fprintf(stdout, "Saving tik to %s\n", path->pathname); break;
		case CIATYPE_TMD: fprintf(stdout, "Saving tmd to %s\n", path->pathname); break;
		case CIATYPE_CONTENT:
			fprintf(stdout, "Saving content to %s\n", path->pathname);

			body  = tmd_get_body(&ctx->tmd);
			chunk = (ctr_tmd_contentchunk*)(body->contentinfo + (36*64));

			for(i = 0; i < getbe16(body->contentcount); i++) {
				printf(
					"content %04x : id:%08x idx:%04x size:%016llx\n",
					i, getbe32(chunk->id), getbe16(chunk->index), getbe64(chunk->size)
				);

				sprintf(tmpname, "%s.%04x.%08x", path->pathname, getbe16(chunk->index), getbe32(chunk->id));
				cia_save_blob(ctx, tmpname, offset, getbe64(chunk->size) & 0xffffffff, 1);

				offset += getbe64(chunk->size) & 0xffffffff;

				chunk++;
			}

			return;
		break;

		case CIATYPE_META: fprintf(stdout, "Saving meta to %s\n", path->pathname); break;
	}

	cia_save_blob(ctx, path->pathname, offset, size, 0);
}

void cia_save_blob(cia_context *ctx, char *out_path, u32 offset, u32 size, int do_cbc) {
	u8 buffer[16*1024];

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);

	FILE *fout = fopen(out_path, "wb");

	if (fout == NULL)
	{
		fprintf(stdout, "Error opening out file %s\n", out_path);
		goto clean;
	}

	while(size)
	{
		u32 max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->file))
		{
			fprintf(stdout, "Error reading file\n");
			goto clean;
		}

		if (do_cbc == 1)
			ctr_decrypt_cbc(&ctx->aes, buffer, buffer, max);

		if (max != fwrite(buffer, 1, max, fout))
		{
			fprintf(stdout, "Error writing file\n");
			goto clean;
		}

		size -= max;
	}

clean:
	if (fout)
		fclose(fout);
}


void cia_process(cia_context* ctx, u32 actions)
{	
	fseek(ctx->file, 0, SEEK_SET);

	if (fread(&ctx->header, 1, sizeof(ctr_ciaheader), ctx->file) != sizeof(ctr_ciaheader))
	{
		fprintf(stderr, "Error reading CIA header\n");
		goto clean;
	}

	ctx->sizeheader = getle32(ctx->header.headersize);
	ctx->sizecert = getle32(ctx->header.certsize);
	ctx->sizetik = getle32(ctx->header.ticketsize);
	ctx->sizetmd = getle32(ctx->header.tmdsize);
	ctx->sizecontent = (u32)getle64(ctx->header.contentsize);
	ctx->sizemeta = getle32(ctx->header.metasize);
	
	ctx->offsetcerts = align(ctx->sizeheader, 64);
	ctx->offsettik = align(ctx->offsetcerts + ctx->sizecert, 64);
	ctx->offsettmd = align(ctx->offsettik + ctx->sizetik, 64);
	ctx->offsetcontent = align(ctx->offsettmd + ctx->sizetmd, 64);
	ctx->offsetmeta = align(ctx->offsetcontent + ctx->sizecontent, 64);

	if (actions & InfoFlag)
		cia_print(ctx);


	tik_set_file(&ctx->tik, ctx->file);
	tik_set_offset(&ctx->tik, ctx->offsettik);
	tik_set_size(&ctx->tik, ctx->sizetik);

	if (ctx->keyvalid)
		tik_set_commonkey(&ctx->tik, ctx->key);

	tik_process(&ctx->tik, actions);
	memset(ctx->iv, 0, 16);

	

	if (ctx->keyvalid)
		tik_get_decrypted_titlekey(&ctx->tik, ctx->titlekey);

	tmd_set_file(&ctx->tmd, ctx->file);
	tmd_set_offset(&ctx->tmd, ctx->offsettmd);
	tmd_set_size(&ctx->tmd, ctx->sizetmd);
	tmd_process(&ctx->tmd, actions);

	if (actions & ExtractFlag)
	{
		cia_save(ctx, CIATYPE_CERTS, actions);
		cia_save(ctx, CIATYPE_TMD, actions);
		cia_save(ctx, CIATYPE_TIK, actions);
		cia_save(ctx, CIATYPE_META, actions);
		cia_save(ctx, CIATYPE_CONTENT, actions);
	}

clean:
	return;
}

void cia_print(cia_context* ctx)
{
	ctr_ciaheader* header = &ctx->header;

	fprintf(stdout, "Header size             0x%08x\n", getle32(header->headersize));
	fprintf(stdout, "Type                    %04x\n", getle16(header->type));
	fprintf(stdout, "Version                 %04x\n", getle16(header->version));
	fprintf(stdout, "Certificates offset:    0x%08x\n", ctx->offsetcerts);
	fprintf(stdout, "Certificates size:      0x%04x\n", ctx->sizecert);
	fprintf(stdout, "Ticket offset:          0x%08x\n", ctx->offsettik);
	fprintf(stdout, "Ticket size             0x%04x\n", ctx->sizetik);
	fprintf(stdout, "TMD offset:             0x%08x\n", ctx->offsettmd);
	fprintf(stdout, "TMD size:               0x%04x\n", ctx->sizetmd);
	fprintf(stdout, "Meta offset:            0x%04x\n", ctx->offsetmeta);
	fprintf(stdout, "Meta size:              0x%04x\n", ctx->sizemeta);
	fprintf(stdout, "Content offset:         0x%08x\n", ctx->offsetcontent);
	fprintf(stdout, "Content size:           0x%016llx\n", getle64(header->contentsize));
}
