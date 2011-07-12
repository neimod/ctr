#ifndef __NCCH_H__
#define __NCCH_H__

typedef enum
{
	NCCHTYPE_EXTHEADER = 1,
	NCCHTYPE_EXEFS = 2,
	NCCHTYPE_ROMFS = 3,
} ctr_ncchtypes;

typedef struct
{
	u8 signature[0x100];
	u8 magic[4];
	u8 contentsize[4];
	u8 partitionid[8];
	u8 makercode[2];
	u8 version[2];
	u8 reserved0[4];
	u8 programid[8];
	u8 tempflag;
	u8 reserved1[0x2f];
	u8 productcode[0x10];
	u8 extendedheaderhash[0x20];
	u8 extendedheadersize[4];
	u8 reserved2[4];
	u8 flags[8];
	u8 plainregionoffset[4];
	u8 plainregionsize[4];
	u8 reserved3[8];
	u8 exefsoffset[4];
	u8 exefssize[4];
	u8 exefshashregionsize[4];
	u8 reserved4[4];
	u8 romfsoffset[4];
	u8 romfssize[4];
	u8 romfshashregionsize[4];
	u8 reserved5[4];
	u8 exefssuperblockhash[0x20];
	u8 romfssuperblockhash[0x20];
} ctr_ncchheader;

#endif
