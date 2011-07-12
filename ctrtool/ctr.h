#ifndef _CTR_H_
#define _CTR_H_

#include "aes.h"


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
	unsigned char signature[0x100];
	unsigned char magic[4];
	unsigned char contentsize[4];
	unsigned char partitionid[8];
	unsigned char makercode[2];
	unsigned char version[2];
	unsigned char reserved0[4];
	unsigned char programid[8];
	unsigned char tempflag;
	unsigned char reserved1[0x2f];
    unsigned char productcode[0x10];
    unsigned char extendedheaderhash[0x20];
	unsigned char extendedheadersize[4];
    unsigned char reserved2[4];
    unsigned char flags[8];
    unsigned char plainregionoffset[4];
    unsigned char plainregionsize[4];
    unsigned char reserved3[8];
    unsigned char exefsoffset[4];
    unsigned char exefssize[4];
    unsigned char exefshashregionsize[4];
    unsigned char reserved4[4];
    unsigned char romfsoffset[4];
    unsigned char romfssize[4];
    unsigned char romfshashregionsize[4];
    unsigned char reserved5[4];
    unsigned char exefssuperblockhash[0x20];
    unsigned char romfssuperblockhash[0x20];
} ctr_ncchheader;


typedef struct
{
	unsigned char headersize[4];
	unsigned char type[2];
	unsigned char version[2];
	unsigned char certsize[4];
	unsigned char ticketsize[4];
	unsigned char tmdsize[4];
	unsigned char metasize[4];
	unsigned char contentsize[8];
	unsigned char contentindex[0x2000];
} ctr_ciaheader;

typedef struct
{
	unsigned char ctr[16];
	unsigned char iv[16];
    aes_context aes;
}
ctr_crypto_context;


#ifdef __cplusplus
extern "C" {
#endif

void		ctr_set_iv( ctr_crypto_context* ctx, 
						 unsigned char iv[16] );

void		ctr_add_counter( ctr_crypto_context* ctx,
						 unsigned char carry );

void		ctr_set_counter( ctr_crypto_context* ctx, 
						 unsigned char ctr[16] );


void		ctr_init_counter( ctr_crypto_context* ctx, 
						  unsigned char key[16], 
						  unsigned char ctr[12] );

void		ctr_crypt_counter_block( ctr_crypto_context* ctx, 
								     unsigned char input[16], 
								     unsigned char output[16] );


void		ctr_crypt_counter( ctr_crypto_context* ctx, 
							   unsigned char* input, 
							   unsigned char* output,
							   unsigned int size );


void		ctr_init_cbc_encrypt( ctr_crypto_context* ctx,
							   unsigned char key[16],
							   unsigned char iv[16] );

void		ctr_init_cbc_decrypt( ctr_crypto_context* ctx,
							   unsigned char key[16],
							   unsigned char iv[16] );

void		ctr_encrypt_cbc( ctr_crypto_context* ctx, 
							  unsigned char* input,
							  unsigned char* output,
							  unsigned int size );

void		ctr_decrypt_cbc( ctr_crypto_context* ctx, 
							  unsigned char* input,
							  unsigned char* output,
							  unsigned int size );


#ifdef __cplusplus
}
#endif

#endif // _CTR_H_
