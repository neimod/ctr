#ifndef _CTR_H_
#define _CTR_H_

#include "polarssl/aes.h"
#include "polarssl/rsa.h"
#include "polarssl/sha2.h"
#include "types.h"
#include "keyset.h"

#define MAGIC_NCCH 0x4843434E
#define MAGIC_NCSD 0x4453434E
#define MAGIC_FIRM 0x4D524946
#define MAGIC_CWAV 0x56415743
#define MAGIC_IVFC 0x43465649

#define SIZE_128MB (128 * 1024 * 1024)

typedef enum
{
	FILETYPE_UNKNOWN = 0,
	FILETYPE_CCI,
	FILETYPE_CXI,
	FILETYPE_CIA,
	FILETYPE_EXHEADER,
	FILETYPE_TMD,
	FILETYPE_LZSS,
	FILETYPE_FIRM,
	FILETYPE_CWAV,
	FILETYPE_ROMFS
} ctr_filetypes;

typedef struct
{
	u8 ctr[16];
	u8 iv[16];
	aes_context aes;
} ctr_aes_context;

typedef struct
{
	rsa_context rsa;
} ctr_rsa_context;

typedef struct
{
	sha2_context sha;
} ctr_sha256_context;


#ifdef __cplusplus
extern "C" {
#endif

void		ctr_set_iv( ctr_aes_context* ctx, 
						 u8 iv[16] );

void		ctr_add_counter( ctr_aes_context* ctx,
						     u32 carry );

void		ctr_set_counter( ctr_aes_context* ctx, 
						 u8 ctr[16] );


void		ctr_init_counter( ctr_aes_context* ctx, 
						  u8 key[16], 
						  u8 ctr[16] );


void		ctr_crypt_counter_block( ctr_aes_context* ctx, 
								     u8 input[16], 
								     u8 output[16] );


void		ctr_crypt_counter( ctr_aes_context* ctx, 
							   u8* input, 
							   u8* output,
							   u32 size );


void		ctr_init_cbc_encrypt( ctr_aes_context* ctx,
							   u8 key[16],
							   u8 iv[16] );

void		ctr_init_cbc_decrypt( ctr_aes_context* ctx,
							   u8 key[16],
							   u8 iv[16] );

void		ctr_encrypt_cbc( ctr_aes_context* ctx, 
							  u8* input,
							  u8* output,
							  u32 size );

void		ctr_decrypt_cbc( ctr_aes_context* ctx, 
							  u8* input,
							  u8* output,
							  u32 size );

void		ctr_rsa_init_key_pubmodulus( rsakey2048* key, 
											u8 modulus[0x100] );

void		ctr_rsa_init_key_pub( rsakey2048* key, 
									u8 modulus[0x100], 
									u8 exponent[3] );

int			ctr_rsa_init( ctr_rsa_context* ctx, 
						  rsakey2048* key );


void		ctr_rsa_free( ctr_rsa_context* ctx );

int			ctr_rsa_verify_hash( const u8 signature[0x100], 
								 const u8 hash[0x20], 
								 rsakey2048* key);

int			ctr_rsa_sign_hash( const u8 hash[0x20], 
							   u8 signature[0x100], 
							   rsakey2048* key );

int			ctr_rsa_public( const u8 signature[0x100], 
						    u8 output[0x100], 
							rsakey2048* key );

void		ctr_sha_256( const u8* data, 
						 u32 size, 
						 u8 hash[0x20] );

int			ctr_sha_256_verify( const u8* data, 
								u32 size, 
								const u8 checkhash[0x20] );


void		ctr_sha_256_init( ctr_sha256_context* ctx );

void		ctr_sha_256_update( ctr_sha256_context* ctx, 
							    const u8* data,
								u32 size );


void		ctr_sha_256_finish( ctr_sha256_context* ctx, 
							    u8 hash[0x20] );

#ifdef __cplusplus
}
#endif

#endif // _CTR_H_
