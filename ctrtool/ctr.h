#ifndef _CTR_H_
#define _CTR_H_

#include "polarssl/aes.h"
#include "polarssl/rsa.h"
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

typedef struct
{
	u8 ctr[16];
	u8 iv[16];
	aes_context aes;
	rsa_context rsa;
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

int			ctr_rsa_key_init( ctr_crypto_context* ctx );


#ifdef __cplusplus
}
#endif

#endif // _CTR_H_
