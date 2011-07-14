#include <stdio.h>
#include <string.h>

#include "types.h"
#include "ncsd.h"
#include "utils.h"
#include "ctr.h"

int ncsd_signature_verify(const u8* blob, u32 size, rsakey2048* key)
{
	u8 hash[0x20];

	ctr_sha_256(blob + 0x100, 0x100, hash);
	return ctr_rsa_verify_hash(blob, hash, key);
}

void ncsd_print(const u8* blob, u32 size, keyset* keys)
{
	int sigcheck;
	char magic[5];
	ctr_ncsdheader* header = (ctr_ncsdheader*)blob;

	memcpy(magic, header->magic, 4);
	magic[4] = 0;
	sigcheck = ncsd_signature_verify(blob, size, &keys->ncsdrsakey);

	fprintf(stdout, "Header:                 %s\n", magic);
	if (sigcheck)
		memdump(stdout, "Signature (GOOD):       ", header->signature, 0x100);       
	else
		memdump(stdout, "Signature (FAIL):       ", header->signature, 0x100);       
	fprintf(stdout, "Media size:             0x%08x\n", getle32(header->mediasize));
	fprintf(stdout, "Media id:               %016llx\n", getle64(header->mediaid));
	memdump(stdout, "Partition FS type:      ", header->partitionfstype, 8);
	memdump(stdout, "Partition crypt type:   ", header->partitioncrypttype, 8);
	memdump(stdout, "Partition offset/size:  ", header->partitionoffsetandsize, 0x40);
	memdump(stdout, "Extended header hash:   ", header->extendedheaderhash, 0x20);
	memdump(stdout, "Additional header size: ", header->additionalheadersize, 4);
	memdump(stdout, "Sector zero offset:     ", header->sectorzerooffset, 4);
	memdump(stdout, "Flags:                  ", header->flags, 8);
	memdump(stdout, "Partition id:           ", header->partitionid, 0x40);
}
