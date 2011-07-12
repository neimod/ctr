#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ctr.h"
#include "ncch.h"
#include "cia.h"

enum actionflags
{
	Extract = (1<<0),
	Info = (1<<1),
	NoDecrypt = (1<<2),
};

enum cryptotype
{
	Plain,
	CTR,
	CBC
};

typedef struct
{
	int actions;
	u32 filetype;
	u8 key[16];
	u8 iv[16];
	char exheaderfname[512];
	char romfsfname[512];
	char exefsfname[512];
	int setexefsfname;
	int setromfsfname;
	int setexheaderfname;
	u32 ncchoffset;
	ctr_crypto_context cryptoctx;
	FILE* infile;
	char certsfname[512];
	char tikfname[512];
	char tmdfname[512];
	char contentsfname[512];
	char bannerfname[512];
	int setcertsfname;
	int settikfname;
	int settmdfname;
	int setcontentsfname;
	int setbannerfname;
	

} toolcontext;

static void usage(const char *argv0)
{
	fprintf(stderr,
		   "Usage: %s [options...] <file>\n"
		   "CTRTOOL (c) neimod.\n"
           "\n"
           "Options:\n"
           "  -i, --info         Show file info.\n"
		   "                          This is the default action.\n"
           "  -x, --extract      Extract data from file.\n"
		   "                          This is also the default action.\n"
		   "  -p, --plain        Extract data without decrypting.\n"
		   "  -k, --key=file     Specify key file.\n"
		   "CXI/CCI options:\n"
		   "  -n, --ncch=offs    Specify offset for NCCH header.\n"
		   "  --exefs=file       Specify ExeFS filepath.\n"
		   "  --romfs=file       Specify RomFS filepath.\n"
		   "  --exheader=file    Specify Extended Header filepath.\n"
		   "CIA options:\n"
		   "  --certs=file       Specify Certificate chain filepath.\n"
		   "  --tik=file         Specify Ticket filepath.\n"
		   "  --tmd=file         Specify TMD filepath.\n"
		   "  --contents=file    Specify Contents filepath.\n"
		   "  --banner=file      Specify Banner filepath.\n"
           "\n",
		   argv0);
   exit(1);
}

static u32 align(u32 offset, u32 alignment)
{
	u32 mask = ~(alignment-1);

	return (offset + (alignment-1)) & mask;
}

u64 getle64(const u8* p)
{
	u64 n = p[0];

	n |= (u64)p[1]<<8;
	n |= (u64)p[2]<<16;
	n |= (u64)p[3]<<24;
	n |= (u64)p[4]<<32;
	n |= (u64)p[5]<<40;
	n |= (u64)p[6]<<48;
	n |= (u64)p[7]<<56;
	return n;
}

u32 getle32(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

u32 getle16(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8);
}

void readkeyfile(u8* key, const char* keyfname)
{
	FILE* f = fopen(keyfname, "rb");
	u32 keysize = 0;

	if (0 == f)
	{
		fprintf(stdout, "Error opening key file\n");
		goto clean;
	}

	fseek(f, 0, SEEK_END);
	keysize = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (keysize != 16)
	{
		fprintf(stdout, "Error key size mismatch, got %d, expected %d\n", keysize, 16);
		goto clean;
	}

	if (16 != fread(key, 1, 16, f))
	{
		fprintf(stdout, "Error reading key file\n");
		goto clean;
	}

clean:
	if (f)
		fclose(f);
}


void memdump(FILE* fout, const char* prefix, const u8* data, u32 size)
{
	u32 i;
	u32 prefixlen = strlen(prefix);
	u32 offs = 0;
	u32 line = 0;
	while(size)
	{
		u32 max = 16;

		if (max > size)
			max = size;

		if (line==0)
			fprintf(fout, "%s", prefix);
		else
			fprintf(fout, "%*s", prefixlen, "");


		for(i=0; i<max; i++)
			fprintf(fout, "%02X", data[offs+i]);
		fprintf(fout, "\n");
		line++;
		size -= max;
		offs += max;
	}
}

