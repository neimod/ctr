#ifndef _INFO_H_
#define _INFO_H_

#include "types.h"
#include "keyset.h"

typedef struct
{
	FILE* file;
	keyset* keys;
	u32 offset;
	const u8* blob;
	u32 blobsize;
} infocontext;

#endif // _INFO_H_
