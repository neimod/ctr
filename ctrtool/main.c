#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "utils.h"
#include "ctr.h"
#include "ncch.h"
#include "ncsd.h"
#include "cia.h"
#include "tmd.h"
#include "tik.h"
#include "keyset.h"
#include "exefs.h"
#include "info.h"

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
	FILE* infile;
	keyset keys;
	ncch_context ncch;
	cia_context cia;
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
		   "  -k, --keyset=file  Specify keyset file.\n"
		   "  -v, --verbose      Give verbose output.\n"
		   "  -y, --verify       Verify hashes and signatures.\n"
		   "CXI/CCI options:\n"
		   "  -n, --ncch=offs    Specify offset for NCCH header.\n"
		   "  --exefs=file       Specify ExeFS file path.\n"
		   "  --exefsdir=dir     Specify ExeFS directory path.\n"
		   "  --romfs=file       Specify RomFS file path.\n"
		   "  --exheader=file    Specify Extended Header file path.\n"
		   "CIA options:\n"
		   "  --certs=file       Specify Certificate chain file path.\n"
		   "  --tik=file         Specify Ticket file path.\n"
		   "  --tmd=file         Specify TMD file path.\n"
		   "  --contents=file    Specify Contents file path.\n"
		   "  --meta=file        Specify Meta file path.\n"
           "\n",
		   argv0);
   exit(1);
}


int main(int argc, char* argv[])
{
	toolcontext ctx;
	u8 magic[4];

	char infname[512];
	int c;
	u32 ncchoffset = ~0;
	u8 key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	char keysetfname[512] = "keys.xml";
	
	memset(&ctx, 0, sizeof(toolcontext));
	ctx.actions = InfoFlag | ExtractFlag;
	ctx.filetype = FILETYPE_UNKNOWN;

	keyset_init(&ctx.keys);
	ncch_init(&ctx.ncch);
	cia_init(&ctx.cia);
	

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
			{"meta", 1, NULL, 7},
			{"exefsdir", 1, NULL, 8},
			{"keyset", 1, NULL, 'k'},
			{"ncch", 1, NULL, 'n'},
			{"verbose", 0, NULL, 'v'},
			{"verify", 0, NULL, 'y'},
			{NULL},
		};

		c = getopt_long(argc, argv, "yxivpk:n:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{
			case 'x':
				ctx.actions |= ExtractFlag;
			break;

			case 'v':
				ctx.actions |= VerboseFlag;
			break;

			case 'y':
				ctx.actions |= VerifyFlag;
			break;

			case 'p':
				ctx.actions |= PlainFlag;
			break;

			case 'i':
				ctx.actions |= InfoFlag;
			break;

			case 'n':
				ncchoffset = strtoul(optarg, 0, 0);
			break;

			case 'k':
				strncpy(keysetfname, optarg, sizeof(keysetfname));
			break;

			case 0: ncch_set_exefspath(&ctx.ncch, optarg); break;
			case 1: ncch_set_romfspath(&ctx.ncch, optarg); break;
			case 2: ncch_set_exheaderpath(&ctx.ncch, optarg); break;
			case 3: cia_set_certspath(&ctx.cia, optarg); break;
			case 4: cia_set_tikpath(&ctx.cia, optarg); break;
			case 5: cia_set_tmdpath(&ctx.cia, optarg); break;
			case 6: cia_set_contentpath(&ctx.cia, optarg); break;
			case 7: cia_set_metapath(&ctx.cia, optarg); break;
			case 8: ncch_set_exefsdirpath(&ctx.ncch, optarg); break;


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

	keyset_load(&ctx.keys, keysetfname, ctx.actions & VerboseFlag);

	ctx.infile = fopen(infname, "rb");

	if (ctx.infile == 0)
		goto clean;

	ncch_set_file(&ctx.ncch, ctx.infile);
	ncch_set_key(&ctx.ncch, ctx.keys.ncchctrkey.data);
	cia_set_file(&ctx.cia, ctx.infile);
	cia_set_offset(&ctx.cia, 0);
	if (ctx.keys.commonkey.valid)
		cia_set_commonkey(&ctx.cia, ctx.keys.commonkey.data);



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
	
	if (ncchoffset == ~0)
	{
		if (ctx.filetype == FILETYPE_CCI)
			ncch_set_offset(&ctx.ncch, 0x4000);
		else if (ctx.filetype == FILETYPE_CXI)
			ncch_set_offset(&ctx.ncch, 0);
	}

	switch(ctx.filetype)
	{
		case FILETYPE_CCI:
		{
			//	if (ctx->filetype == FILETYPE_CCI)
			//	{
			//		unsigned char ncsdblob[0x200];
			//
			//		fseek(ctx->infile, 0, SEEK_SET);
			//		fread(ncsdblob, 1, 0x200, ctx->infile);
			//		ncsd_print(ncsdblob, 0x200, &ctx->keys);
			//	}
		}
		case FILETYPE_CXI:
			ncch_process(&ctx.ncch, ctx.actions);
		break;

		case FILETYPE_CIA:
			cia_process(&ctx.cia, ctx.actions);
		break;
	}
	

clean:
	if (ctx.infile)
		fclose(ctx.infile);

	return 0;
}