void decode_ncch_header(const ctr_ncchheader* header, u32 offset)
{
	char magic[5];
	char productcode[0x11];

	memcpy(magic, header->magic, 4);
	magic[4] = 0;
	memcpy(productcode, header->productcode, 0x10);
	productcode[0x10] = 0;

	fprintf(stdout, "Header:                 %s\n", magic);
	memdump(stdout, "Signature:              ", header->signature, 0x100);       
	fprintf(stdout, "Content size:           0x%08x\n", getle32(header->contentsize)*0x200);
	fprintf(stdout, "Partition id:           %016llx\n", getle64(header->partitionid));
	fprintf(stdout, "Maker code:             %04x\n", getle16(header->makercode));
	fprintf(stdout, "Version:                %04x\n", getle16(header->version));
	fprintf(stdout, "Program id:             %016llx\n", getle64(header->programid));
	fprintf(stdout, "Temp flag:              %02x\n", header->tempflag);
	fprintf(stdout, "Product code:           %s\n", productcode);
	memdump(stdout, "Extended header hash:   ", header->extendedheaderhash, 0x20);
	fprintf(stdout, "Extended header size:   %08x\n", getle32(header->extendedheadersize));
	fprintf(stdout, "Flags:                  %016llx\n", getle64(header->flags));
	fprintf(stdout, "Plain region offset:    0x%08x\n", getle32(header->plainregionsize)? offset+getle32(header->plainregionoffset)*0x200 : 0);
	fprintf(stdout, "Plain region size:      0x%08x\n", getle32(header->plainregionsize)*0x200);
	fprintf(stdout, "ExeFS offset:           0x%08x\n", getle32(header->exefssize)? offset+getle32(header->exefsoffset)*0x200 : 0);
	fprintf(stdout, "ExeFS size:             0x%08x\n", getle32(header->exefssize)*0x200);
	fprintf(stdout, "ExeFS hash region size: 0x%08x\n", getle32(header->exefshashregionsize)*0x200);
	fprintf(stdout, "RomFS offset:           0x%08x\n", getle32(header->romfssize)? offset+getle32(header->romfsoffset)*0x200 : 0);
	fprintf(stdout, "RomFS size:             0x%08x\n", getle32(header->romfssize)*0x200);
	fprintf(stdout, "RomFS hash region size: 0x%08x\n", getle32(header->romfshashregionsize)*0x200);
	memdump(stdout, "ExeFS Superblock Hash:  ", header->exefssuperblockhash, 0x20);
	memdump(stdout, "RomFS Superblock Hash:  ", header->romfssuperblockhash, 0x20);
}


void decode_cia_header(const ctr_ciaheader* header)
{
	fprintf(stdout, "Header size             0x%08x\n", getle32(header->headersize));
	fprintf(stdout, "Type                    %04x\n", getle16(header->type));
	fprintf(stdout, "Version                 %04x\n", getle16(header->version));
	fprintf(stdout, "Certificate chain size  0x%04x\n", getle32(header->certsize));
	fprintf(stdout, "Ticket size             0x%04x\n", getle32(header->ticketsize));
	fprintf(stdout, "TMD size                0x%04x\n", getle32(header->tmdsize));
	fprintf(stdout, "Metasize                0x%04x\n", getle32(header->metasize));
	fprintf(stdout, "Contentsize             0x%016llx\n", getle64(header->contentsize));
}

