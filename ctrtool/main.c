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
#include "lzss.h"
#include "keyset.h"
#include "exefs.h"
#include "info.h"
#include "settings.h"

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
	u32 infilesize;
	settings usersettings;
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
		   "  -r, --raw          Keep raw data, don't unpack.\n"
		   "  -k, --keyset=file  Specify keyset file.\n"
		   "  -v, --verbose      Give verbose output.\n"
		   "  -y, --verify       Verify hashes and signatures.\n"
		   "  --unitsize=size    Set media unit size (default 0x200).\n"
		   "  --commonkey=key    Set common key.\n"
		   "  --ncchctrkey=key   Set ncchctr key.\n"
		   "  --showkeys         Show the keys being used.\n"
		   "  -t, --intype=type	 Specify input file type [ncsd, ncch, exheader, cia, tmd]\n"
		   "LZSS options:\n"
		   "  --lzssout=file	 Specify lzss output file\n"
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
	char keysetfname[512] = "keys.xml";
	keyset tmpkeys;
	unsigned int checkkeysetfile = 0;
	
	memset(&ctx, 0, sizeof(toolcontext));
	ctx.actions = InfoFlag | ExtractFlag;
	ctx.filetype = FILETYPE_UNKNOWN;

	settings_init(&ctx.usersettings);
	keyset_init(&ctx.usersettings.keys);
	keyset_init(&tmpkeys);


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
			{"raw", 0, NULL, 'r'},
			{"unitsize", 1, NULL, 9},
			{"showkeys", 0, NULL, 10},
			{"commonkey", 1, NULL, 11},
			{"ncchctrkey", 1, NULL, 12},
			{"intype", 1, NULL, 't'},
			{"lzssout", 1, NULL, 13},
			{NULL},
		};

		c = getopt_long(argc, argv, "ryxivpk:n:t:", long_options, &option_index);
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

			case 'r':
				ctx.actions |= RawFlag;
			break;

			case 'i':
				ctx.actions |= InfoFlag;
			break;

			case 'n':
				ncchoffset = strtoul(optarg, 0, 0);
			break;

			case 'k':
				strncpy(keysetfname, optarg, sizeof(keysetfname));
				checkkeysetfile = 1;
			break;

			case 't':
				if (!strcmp(optarg, "exheader"))
					ctx.filetype = FILETYPE_EXHEADER;
				else if (!strcmp(optarg, "ncch"))
					ctx.filetype = FILETYPE_CXI;
				else if (!strcmp(optarg, "ncsd"))
					ctx.filetype = FILETYPE_CCI;
				else if (!strcmp(optarg, "cia"))
					ctx.filetype = FILETYPE_CIA;
				else if (!strcmp(optarg, "tmd"))
					ctx.filetype = FILETYPE_TMD;
				else if (!strcmp(optarg, "lzss"))
					ctx.filetype = FILETYPE_LZSS;
			break;

			case 0: settings_set_exefs_path(&ctx.usersettings, optarg); break;
			case 1: settings_set_romfs_path(&ctx.usersettings, optarg); break;
			case 2: settings_set_exheader_path(&ctx.usersettings, optarg); break;
			case 3: settings_set_certs_path(&ctx.usersettings, optarg); break;
			case 4: settings_set_tik_path(&ctx.usersettings, optarg); break;
			case 5: settings_set_tmd_path(&ctx.usersettings, optarg); break;
			case 6: settings_set_content_path(&ctx.usersettings, optarg); break;
			case 7: settings_set_content_path(&ctx.usersettings, optarg); break;
			case 8: settings_set_exefs_dir_path(&ctx.usersettings, optarg); break;
			case 9: settings_set_mediaunit_size(&ctx.usersettings, strtoul(optarg, 0, 0)); break;
			case 10: ctx.actions |= ShowKeysFlag; break;
			case 11: keyset_parse_commonkey(&tmpkeys, optarg, strlen(optarg)); break;
			case 12: keyset_parse_ncchctrkey(&tmpkeys, optarg, strlen(optarg)); break;
			case 13: settings_set_lzss_path(&ctx.usersettings, optarg); break;


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

	keyset_load(&ctx.usersettings.keys, keysetfname, (ctx.actions & VerboseFlag) | checkkeysetfile);
	keyset_merge(&ctx.usersettings.keys, &tmpkeys);
	if (ctx.actions & ShowKeysFlag)
		keyset_dump(&ctx.usersettings.keys);

	ctx.infile = fopen(infname, "rb");

	if (ctx.infile == 0) 
	{
		fprintf(stderr, "error: could not open input file!\n");
		return -1;
	}

	fseek(ctx.infile, 0, SEEK_END);
	ctx.infilesize = ftell(ctx.infile);
	fseek(ctx.infile, 0, SEEK_SET);




	if (ctx.filetype == FILETYPE_UNKNOWN)
	{
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
	}

	if (ctx.filetype == FILETYPE_UNKNOWN)
	{
		fprintf(stdout, "Unknown file\n");
		exit(1);
	}
	
