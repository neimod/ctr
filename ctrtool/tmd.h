#ifndef _TMD_H_
#define _TMD_H_

#include "types.h"
#include "settings.h"

#define TMD_MAX_CONTENTS 64

typedef enum
{
	TMD_RSA_2048_SHA256 = 0x00010004,
	TMD_RSA_4096_SHA256 = 0x00010003,
	TMD_RSA_2048_SHA1   = 0x00010001,
	TMD_RSA_4096_SHA1   = 0x00010000
} ctr_tmdtype;


typedef struct
{
	unsigned char padding[60];
	unsigned char issuer[64];
	unsigned char version;
	unsigned char ca_crl_version;
	unsigned char signer_crl_version;
	unsigned char padding2;
	unsigned char systemversion[8];
	unsigned char titleid[8];
	unsigned char titletype[4];
	unsigned char groupid[2];
	unsigned char padding3[62];
	unsigned char accessrights[4];
	unsigned char titleversion[2];
	unsigned char contentcount[2];
	unsigned char bootcontent[2];
	unsigned char padding4[2];
	unsigned char hash[32];
	unsigned char contentinfo[36*64];
} ctr_tmd_body;

typedef struct
{
	unsigned char index[2];
	unsigned char commandcount[2];
	unsigned char unk[32];
} ctr_tmd_contentinfo;


typedef struct
{
	unsigned char id[4];
	unsigned char index[2];
	unsigned char type[2];
	unsigned char size[8];
	unsigned char hash[32];
} ctr_tmd_contentchunk;


typedef struct
{
	unsigned char signaturetype[4];
	unsigned char signature[256];
} ctr_tmd_header_2048;

typedef struct
{
	unsigned char signaturetype[4];
	unsigned char signature[512];
} ctr_tmd_header_4096;

typedef struct
{
	FILE* file;
	u32 offset;
	u32 size;
	u8* buffer;
	u8 content_hash_stat[64];
	settings* usersettings;
} tmd_context;



#ifdef __cplusplus
extern "C" {
#endif

void tmd_init(tmd_context* ctx);
void tmd_set_file(tmd_context* ctx, FILE* file);
void tmd_set_offset(tmd_context* ctx, u32 offset);
void tmd_set_size(tmd_context* ctx, u32 size);
void tmd_set_usersettings(tmd_context* ctx, settings* usersettings);
void tmd_print(tmd_context* ctx);
void tmd_process(tmd_context* ctx, u32 actions);
ctr_tmd_body *tmd_get_body(tmd_context *ctx);

#ifdef __cplusplus
}
#endif

#endif // _TMD_H_
