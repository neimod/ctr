#ifndef _CTR_H_
#define _CTR_H_

#include "aes.h"
#include "types.h"


#define MAGIC_NCCH 0x4843434E
#define MAGIC_NCSD 0x4453434E

typedef enum
{
	FILETYPE_UNKNOWN = 0,
	FILETYPE_CCI,
	FILETYPE_CXI,
	FILETYPE_CIA,
} ctr_filetypes;

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

typedef struct
{
	u8 ctr[16];
	u8 iv[16];
	aes_context aes;
}
ctr_crypto_context;


#ifdef __cplusplus
extern "C" {
#endif

void		ctr_set_iv( ctr_crypto_context* ctx, 
						 u8 iv[16] );

void		ctr_add_counter( ctr_crypto_context* ctx,
						 u8 carry );

void		ctr_set_counter( ctr_crypto_context* ctx, 
						 u8 ctr[16] );


void		ctr_init_counter( ctr_crypto_context* ctx, 
						  u8 key[16], 
						  u8 ctr[12] );

void		ctr_crypt_counter_block( ctr_crypto_context* ctx, 
								     u8 input[16], 
								     u8 output[16] );


void		ctr_crypt_counter( ctr_crypto_context* ctx, 
							   u8* input, 
							   u8* output,
							   u32 size );


void		ctr_init_cbc_encrypt( ctr_crypto_context* ctx,
							   u8 key[16],
							   u8 iv[16] );

void		ctr_init_cbc_decrypt( ctr_crypto_context* ctx,
							   u8 key[16],
							   u8 iv[16] );

void		ctr_encrypt_cbc( ctr_crypto_context* ctx, 
							  u8* input,
							  u8* output,
							  u32 size );

void		ctr_decrypt_cbc( ctr_crypto_context* ctx, 
							  u8* input,
							  u8* output,
							  u32 size );


#ifdef __cplusplus
}
#endif

#endif // _CTR_H_