//	if (ncchoffset == ~0)
//	{
//		if (ctx.filetype == FILETYPE_CCI)
//			ncch_set_offset(&ctx.ncch, 0x4000);
//		else if (ctx.filetype == FILETYPE_CXI)
//			ncch_set_offset(&ctx.ncch, 0);
//	}

	switch(ctx.filetype)
	{
		case FILETYPE_CCI:
		{
			ncsd_context ncsdctx;

			ncsd_init(&ncsdctx);
			ncsd_set_file(&ncsdctx, ctx.infile);
			ncsd_set_size(&ncsdctx, ctx.infilesize);
			ncsd_set_usersettings(&ncsdctx, &ctx.usersettings);
			ncsd_process(&ncsdctx, ctx.actions);
			
			break;			
		}
		
		case FILETYPE_CXI:
		{
			ncch_context ncchctx;

			ncch_init(&ncchctx);
			ncch_set_file(&ncchctx, ctx.infile);
			ncch_set_size(&ncchctx, ctx.infilesize);
			ncch_set_usersettings(&ncchctx, &ctx.usersettings);
			ncch_process(&ncchctx, ctx.actions);

			break;
		}
		

		case FILETYPE_CIA:
		{
			cia_context ciactx;

			cia_init(&ciactx);
			cia_set_file(&ciactx, ctx.infile);
			cia_set_size(&ciactx, ctx.infilesize);
			cia_set_usersettings(&ciactx, &ctx.usersettings);
			cia_process(&ciactx, ctx.actions);

			break;
		}

		case FILETYPE_EXHEADER:
		{
			exheader_context exheaderctx;

			exheader_init(&exheaderctx);
			exheader_set_file(&exheaderctx, ctx.infile);
			exheader_set_size(&exheaderctx, ctx.infilesize);
			settings_set_ignore_programid(&ctx.usersettings, 1);

			exheader_set_usersettings(&exheaderctx, &ctx.usersettings);
			exheader_process(&exheaderctx, ctx.actions);
	
			break;
		}

		case FILETYPE_TMD:
		{
			tmd_context tmdctx;

			tmd_init(&tmdctx);
			tmd_set_file(&tmdctx, ctx.infile);
			tmd_set_size(&tmdctx, ctx.infilesize);
			tmd_set_usersettings(&tmdctx, &ctx.usersettings);
			tmd_process(&tmdctx, ctx.actions);
	
			break;
		}

		case FILETYPE_LZSS:
		{
			lzss_context lzssctx;

			lzss_init(&lzssctx);
			lzss_set_file(&lzssctx, ctx.infile);
			lzss_set_size(&lzssctx, ctx.infilesize);
			lzss_set_usersettings(&lzssctx, &ctx.usersettings);
			lzss_process(&lzssctx, ctx.actions);
	
			break;
		}
	}
	
	if (ctx.infile)
		fclose(ctx.infile);

	return 0;
}
