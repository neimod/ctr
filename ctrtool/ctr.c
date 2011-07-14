#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ctr.h"

void ctr_set_iv( ctr_crypto_context* ctx,
				  u8 iv[16] )
{
	memcpy(ctx->iv, iv, 16);
}

void ctr_add_counter( ctr_crypto_context* ctx,
				      u8 carry )
{
	u8 sum;
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
				      u8 ctr[16] )
{
	memcpy(ctx->ctr, ctr, 16);
}


void ctr_init_counter( ctr_crypto_context* ctx,
				       u8 key[16],
				       u8 ctr[12] )
{
	aes_setkey_enc(&ctx->aes, key, 128);
	ctr_set_counter(ctx, ctr);
}

void ctr_crypt_counter_block( ctr_crypto_context* ctx, 
						      u8 input[16], 
						      u8 output[16] )
{
	int i;
	u8 stream[16];


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
					    u8* input, 
					    u8* output,
					    u32 size )
{
	u8 stream[16];
	u32 i;

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
						   u8 key[16],
						   u8 iv[16] )
{
	aes_setkey_enc(&ctx->aes, key, 128);
	ctr_set_iv(ctx, iv);
}

void ctr_init_cbc_decrypt( ctr_crypto_context* ctx,
						   u8 key[16],
						   u8 iv[16] )
{
	aes_setkey_dec(&ctx->aes, key, 128);
	ctr_set_iv(ctx, iv);
}

void ctr_encrypt_cbc( ctr_crypto_context* ctx, 
					  u8* input,
					  u8* output,
					  u32 size )
{
	aes_crypt_cbc(&ctx->aes, AES_ENCRYPT, size, ctx->iv, input, output);
}

void ctr_decrypt_cbc( ctr_crypto_context* ctx, 
					  u8* input,
					  u8* output,
					  u32 size )
{
	aes_crypt_cbc(&ctx->aes, AES_DECRYPT, size, ctx->iv, input, output);
}



/*
 * Generate DP, DQ, QP based on private key
 */ 
int ctr_rsa_key_init(ctr_crypto_context* ctx )
{
    int ret;
    mpi P1, Q1;

    mpi_init( &P1, &Q1, NULL );

    MPI_CHK( mpi_sub_int( &P1, &ctx->rsa.P, 1 ) );
    MPI_CHK( mpi_sub_int( &Q1, &ctx->rsa.Q, 1 ) );

	/*
     * DP = D mod (P - 1)
     * DQ = D mod (Q - 1)
     * QP = Q^-1 mod P
     */
    MPI_CHK( mpi_mod_mpi( &ctx->rsa.DP, &ctx->rsa.D, &P1 ) );
    MPI_CHK( mpi_mod_mpi( &ctx->rsa.DQ, &ctx->rsa.D, &Q1 ) );
    MPI_CHK( mpi_inv_mod( &ctx->rsa.QP, &ctx->rsa.Q, &ctx->rsa.P ) );

cleanup:

    mpi_free(&Q1, &P1, NULL );

    if( ret != 0 )
    {
        rsa_free( &ctx->rsa );
        return( POLARSSL_ERR_RSA_KEY_GEN_FAILED | ret );
    }

    return( 0 );   
}
