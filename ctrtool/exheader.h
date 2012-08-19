#ifndef _EXHEADER_H_
#define _EXHEADER_H_

#include <stdio.h>
#include "types.h"
#include "ctr.h"
#include "settings.h"

typedef struct
{
	u8 reserved[5];
	u8 flag;
	u8 remasterversion[2];
} exheader_systeminfoflags;

typedef struct
{
	u8 address[4];
	u8 nummaxpages[4];
	u8 codesize[4];
} exheader_codesegmentinfo;

typedef struct
{
	u8 name[8];
	exheader_systeminfoflags flags;
	exheader_codesegmentinfo text;
	u8 stacksize[4];
	exheader_codesegmentinfo ro;
	u8 reserved[4];
	exheader_codesegmentinfo data;
	u8 bsssize[4];
} exheader_codesetinfo;

typedef struct
{
	u8 programid[0x30][8];
} exheader_dependencylist;

typedef struct
{
	u8 savedatasize[4];
	u8 reserved[4];
	u8 jumpid[8];
	u8 reserved2[0x30];
} exheader_systeminfo;

typedef struct
{
	u8 extsavedataid[8];
	u8 systemsavedataid[8];
	u8 reserved[8];
	u8 accessinfo[7];
	u8 otherattributes;
} exheader_storageinfo;

typedef struct
{
	u8 programid[8];
	u8 flags[8];
	u8 resourcelimitdescriptor[0x10][2];
	exheader_storageinfo storageinfo;
	u8 serviceaccesscontrol[0x20][8];
	u8 reserved[0x1f];
	u8 resourcelimitcategory;
} exheader_arm11systemlocalcaps;

typedef struct
{
	u8 descriptors[28][4];
	u8 reserved[0x10];
} exheader_arm11kernelcapabilities;

typedef struct
{
	u8 descriptors[15];
	u8 descversion;
} exheader_arm9accesscontrol;

typedef struct
{
	// systemcontrol info {
	//   coreinfo {
	exheader_codesetinfo codesetinfo;
	exheader_dependencylist deplist;
	//   }
	exheader_systeminfo systeminfo;
	// }
	// accesscontrolinfo {
	exheader_arm11systemlocalcaps arm11systemlocalcaps;
	exheader_arm11kernelcapabilities arm11kernelcaps;
	exheader_arm9accesscontrol arm9accesscontrol;
	// }
	struct {
		u8 signature[0x100];
		u8 ncchpubkeymodulus[0x100];
		exheader_arm11systemlocalcaps arm11systemlocalcaps;
		exheader_arm11kernelcapabilities arm11kernelcaps;
		exheader_arm9accesscontrol arm9accesscontrol;
	} accessdesc;
} exheader_header;

typedef struct
{
	int haveread;
	FILE* file;
	settings* usersettings;
	u8 partitionid[8];
	u8 programid[8];
	u8 counter[16];
	u8 key[16];
	u32 offset;
	u32 size;
	exheader_header header;
	ctr_aes_context aes;
	ctr_rsa_context rsa;
	int compressedflag;
	int encrypted;
	int validprogramid;
	int validpriority;
	int validaffinitymask;
	int validsignature;
} exheader_context;

void exheader_init(exheader_context* ctx);
void exheader_set_file(exheader_context* ctx, FILE* file);
void exheader_set_offset(exheader_context* ctx, u32 offset);
void exheader_set_size(exheader_context* ctx, u32 size);
void exheader_set_partitionid(exheader_context* ctx, u8 partitionid[8]);
void exheader_set_counter(exheader_context* ctx, u8 counter[16]);
void exheader_set_programid(exheader_context* ctx, u8 programid[8]);
void exheader_set_encrypted(exheader_context* ctx, u32 encrypted);
void exheader_set_key(exheader_context* ctx, u8 key[16]);
void exheader_set_usersettings(exheader_context* ctx, settings* usersettings);
int exheader_get_compressedflag(exheader_context* ctx);
void exheader_read(exheader_context* ctx, u32 actions);
int exheader_process(exheader_context* ctx, u32 actions);
void exheader_print(exheader_context* ctx);
void exheader_verify(exheader_context* ctx);
int exheader_programid_valid(exheader_context* ctx);
void exheader_determine_key(exheader_context* ctx, u32 actions);

#endif // _EXHEADER_H_
