#include <stdio.h>
#include <string.h>

#include "types.h"
#include "ncch.h"
#include "utils.h"


void ncch_print(const u8 *blob, u32 offset)
{
	char magic[5];
	char productcode[0x11];
	ctr_ncchheader *header = (ctr_ncchheader*)blob;


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