void decrypt_ncch(toolcontext* ctx, u32 ncchoffset, const ctr_ncchheader* header, u32 type)
{
	u32 size = 0;
	u32 offset = 0;
	FILE* fout = 0;
	u8 buffer[16 * 1024];
	u8 counter[16];
	u32 i;
	char* outfname = 0;


	if (type == NCCHTYPE_EXEFS)
	{
		offset = ncchoffset + getle32(header->exefsoffset) * 0x200;
		size = getle32(header->exefssize) * 0x200;
		outfname = ctx->exefsfname;
	}
	else if (type == NCCHTYPE_ROMFS)
	{
		offset = ncchoffset + getle32(header->romfsoffset) * 0x200;
		size = getle32(header->romfssize) * 0x200;
		outfname = ctx->romfsfname;
	}
	else if (type == NCCHTYPE_EXTHEADER)
	{
		offset = ncchoffset + 0x200;
		size = getle32(header->extendedheadersize);
		outfname = ctx->exheaderfname;
	}
	else
	{
		fprintf(stderr, "Error invalid NCCH type\n");
		goto clean;
	}

	fout = fopen(outfname, "wb");
	if (0 == fout)
	{
		fprintf(stdout, "Error opening out file %s\n", outfname);
		goto clean;
	}

	fseek(ctx->infile, offset, SEEK_SET);

	memset(counter, 0, 16);
	for(i=0; i<8; i++)
		counter[i] = header->partitionid[7-i];
	counter[8] = type;

	ctr_init_counter(&ctx->cryptoctx, ctx->key, counter);

	while(size)
	{
		u32 max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->infile))
		{
			fprintf(stdout, "Error reading file\n");
			goto clean;
		}

		if (0 == (ctx->actions & NoDecrypt))
			ctr_crypt_counter(&ctx->cryptoctx, buffer, buffer, max);

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

void process_ncch(toolcontext* ctx, u32 ncchoffset)
{
	ctr_ncchheader ncchheader;


	if (ncchoffset == ~0)
	{
		if (ctx->filetype == FILETYPE_CCI)
			ncchoffset = 0x4000;
		else if (ctx->filetype == FILETYPE_CXI)
			ncchoffset = 0;
	}

	fseek(ctx->infile, ncchoffset, SEEK_SET);

	fread(&ncchheader, 1, 0x200, ctx->infile);

	if (ctx->actions & Info)
		decode_ncch_header(&ncchheader, ncchoffset);
	if (ctx->actions & Extract)
	{
		if (ctx->setexefsfname)
		{
			fprintf(stdout, "Decrypting ExeFS...\n");
			decrypt_ncch(ctx, ncchoffset, &ncchheader, NCCHTYPE_EXEFS);
		}
		if (ctx->setromfsfname)
		{
			fprintf(stdout, "Decrypting RomFS...\n");
			decrypt_ncch(ctx, ncchoffset, &ncchheader, NCCHTYPE_ROMFS);
		}
		if (ctx->setexheaderfname)
		{
			fprintf(stdout, "Decrypting Extended Header...\n");
			decrypt_ncch(ctx, ncchoffset, &ncchheader, NCCHTYPE_EXTHEADER);
		}
	}
}

void save_blob(toolcontext* ctx, u32 offset, u32 size, const char* outfname, u32 type)
{
	u8 buffer[16*1024];
	FILE* fout = 0;

	fseek(ctx->infile, offset, SEEK_SET);

	fout = fopen(outfname, "wb");
	if (0 == fout)
	{
		fprintf(stdout, "Error opening out file %s\n", outfname);
		goto clean;
	}

	if (type == CBC)
	{
		ctr_init_cbc_decrypt(&ctx->cryptoctx, ctx->key, ctx->iv);
	}


	while(size)
	{
		u32 max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, ctx->infile))
		{
			fprintf(stdout, "Error reading file\n");
			goto clean;
		}

		if (type == CBC)
		{
			ctr_decrypt_cbc(&ctx->cryptoctx, buffer, buffer, max);
		}

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

