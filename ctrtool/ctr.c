#include "ctr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


void ctr_set_iv( ctr_crypto_context* ctx,
				  unsigned char iv[16] )
{
	memcpy(ctx->iv, iv, 16);
}

void ctr_add_counter( ctr_crypto_context* ctx,
				      unsigned char carry )
{
	unsigned char sum;
	int i;

	for(i=15; i>=0; i--)
	{
		sum = ctx->ctr[i] + carry;

		if (sum < ctx->ctr[i])
			carry = 1;
		else
			carry = 0;

		ctx->ctr[i] = sum;
	}
}
				  
void ctr_set_counter( ctr_crypto_context* ctx,
				      unsigned char ctr[16] )
{
	memcpy(ctx->ctr, ctr, 16);
}


void ctr_init_counter( ctr_crypto_context* ctx,
				       unsigned char key[16],
				       unsigned char ctr[12] )
{
	aes_setkey_enc(&ctx->aes, key, 128);
	ctr_set_counter(ctx, ctr);
}

void ctr_crypt_counter_block( ctr_crypto_context* ctx, 
						      unsigned char input[16], 
						      unsigned char output[16] )
{
	int i;
	unsigned char stream[16];


	aes_crypt_ecb(&ctx->aes, AES_ENCRYPT, ctx->ctr, stream);


	if (input)
	{
		for(i=0; i<16; i++)
		{
			output[i] = stream[i] ^ input[i];
		}
	}
	else
	{
		for(i=0; i<16; i++)
			output[i] = stream[i];
	}

	ctr_add_counter(ctx, 1);
}


void ctr_crypt_counter( ctr_crypto_context* ctx, 
					    unsigned char* input, 
					    unsigned char* output,
					    unsigned int size )
{
	unsigned char stream[16];
	unsigned int i;

	while(size >= 16)
	{
		ctr_crypt_counter_block(ctx, input, output);

		if (input)
			input += 16;
		if (output)
			output += 16;

		size -= 16;
	}

	if (size)
	{
		memset(stream, 0, 16);
		ctr_crypt_counter_block(ctx, stream, stream);

		if (input)
		{
			for(i=0; i<size; i++)
				output[i] = input[i] ^ stream[i];
		}
		else
		{
			memcpy(output, stream, size);
		}
	}
}

void ctr_init_cbc_encrypt( ctr_crypto_context* ctx,
						   unsigned char key[16],
						   unsigned char iv[16] )
{
	aes_setkey_enc(&ctx->aes, key, 128);
	ctr_set_iv(ctx, iv);
}

void ctr_init_cbc_decrypt( ctr_crypto_context* ctx,
						   unsigned char key[16],
						   unsigned char iv[16] )
{
	aes_setkey_dec(&ctx->aes, key, 128);
	ctr_set_iv(ctx, iv);
}

void ctr_encrypt_cbc( ctr_crypto_context* ctx, 
					  unsigned char* input,
					  unsigned char* output,
					  unsigned int size )
{
	aes_crypt_cbc(&ctx->aes, AES_ENCRYPT, size, ctx->iv, input, output);
}

void ctr_decrypt_cbc( ctr_crypto_context* ctx, 
					  unsigned char* input,
					  unsigned char* output,
					  unsigned int size )
{
	aes_crypt_cbc(&ctx->aes, AES_DECRYPT, size, ctx->iv, input, output);
}
