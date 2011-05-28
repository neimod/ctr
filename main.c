#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ctr.h"


static void usage(const char *argv0)
{
	fprintf(stderr,
		   "Usage: %s [options...] <file>\n"
		   "CTRTOOL (c) neimod.\n"
           "\n"
           "Options:\n"
           "  -i, --info         Show file info.\n"
		   "                          This is the default action.\n"
           "  -d, --decrypt      Decrypt file.\n"
		   "  -k, --key=file     Specify key file.\n"
		   "  --exefs=file       Specify ExeFS filepath.\n"
		   "  --romfs=file       Specify RomFS filepath.\n"
		   "  --exheader=file    Specify Extended Header filepath.\n"
           "\n",
		   argv0);
   exit(1);
}

unsigned long long getle64(const unsigned char* p)
{
	unsigned long long n = p[0];

	n |= (unsigned long long)p[1]<<8;
	n |= (unsigned long long)p[2]<<16;
	n |= (unsigned long long)p[3]<<24;
	n |= (unsigned long long)p[4]<<32;
	n |= (unsigned long long)p[5]<<40;
	n |= (unsigned long long)p[6]<<48;
	n |= (unsigned long long)p[7]<<56;
	return n;
}

unsigned int getle32(const unsigned char* p)
{
	return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

unsigned int getle16(const unsigned char* p)
{
	return (p[0]<<0) | (p[1]<<8);
}

void readkeyfile(unsigned char* key, const char* keyfname)
{
	FILE* f = fopen(keyfname, "rb");
	unsigned int keysize = 0;

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


void memdump(FILE* fout, const char* prefix, const unsigned char* data, unsigned int size)
{
	unsigned int i;
	unsigned int prefixlen = strlen(prefix);
	unsigned int offs = 0;
	unsigned int line = 0;
	while(size)
	{
		unsigned int max = 16;

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

void decode_ncch_header(const ctr_ncchheader* header, unsigned int offset)
{
	char magic[5];
	char productcode[0x11];

	memcpy(magic, header->magic, 4);
	magic[4] = 0;
	memcpy(productcode, header->productcode, 0x10);
	productcode[0x10] = 0;

	fprintf(stdout, "Header:                 %s\n", magic);
	memdump(stdout, "Signature:              ", header->signature, 0x100);       
	fprintf(stdout, "Content size:           0x%08x\n", getle32(header->contentsize));
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

void decrypt_ncch(FILE* f, ctr_crypto_context* ctx, unsigned int ncchoffset, const ctr_ncchheader* header, const char* outfname, unsigned int type)
{
	unsigned int size = 0;
	unsigned int offset = 0;
	FILE* fout = 0;
	unsigned char buffer[16 * 1024];
	unsigned char counter[16];
	unsigned int i;

	if (type == NCCHTYPE_EXEFS)
	{
		offset = ncchoffset + getle32(header->exefsoffset) * 0x200;
		size = getle32(header->exefssize) * 0x200;
	}
	else if (type == NCCHTYPE_ROMFS)
	{
		offset = ncchoffset + getle32(header->romfsoffset) * 0x200;
		size = getle32(header->romfssize) * 0x200;
	}
	else if (type == NCCHTYPE_EXTHEADER)
	{
		offset = ncchoffset + 0x200;
		size = getle32(header->extendedheadersize);
	}

	fout = fopen(outfname, "wb");
	if (0 == fout)
	{
		fprintf(stdout, "Error opening out file %s\n", outfname);
		goto clean;
	}

	fseek(f, offset, SEEK_SET);

	memset(counter, 0, 16);
	for(i=0; i<8; i++)
		counter[i] = header->partitionid[7-i];
	counter[8] = type;

	ctr_set_counter(ctx, counter);

	while(size)
	{
		unsigned int max = sizeof(buffer);
		if (max > size)
			max = size;

		if (max != fread(buffer, 1, max, f))
		{
			fprintf(stdout, "Error reading file\n");
			goto clean;
		}

		ctr_crypt_counter(ctx, buffer, buffer, max);

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


int main(int argc, char* argv[])
{
	enum actionflags
	{
		Decrypt = (1<<0),
		Info = (1<<1),
	};

	FILE* f = 0;
	ctr_ncchheader ncchheader;
	unsigned char magic[4];
	unsigned int filetype;
	unsigned int ncchoffset = 0;
	char infname[512];
	char exheaderfname[512];
	char romfsfname[512];
	char exefsfname[512];
	int setexefsfname = 0;
	int setromfsfname = 0;
	int setexheaderfname = 0;
	int actions = Info;
	int c;
	ctr_crypto_context cryptoctx;
	unsigned char key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	

	while (1) 
	{
		int option_index;
		static struct option long_options[] = 
		{
			{"decrypt", 0, NULL, 'd'},
			{"info", 0, NULL, 'i'},
			{"exefs", 1, NULL, 0},
			{"romfs", 1, NULL, 1},
			{"exheader", 1, NULL, 2},
			{"key", 1, NULL, 'k'},
			{NULL},
		};

		c = getopt_long(argc, argv, "dik:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{
			case 'd':
				actions |= Decrypt;
			break;

			case 'i':
				actions |= Info;
			break;

			case 'k':
				readkeyfile(key, optarg);
			break;

			case 0:
				strncpy(exefsfname, optarg, sizeof(exefsfname));
				setexefsfname = 1;
			break;

			case 1:
				strncpy(romfsfname, optarg, sizeof(romfsfname));
				setromfsfname = 1;
			break;

			case 2:
				strncpy(exheaderfname, optarg, sizeof(exheaderfname));
				setexheaderfname = 1;
			break;

			default:
				usage(argv[0]);
		}
	}

	if (optind == argc - 1) 
	{
		// Exactly one extra argument- a trace file
		strncpy(infname, argv[optind], sizeof(infname));
	} 
	else if ( (optind < argc) || (argc == 1) )
	{
		// Too many extra args
		usage(argv[0]);
	}

	ctr_set_key(&cryptoctx, key);

	f = fopen(infname, "rb");

	if (!f)
		goto clean;

	fseek(f, 0x100, SEEK_SET);
	fread(&magic, 1, 4, f);

	switch(getle32(magic))
	{
		case MAGIC_NCCH:
			filetype = FILETYPE_CXI;
		break;

		case MAGIC_NCSD:
			filetype = FILETYPE_CCI;
		break;

		default:
			fprintf(stdout, "Unknown file\n");
			exit(1);
		break;
	}

	if (filetype == FILETYPE_CCI)
		ncchoffset = 0x4000;
	else if (filetype == FILETYPE_CXI)
		ncchoffset = 0;

	fseek(f, ncchoffset, SEEK_SET);

	fread(&ncchheader, 1, 0x200, f);

	if (actions & Info)
		decode_ncch_header(&ncchheader, ncchoffset);
	if (actions & Decrypt)
	{
		if (setexefsfname)
		{
			fprintf(stdout, "Decrypting ExeFS...\n");
			decrypt_ncch(f, &cryptoctx, ncchoffset, &ncchheader, exefsfname, NCCHTYPE_EXEFS);
		}
		if (setromfsfname)
		{
			fprintf(stdout, "Decrypting RomFS...\n");
			decrypt_ncch(f, &cryptoctx, ncchoffset, &ncchheader, romfsfname, NCCHTYPE_ROMFS);
		}
		if (setexheaderfname)
		{
			fprintf(stdout, "Decrypting Extended Header...\n");
			decrypt_ncch(f, &cryptoctx, ncchoffset, &ncchheader, exheaderfname, NCCHTYPE_EXTHEADER);
		}
	}
clean:
	if (f)
		fclose(f);

	return 0;
}