void process_cia(toolcontext* ctx)
{
	ctr_ciaheader ciaheader;
	u32 cryptotype;
	u8 titleid[16];
	u8 titlekey[16];
	u32 offsetcerts = 0;
	u32 offsettik = 0;
	u32 offsettmd = 0;
	u32 offsetmeta = 0;
	u32 offsetcontent = 0;

	fseek(ctx->infile, 0, SEEK_SET);

	if (fread(&ciaheader, 1, sizeof(ciaheader), ctx->infile) != sizeof(ciaheader))
	{
		fprintf(stderr, "Error reading CIA header\n");
		goto clean;
	}

	if (ctx->actions & Info)
		decode_cia_header(&ciaheader);

	offsetcerts = align(getle32(ciaheader.headersize), 64);
	offsettik = align(offsetcerts + getle32(ciaheader.certsize), 64);
	offsettmd = align(offsettik + getle32(ciaheader.ticketsize), 64);
	offsetcontent = align(offsettmd + getle32(ciaheader.tmdsize), 64);
	offsetmeta = align(offsetcontent + getle32(ciaheader.contentsize), 64);
	fseek(ctx->infile, offsettik + 0x1BF, SEEK_SET);
	fread(titlekey, 1, 16, ctx->infile);
	fseek(ctx->infile, offsettik + 0x1DC, SEEK_SET);
	fread(titleid, 1, 16, ctx->infile);

	if (ctx->actions & Info)
	{
		fprintf(stdout, "Certificates offset:    0x%08x\n", offsetcerts);
		fprintf(stdout, "Ticket offset:          0x%08x\n", offsettik);
		fprintf(stdout, "TMD offset:             0x%08x\n", offsettmd);
		fprintf(stdout, "Content offset:         0x%08x\n", offsetcontent);
		fprintf(stdout, "Banner offset:          0x%08x\n", offsetmeta);
		memdump(stdout, "Title ID:               ", titleid, 0x10);
		memdump(stdout, "Encrypted title key:    ", titlekey, 0x10);
	}


	if (ctx->actions & Extract)
	{
		if (ctx->setcertsfname)
		{
			fprintf(stdout, "Saving certificate chain to %s...\n", ctx->certsfname);
			save_blob(ctx, offsetcerts, getle32(ciaheader.certsize), ctx->certsfname, Plain);
		}

		if (ctx->settikfname)
		{
			fprintf(stdout, "Saving ticket to %s...\n", ctx->tikfname);
			save_blob(ctx, offsettik, getle32(ciaheader.ticketsize), ctx->tikfname, Plain);
		}

		if (ctx->settmdfname)
		{
			fprintf(stdout, "Saving TMD to %s...\n", ctx->tmdfname);
			save_blob(ctx, offsettmd, getle32(ciaheader.tmdsize), ctx->tmdfname, Plain);
		}

		if (ctx->setcontentsfname)
		{
			

			// Larger than 4GB content? Seems unlikely..
			if (ctx->actions & NoDecrypt)
			{
				fprintf(stdout, "Saving content to %s...\n", ctx->contentsfname);
				cryptotype = Plain;
			}
			else
			{
				cryptotype = CBC;
				memcpy(ctx->iv, titleid, 16);
				fprintf(stdout, "Decrypting content to %s...\n", ctx->contentsfname);
			}			

			save_blob(ctx, offsetcontent, (u32)getle64(ciaheader.contentsize), ctx->contentsfname, cryptotype);
		}

		if (ctx->setbannerfname)
		{
			fprintf(stdout, "Saving banner to %s...\n", ctx->bannerfname);
			save_blob(ctx, offsetmeta, getle32(ciaheader.metasize), ctx->bannerfname, Plain);
		}
	}

clean:
	return;
}

