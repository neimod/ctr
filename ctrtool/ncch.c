#include <stdio.h>
#include <string.h>

#include "types.h"
#include "ncch.h"
#include "utils.h"
#include "ctr.h"

int ncch_signature_verify(const u8* blob, u32 size, rsakey2048* key)
{
	u8 hash[0x20];
	u8 output[0x100];

	ctr_rsa_public(blob, output, key);

	memdump(stdout, "RSA decrypted:      ", output, 0x100);

	ctr_sha_256(blob + 0x100, 0x100, hash);
	return ctr_rsa_verify_hash(blob, hash, key);
}


void ncch_print(const u8 *blob, u32 size, u32 offset, keyset* keys)
{
	char magic[5];
	char productcode[0x11];
	ctr_ncchheader *header = (ctr_ncchheader*)blob;
	int sigcheck;


	sigcheck = ncch_signature_verify(blob, size, &keys->ncchrsakey);
	if (!sigcheck)
		sigcheck = ncch_signature_verify(blob, size, &keys->ncchdlprsakey);
	if (!sigcheck)
		sigcheck = ncch_signature_verify(blob, size, &keys->crrrsakey);
	if (!sigcheck)
		sigcheck = ncch_signature_verify(blob, size, &keys->ncsdrsakey);

	memcpy(magic, header->magic, 4);
	magic[4] = 0;
	memcpy(productcode, header->productcode, 0x10);
	productcode[0x10] = 0;

	fprintf(stdout, "Header:                 %s\n", magic);
	if (sigcheck)
		memdump(stdout, "Signature (GOOD):       ", header->signature, 0x100);
	else
		memdump(stdout, "Signature (FAIL):       ", header->signature, 0x100);
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
