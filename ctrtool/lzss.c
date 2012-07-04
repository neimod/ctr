#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "utils.h"
#include "lzss.h"

void lzss_init(lzss_context* ctx)
{
	memset(ctx, 0, sizeof(lzss_context));
}

void lzss_set_usersettings(lzss_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void lzss_set_offset(lzss_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void lzss_set_size(lzss_context* ctx, u32 size)
{
	ctx->size = size;
}

void lzss_set_file(lzss_context* ctx, FILE* file)
{
	ctx->file = file;
}


void lzss_process(lzss_context* ctx, u32 actions)
{
	unsigned int compressedsize;
	unsigned char* compressedbuffer = 0;
	unsigned int decompressedsize;
	unsigned char* decompressedbuffer = 0;
	FILE* fout = 0;


	fseek(ctx->file, ctx->offset, SEEK_SET);
	

	if (actions & ExtractFlag)
	{
		
		filepath* path = settings_get_lzss_path(ctx->usersettings);

		if (path == 0 || path->valid == 0)
			goto clean;

		fout = fopen(path->pathname, "wb");
		if (0 == fout)
		{
			fprintf(stdout, "Error opening out file %s\n", path->pathname);
			goto clean;
		}
		compressedsize = ctx->size;
		compressedbuffer = malloc(compressedsize);
		if (1 != fread(compressedbuffer, compressedsize, 1, ctx->file))
		{
			fprintf(stdout, "Error read input file\n");
			goto clean;
		}

		decompressedsize = lzss_get_decompressed_size(compressedbuffer, compressedsize);
		decompressedbuffer = malloc(decompressedsize);

		printf("Compressed: %d\n", compressedsize);
		printf("Decompressed: %d\n", decompressedsize);
		
		if (decompressedbuffer == 0)
		{
			fprintf(stdout, "Error allocating memory\n");
			goto clean;
		}

		if (0 == lzss_decompress(compressedbuffer, compressedsize, decompressedbuffer, decompressedsize))
			goto clean;

		printf("Saving decompressed lzss blob to %s...\n", path->pathname);
		if (decompressedsize != fwrite(decompressedbuffer, 1, decompressedsize, fout))
		{
			fprintf(stdout, "Error writing output file\n");
			goto clean;
		}
	}

clean:
	free(decompressedbuffer);
	free(compressedbuffer);
	if (fout)
		fclose(fout);
}


u32 lzss_get_decompressed_size(u8* compressed, u32 compressedsize)
{
	u8* footer = compressed + compressedsize - 8;

	//u32 buffertopandbottom = getle32(footer+0);
	u32 originalbottom = getle32(footer+4);

	return originalbottom + compressedsize;
}

int lzss_decompress(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize)
{
	u8* footer = compressed + compressedsize - 8;
	u32 buffertopandbottom = getle32(footer+0);
	//u32 originalbottom = getle32(footer+4);
	u32 i, j;
	u32 out = decompressedsize;
	u32 index = compressedsize - ((buffertopandbottom>>24)&0xFF);
	u32 segmentoffset;
	u32 segmentsize;
	u8 control;
	u32 stopindex = compressedsize - (buffertopandbottom&0xFFFFFF);

	memset(decompressed, 0, decompressedsize);
	memcpy(decompressed, compressed, compressedsize);

	
	while(index > stopindex)
	{
		control = compressed[--index];
		

		for(i=0; i<8; i++)
		{
			if (index <= stopindex)
				break;

			if (index <= 0)
				break;

			if (out <= 0)
				break;

			if (control & 0x80)
			{
				if (index < 2)
				{
					fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}

				index -= 2;

				segmentoffset = compressed[index] | (compressed[index+1]<<8);
				segmentsize = ((segmentoffset >> 12)&15)+3;
				segmentoffset &= 0x0FFF;
				segmentoffset += 2;

				
				if (out < segmentsize)
				{
					fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}

				for(j=0; j<segmentsize; j++)
				{
					u8 data;
					
					if (out+segmentoffset >= decompressedsize)
					{
						fprintf(stderr, "Error, compression out of bounds\n");
						goto clean;
					}

					data  = decompressed[out+segmentoffset];
					decompressed[--out] = data;
				}
			}
			else
			{
				if (out < 1)
				{
					fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}
				decompressed[--out] = compressed[--index];
			}

			control <<= 1;
		}
	}

	return 1;
clean:
	return 0;
}