int main(int argc, char* argv[])
{
	toolcontext ctx;
	u8 magic[4];
	char infname[512];
	int c;
	u32 ncchoffset = ~0;
	u8 key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	memset(&ctx, 0, sizeof(toolcontext));
	ctx.actions = Info | Extract;
	ctx.filetype = FILETYPE_UNKNOWN;


	while (1) 
	{
		int option_index;
		static struct option long_options[] = 
		{
			{"extract", 0, NULL, 'x'},
			{"plain", 0, NULL, 'p'},
			{"info", 0, NULL, 'i'},
			{"exefs", 1, NULL, 0},
			{"romfs", 1, NULL, 1},
			{"exheader", 1, NULL, 2},
			{"certs", 1, NULL, 3},
			{"tik", 1, NULL, 4},
			{"tmd", 1, NULL, 5},
			{"contents", 1, NULL, 6},
			{"banner", 1, NULL, 7},
			{"key", 1, NULL, 'k'},
			{"ncch", 1, NULL, 'n'},
			{NULL},
		};

		c = getopt_long(argc, argv, "dik:n:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{
			case 'x':
				ctx.actions |= Extract;
			break;

			case 'p':
				ctx.actions |= NoDecrypt;
			break;

			case 'i':
				ctx.actions |= Info;
			break;

			case 'n':
				ncchoffset = strtoul(optarg, 0, 0);
			break;

			case 'k':
				readkeyfile(key, optarg);
			break;

			case 0:
				strncpy(ctx.exefsfname, optarg, sizeof(ctx.exefsfname));
				ctx.setexefsfname = 1;
			break;

			case 1:
				strncpy(ctx.romfsfname, optarg, sizeof(ctx.romfsfname));
				ctx.setromfsfname = 1;
			break;

			case 2:
				strncpy(ctx.exheaderfname, optarg, sizeof(ctx.exheaderfname));
				ctx.setexheaderfname = 1;
			break;

			case 3:
				strncpy(ctx.certsfname, optarg, sizeof(ctx.certsfname));
				ctx.setcertsfname = 1;
			break;

			case 4:
				strncpy(ctx.tikfname, optarg, sizeof(ctx.tikfname));
				ctx.settikfname = 1;
			break;

			case 5:
				strncpy(ctx.tmdfname, optarg, sizeof(ctx.tmdfname));
				ctx.settmdfname = 1;
			break;

			case 6:
				strncpy(ctx.contentsfname, optarg, sizeof(ctx.contentsfname));
				ctx.setcontentsfname = 1;
			break;

			case 7:
				strncpy(ctx.bannerfname, optarg, sizeof(ctx.bannerfname));
				ctx.setbannerfname = 1;
			break;

			default:
				usage(argv[0]);
		}
	}

	if (optind == argc - 1) 
	{
		// Exactly one extra argument - an input file
		strncpy(infname, argv[optind], sizeof(infname));
	} 
	else if ( (optind < argc) || (argc == 1) )
	{
		// Too many extra args
		usage(argv[0]);
	}

	memcpy(ctx.key, key, 16);

	ctx.infile = fopen(infname, "rb");

	if (!ctx.infile)
		goto clean;

	fseek(ctx.infile, 0x100, SEEK_SET);
	fread(&magic, 1, 4, ctx.infile);

	switch(getle32(magic))
	{
		case MAGIC_NCCH:
			ctx.filetype = FILETYPE_CXI;
		break;

		case MAGIC_NCSD:
			ctx.filetype = FILETYPE_CCI;
		break;

		default:
			fseek(ctx.infile, 0, SEEK_SET);
			fread(magic, 1, 4, ctx.infile);
			if (getle32(magic) == 0x2020)
				ctx.filetype = FILETYPE_CIA;
		break;
	}

	if (ctx.filetype == FILETYPE_UNKNOWN)
	{
		fprintf(stdout, "Unknown file\n");
		exit(1);
	}

	switch(ctx.filetype)
	{
		case FILETYPE_CXI:
		case FILETYPE_CCI:
			process_ncch(&ctx, ncchoffset);
		break;

		case FILETYPE_CIA:
			process_cia(&ctx);
		break;
	}
	

clean:
	if (ctx.infile)
		fclose(ctx.infile);

	return 0;
}
