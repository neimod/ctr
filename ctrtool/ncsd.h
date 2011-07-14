#ifndef _NCSD_H_
#define _NCSD_H_

#include "types.h"
#include "keyset.h"


typedef struct
{
	u8 signature[0x100];
	u8 magic[4];
	u8 mediasize[4];
	u8 mediaid[8];
	u8 partitionfstype[8];
	u8 partitioncrypttype[8];
	u8 partitionoffsetandsize[0x40];
	u8 extendedheaderhash[0x20];
	u8 additionalheadersize[4];
	u8 sectorzerooffset[4];
	u8 flags[8];
	u8 partitionid[0x40];
	u8 reserved[0x30];
} ctr_ncsdheader;

int ncsd_signature_verify(const u8* blob, u32 size, rsakey2048* key);
void ncsd_print(const u8 *blob, u32 size, keyset* keys);

#endif // _NCSD_H_
