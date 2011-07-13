#ifndef __CIA_H__
#define __CIA_H__

typedef struct
{
	u8 headersize[4];
	u8 type[2];
	u8 version[2];
	u8 certsize[4];
	u8 ticketsize[4];
	u8 tmdsize[4];
	u8 metasize[4];
	u8 contentsize[8];
	u8 contentindex[0x2000];
} ctr_ciaheader;

void cia_print(const u8* blob);

#endif
