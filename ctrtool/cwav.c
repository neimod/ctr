#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "cwav.h"
#include "utils.h"
#include "stream.h"

#define BUFFERSIZE (4*1024)
#define SAMPLECOUNT 1024

static const int ima_adpcm_step_table[89] = { 
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 
  19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
  876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
};

static const int ima_adpcm_index_table[16] = {
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
}; 

void cwav_init(cwav_context* ctx)
{
	memset(ctx, 0, sizeof(cwav_context));
}

void cwav_set_file(cwav_context* ctx, FILE* file)
{
	ctx->file = file;
}

void cwav_set_offset(cwav_context* ctx, u32 offset)
{
	ctx->offset = offset;
}

void cwav_set_size(cwav_context* ctx, u32 size)
{
	ctx->size = size;
}

void cwav_set_usersettings(cwav_context* ctx, settings* usersettings)
{
	ctx->usersettings = usersettings;
}

void cwav_process(cwav_context* ctx, u32 actions)
{
	u32 i;
	u32 infoheaderoffset;

	fseek(ctx->file, ctx->offset, SEEK_SET);
	fread(&ctx->header, 1, sizeof(cwav_header), ctx->file);

	infoheaderoffset = getle32(ctx->header.infoblockref.offset);

	fseek(ctx->file, ctx->offset + infoheaderoffset, SEEK_SET);
	fread(&ctx->infoheader, 1, sizeof(cwav_infoheader), ctx->file);

	ctx->channelcount = getle32(ctx->infoheader.channelcount);
	if (ctx->channelcount)
	{
		ctx->channel = malloc(ctx->channelcount * sizeof(cwav_channel));

		for(i=0; i<ctx->channelcount; i++)
		{
			fread(&ctx->channel[i].inforef, sizeof(cwav_reference), 1, ctx->file);
		}

		for(i=0; i<ctx->channelcount; i++)
		{
			u32 channeloffset = infoheaderoffset + 0x1C + getle32(ctx->channel[i].inforef.offset);

			fseek(ctx->file, ctx->offset + channeloffset, SEEK_SET);
			fread(&ctx->channel[i].info, sizeof(cwav_channelinfo), 1, ctx->file);

			if (ctx->infoheader.encoding == CWAV_ENCODING_DSPADPCM)
			{
				if (getle16(ctx->channel[i].info.codecref.idtype) == 0x300)
				{
					u32 codecoffset = channeloffset + getle32(ctx->channel[i].info.codecref.offset);

					fseek(ctx->file, ctx->offset + codecoffset, SEEK_SET);
					fread(&ctx->channel[i].infodspadpcm, sizeof(cwav_dspadpcminfo), 1, ctx->file);
				}
			}
			else if (ctx->infoheader.encoding == CWAV_ENCODING_IMAADPCM)
			{
				if (getle16(ctx->channel[i].info.codecref.idtype) == 0x301)
				{
					u32 codecoffset = channeloffset + getle32(ctx->channel[i].info.codecref.offset);

					fseek(ctx->file, ctx->offset + codecoffset, SEEK_SET);
					fread(&ctx->channel[i].infoimaadpcm, sizeof(cwav_imaadpcminfo), 1, ctx->file);
				}
			}
		}
	}
	

	if (actions & InfoFlag)
	{
		cwav_print(ctx);
	}

	if (actions & ExtractFlag)
	{
		filepath* path = settings_get_wav_path(ctx->usersettings);

		if (path && path->valid)
			cwav_save_to_wav(ctx, path->pathname);
	}


	free(ctx->channel);
}


void cwav_write_wav_header(cwav_context* ctx, stream_out_context* outstreamctx, u32 size)
{
	wav_pcm_header header;
	u32 samplerate = getle32(ctx->infoheader.samplerate);
	u32 channelcount = ctx->channelcount;


	putle32(header.chunkid, 0x46464952);
	putle32(header.chunksize, 36 + size);
	putle32(header.format, 0x45564157);
	putle32(header.subchunk1id, 0x20746d66);
	putle32(header.subchunk1size, 16);
	putle16(header.audioformat, 1);
	putle16(header.numchannels, channelcount);
	putle32(header.samplerate, samplerate);
	putle32(header.byterate, samplerate * channelcount * 2);
	putle16(header.blockalign, channelcount * 2);
	putle16(header.bitspersample, 16);

	putle32(header.subchunk2id, 0x61746164);
	putle32(header.subchunk2size, size);

	stream_out_buffer(outstreamctx, &header, sizeof(wav_pcm_header));
}

int cwav_dspadpcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx)
{
	u32 s, c, i;
	int result = 0;
	cwav_dspadpcmstate state;
	u32 loopcount = settings_get_cwav_loopcount(ctx->usersettings);

	cwav_dspadpcm_init(&state);

	if (0 == cwav_dspadpcm_allocate(&state, ctx))
		goto clean;

	for(i=0; i<1+loopcount; i++)
	{
		int isloop = (i != 0);

		if (0 == cwav_dspadpcm_setup(&state, ctx, isloop))
			goto clean;

		while(1)
		{
			if (0 == cwav_dspadpcm_decode(&state, ctx))
				goto clean;

			if (state.samplecountavailable == 0)
				break;

			for(s=0; s<state.samplecountavailable; s++)
			{
				for(c=0; c<ctx->channelcount; c++)
				{
					s16 sampledata = state.channelstate[c].samplebuffer[s];

					if (!stream_out_byte(outstreamctx, 0xFF & sampledata) || !stream_out_byte(outstreamctx, 0xFF & (sampledata>>8)))
					{
						fprintf(stderr, "Error writing output stream\n");
						goto clean;
					}
				}
			}	
		}
	}

	result = 1;

clean:
	cwav_dspadpcm_destroy(&state);

	return result;
}


int cwav_imaadpcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx)
{
	u32 s, c, i;
	int result = 0;
	cwav_imaadpcmstate state;
	u32 loopcount = settings_get_cwav_loopcount(ctx->usersettings);


	cwav_imaadpcm_init(&state);
	if (0 == cwav_imaadpcm_allocate(&state, ctx))
		goto clean;

	for(i=0; i<1+loopcount; i++)
	{
		int isloop = (i != 0);

		if (0 == cwav_imaadpcm_setup(&state, ctx, isloop))
			goto clean;

		while(1)
		{
			if (0 == cwav_imaadpcm_decode(&state, ctx))
				goto clean;

			if (state.samplecountavailable == 0)
				break;

			for(s=0; s<state.samplecountavailable; s++)
			{
				for(c=0; c<ctx->channelcount; c++)
				{
					s16 sampledata = state.channelstate[c].samplebuffer[s];

					if (!stream_out_byte(outstreamctx, 0xFF & sampledata) || !stream_out_byte(outstreamctx, 0xFF & (sampledata>>8)))
					{
						fprintf(stderr, "Error writing output stream\n");
						goto clean;
					}
				}
			}	
		}
	}

	result = 1;

clean:
	cwav_imaadpcm_destroy(&state);

	return result;
}


int cwav_pcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx)
{
	u32 s, c, i;
	int result = 0;
	cwav_pcmstate state;
	u32 loopcount = settings_get_cwav_loopcount(ctx->usersettings);


	cwav_pcm_init(&state);


	if (0 == cwav_pcm_allocate(&state, ctx))
		goto clean;

	for(i=0; i<1+loopcount; i++)
	{
		int isloop = (i != 0);

		if (0 == cwav_pcm_setup(&state, ctx, isloop))
			goto clean;

		while(1)
		{
			if (0 == cwav_pcm_decode(&state, ctx))
				goto clean;

			if (state.samplecountavailable == 0)
				break;

			for(s=0; s<state.samplecountavailable; s++)
			{
				for(c=0; c<ctx->channelcount; c++)
				{
					s16 sampledata = state.channelstate[c].samplebuffer[s];

					if (!stream_out_byte(outstreamctx, 0xFF & sampledata) || !stream_out_byte(outstreamctx, 0xFF & (sampledata>>8)))
					{
						fprintf(stderr, "Error writing output stream\n");
						goto clean;
					}
				}
			}	
		}
	}

	result = 1;

clean:
	cwav_pcm_destroy(&state);

	return result;
}

int cwav_save_to_wav(cwav_context* ctx, const char* filepath)
{
	u32 startposition = 0;
	u32 endposition = 0;
	int result = 0;
	FILE* outfile = 0;	
	stream_out_context outstreamctx;


	stream_out_init(&outstreamctx);

	if (ctx->channelcount == 0)
		goto clean;

	fprintf(stdout, "Saving sound data to %s...\n", filepath);
	outfile = fopen(filepath, "wb");
	if (!outfile)
	{
		fprintf(stderr, "Error could not open file %s for writing.\n", filepath);
		goto clean;
	}

	stream_out_allocate(&outstreamctx, BUFFERSIZE, outfile);
	stream_out_skip(&outstreamctx, sizeof(wav_pcm_header));
	stream_out_position(&outstreamctx, &startposition);

	if (ctx->infoheader.encoding == CWAV_ENCODING_DSPADPCM)
		result = cwav_dspadpcm_decode_to_wav(ctx, &outstreamctx);
	else if (ctx->infoheader.encoding == CWAV_ENCODING_IMAADPCM)
		result = cwav_imaadpcm_decode_to_wav(ctx, &outstreamctx);
	else if (ctx->infoheader.encoding == CWAV_ENCODING_PCM16)
		result = cwav_pcm_decode_to_wav(ctx, &outstreamctx);
	else if (ctx->infoheader.encoding == CWAV_ENCODING_PCM8)
		result = cwav_pcm_decode_to_wav(ctx, &outstreamctx);

	if (!result)
		goto clean;

	stream_out_position(&outstreamctx, &endposition);

	stream_out_seek(&outstreamctx, 0);
	cwav_write_wav_header(ctx, &outstreamctx, endposition-startposition);
	stream_out_flush(&outstreamctx);
	result = 1;

clean:
	stream_out_destroy(&outstreamctx);

	if (outfile)
		fclose(outfile);

	return result;
}

void cwav_dspadpcm_init(cwav_dspadpcmstate* state)
{
	memset(state, 0, sizeof(cwav_dspadpcmstate));
}

int cwav_dspadpcm_allocate(cwav_dspadpcmstate* state, cwav_context* ctx)
{
	u32 channelcount = ctx->channelcount;


	state->samplebuffer = malloc(sizeof(s16) * SAMPLECOUNT * channelcount);
	state->channelstate = malloc(sizeof(cwav_dspadpcmchannelstate) * channelcount);
	state->samplecountcapacity = SAMPLECOUNT;
	state->samplecountavailable = 0;
	state->samplecountremaining = 0;

	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
	{
		fprintf(stderr, "Error allocating memory\n");
		return 0;
	}

	return 1;
}

int cwav_dspadpcm_setup(cwav_dspadpcmstate* state, cwav_context* ctx, int isloop)
{
	u32 channelcount = ctx->channelcount;
	u32 i;
	u32 startoffset = 0;


	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
		return 0;

	state->samplecountavailable = 0;

	if (isloop)
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend) - getle32(ctx->infoheader.loopstart);

		startoffset = getle32(ctx->infoheader.loopstart) * 8 / 14;
	}
	else
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend);
		startoffset = 0;
	}

	

	for(i=0; i<channelcount; i++)
	{
		cwav_channel* adpcmchannel = &ctx->channel[i];
		cwav_dspadpcminfo* adpcminfo = &adpcmchannel->infodspadpcm;

		if (getle16(adpcmchannel->info.codecref.idtype) != 0x300)
		{
			fprintf(stderr, "Error, not DSP-ADPCM format.\n");
			return 0;
		}

		state->channelstate[i].samplebuffer = state->samplebuffer + SAMPLECOUNT * i;
		state->channelstate[i].sampleoffset = ctx->offset + getle32(adpcmchannel->info.sampleref.offset) + getle32(ctx->header.datablockref.offset) + 8 + startoffset;
		if (isloop)
		{
			state->channelstate[i].yn1 = getle16(adpcminfo->loopyn1);
			state->channelstate[i].yn2 = getle16(adpcminfo->loopyn2);
		}
		else
		{
			state->channelstate[i].yn1 = getle16(adpcminfo->yn1);
			state->channelstate[i].yn2 = getle16(adpcminfo->yn2);
		}
		stream_in_allocate(&state->channelstate[i].instreamctx, BUFFERSIZE, ctx->file);
		stream_in_seek(&state->channelstate[i].instreamctx, state->channelstate[i].sampleoffset);
	}

	return 1;
}

// decode dsp-adpcm to pcm signed 16-bit
int cwav_dspadpcm_decode(cwav_dspadpcmstate* state, cwav_context* ctx)
{
	u32 i, c;
	u32 maxsamplecount;
	u32 channelcount = ctx->channelcount;
	
	if (ctx->channel == 0 || state->samplebuffer == 0 || state->channelstate == 0)
		return 0;


	state->samplecountavailable = 0;
	if (state->samplecountremaining <= 0)
	{
		return 1;
	}

	while(state->samplecountremaining > 0)
	{
		u32 samplecountavailable = state->samplecountcapacity - state->samplecountavailable;

		if (state->samplecountremaining < 14)
			maxsamplecount = state->samplecountremaining;
		else
			maxsamplecount = 14;

		if (samplecountavailable < maxsamplecount)
			break;

		for(c=0; c<channelcount; c++)
		{	
			cwav_dspadpcmchannelstate* channelstate = &state->channelstate[c];

			s16* samplebuffer = channelstate->samplebuffer + state->samplecountavailable;
			stream_in_context* instreamctx = &channelstate->instreamctx;
			s16 yn1 = channelstate->yn1;
			s16 yn2 = channelstate->yn2;
			cwav_channel* adpcmchannel = &ctx->channel[c];
			cwav_dspadpcminfo* adpcminfo = &adpcmchannel->infodspadpcm;
			
			u8 data;
			u8 lonibble;
			u8 hinibble;
			s16 coef1;
			s16 coef2;
			u32 shift;
			s16 table[14];

			stream_in_reseek(instreamctx);


			if (0 == stream_in_byte(instreamctx, &data))
			{
				fprintf(stderr, "Error reading input stream\n");
				return 1;
			}

			lonibble = data & 0xF;
			hinibble = data>>4;

			coef1 = getle16(adpcminfo->coef[hinibble*2+0]);
			coef2 = getle16(adpcminfo->coef[hinibble*2+1]);
			shift = 17 - lonibble;

			for(i=0; i<7; i++)
			{
				stream_in_byte(instreamctx, &data);
				table[i*2+0] = data>>4;
				table[i*2+1] = data & 0xF;
			}


			for(i=0; i<maxsamplecount; i++)
			{
				s32 x = table[i] << 28;
				s32 xshifted = x >> shift;

				s32 prediction = (yn1 * coef1 + yn2 * coef2 + xshifted + 0x400)>>11;
				
				if (prediction < -0x8000)
					prediction = -0x8000;
				if (prediction > 0x7FFF)
					prediction = 0x7FFF;

				yn2 = yn1;
				yn1 = prediction;

				samplebuffer[i] = prediction;
			}

			channelstate->yn1 = yn1;
			channelstate->yn2 = yn2;
		}

		state->samplecountremaining -= maxsamplecount;
		state->samplecountavailable += maxsamplecount;
	}

	return 1;
}

void cwav_dspadpcm_destroy(cwav_dspadpcmstate* state)
{
	free(state->channelstate);
	free(state->samplebuffer);

	state->channelstate = 0;
	state->samplebuffer = 0;
}

void cwav_imaadpcm_init(cwav_imaadpcmstate* state)
{
	memset(state, 0, sizeof(cwav_imaadpcmstate));
}


int cwav_imaadpcm_allocate(cwav_imaadpcmstate* state, cwav_context* ctx)
{
	u32 channelcount = ctx->channelcount;


	state->samplebuffer = malloc(sizeof(s16) * SAMPLECOUNT * channelcount);
	state->channelstate = malloc(sizeof(cwav_imaadpcmchannelstate) * channelcount);
	state->samplecountcapacity = SAMPLECOUNT;
	state->samplecountavailable = 0;
	state->samplecountremaining = 0;

	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
	{
		fprintf(stderr, "Error allocating memory\n");
		return 0;
	}

	return 1;
}

int cwav_imaadpcm_setup(cwav_imaadpcmstate* state, cwav_context* ctx, int isloop)
{
	u32 channelcount = ctx->channelcount;
	u32 i;
	u32 startoffset = 0;


	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
	{
		fprintf(stderr, "Error allocating memory\n");
		return 0;
	}

	state->samplecountavailable = 0;
	if (isloop)
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend) - getle32(ctx->infoheader.loopstart);

		startoffset = getle32(ctx->infoheader.loopstart) / 2;
	}
	else
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend);
		startoffset = 0;
	}

	for(i=0; i<channelcount; i++)
	{
		cwav_channel* adpcmchannel = &ctx->channel[i];
		cwav_imaadpcminfo* adpcminfo = &adpcmchannel->infoimaadpcm;

		if (getle16(adpcmchannel->info.codecref.idtype) != 0x301)
		{
			fprintf(stderr, "Error, not IMA-ADPCM format.\n");
			return 0;
		}

		state->channelstate[i].samplebuffer = state->samplebuffer + SAMPLECOUNT * i;
		state->channelstate[i].sampleoffset = ctx->offset + getle32(adpcmchannel->info.sampleref.offset) + getle32(ctx->header.datablockref.offset) + 8 + startoffset;
		if (isloop)
		{
			state->channelstate[i].data = getle16(adpcminfo->loopdata);
			state->channelstate[i].tableindex = adpcminfo->looptableindex;
		}
		else
		{
			state->channelstate[i].data = getle16(adpcminfo->data);
			state->channelstate[i].tableindex = adpcminfo->tableindex;
		}
		stream_in_allocate(&state->channelstate[i].instreamctx, BUFFERSIZE, ctx->file);
		stream_in_seek(&state->channelstate[i].instreamctx, state->channelstate[i].sampleoffset);
	}

	return 1;
}

u8 cwav_imaadpcm_clamp_tableindex(u8 tableindex, int inc)
{
	int unclamped = tableindex + inc;

	if (unclamped < 0)
		unclamped = 0;
	if (unclamped > 88)
		unclamped = 88;

	return unclamped;
}

// decode ima-adpcm to pcm signed 16-bit
int cwav_imaadpcm_decode(cwav_imaadpcmstate* state, cwav_context* ctx)
{
	u32 i, c;
	u32 maxsamplecount;
	u32 channelcount = ctx->channelcount;
	
	if (ctx->channel == 0 || state->samplebuffer == 0 || state->channelstate == 0)
		return 0;


	state->samplecountavailable = 0;
	if (state->samplecountremaining <= 0)
	{
		return 1;
	}

	while(state->samplecountremaining > 0)
	{
		u32 samplecountavailable = state->samplecountcapacity - state->samplecountavailable;

		if (state->samplecountremaining < 2)
			maxsamplecount = state->samplecountremaining;
		else
			maxsamplecount = 2;

		if (samplecountavailable < maxsamplecount)
			break;

		for(c=0; c<channelcount; c++)
		{	
			cwav_imaadpcmchannelstate* channelstate = &state->channelstate[c];

			s16* samplebuffer = channelstate->samplebuffer + state->samplecountavailable;
			stream_in_context* instreamctx = &channelstate->instreamctx;
			s16 prediction = channelstate->data;
			u8 tableindex = channelstate->tableindex;
			cwav_channel* adpcmchannel = &ctx->channel[c];
			cwav_imaadpcminfo* adpcminfo = &adpcmchannel->infoimaadpcm;			
			u8 data;


			stream_in_reseek(instreamctx);


			if (0 == stream_in_byte(instreamctx, &data))
			{
				fprintf(stderr, "Error reading input stream\n");
				return 1;
			}


			for(i=0; i<maxsamplecount; i++)
			{
				s32 nibble = data & 0xF;
				s32 step = 0;
				s32 diff = 0;


				tableindex = cwav_imaadpcm_clamp_tableindex(tableindex, 0);
				step = ima_adpcm_step_table[tableindex];
				tableindex = cwav_imaadpcm_clamp_tableindex(tableindex, ima_adpcm_index_table[nibble]); 
				
				diff = step/8;
				if (nibble & 1) diff += step/4;
				if (nibble & 2) diff += step/2;
				if (nibble & 4) diff += step;

				if (nibble & 8) 
					prediction -= diff;
				else 
					prediction += diff;

				if (prediction < -0x8000)
					prediction = -0x8000;
				if (prediction > 0x7FFF)
					prediction = 0x7FFF;

				samplebuffer[i] = prediction;
				data >>= 4;
			}

			channelstate->data = prediction;
			channelstate->tableindex = tableindex;
		}

		state->samplecountremaining -= maxsamplecount;
		state->samplecountavailable += maxsamplecount;
	}

	return 1;
}

void cwav_imaadpcm_destroy(cwav_imaadpcmstate* state)
{
	free(state->channelstate);
	free(state->samplebuffer);

	state->channelstate = 0;
	state->samplebuffer = 0;
}


void cwav_pcm_init(cwav_pcmstate* state)
{
	memset(state, 0, sizeof(cwav_pcmstate));
}

int cwav_pcm_allocate(cwav_pcmstate* state, cwav_context* ctx)
{
	u32 channelcount = ctx->channelcount;


	state->samplebuffer = malloc(sizeof(s16) * SAMPLECOUNT * channelcount);
	state->channelstate = malloc(sizeof(cwav_pcmchannelstate) * channelcount);
	state->samplecountcapacity = SAMPLECOUNT;
	state->samplecountavailable = 0;
	state->samplecountremaining = 0;

	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
	{
		fprintf(stderr, "Error allocating memory\n");
		return 0;
	}

	return 1;
}

int cwav_pcm_setup(cwav_pcmstate* state, cwav_context* ctx, int isloop)
{
	u32 channelcount = ctx->channelcount;
	u32 i;
	u32 startoffset;


	if (ctx->channel == 0)
		return 0;

	if (state->samplebuffer == 0 || state->channelstate == 0)
	{
		fprintf(stderr, "Error allocating memory\n");
		return 0;
	}

	state->samplecountavailable = 0;
	state->samplecountcapacity = SAMPLECOUNT;
	if (isloop)
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend) - getle32(ctx->infoheader.loopstart);

		if (ctx->infoheader.encoding == CWAV_ENCODING_PCM8)
			startoffset = getle32(ctx->infoheader.loopstart);
		else if (ctx->infoheader.encoding == CWAV_ENCODING_PCM16)
			startoffset = getle32(ctx->infoheader.loopstart) * 2;
	}
	else
	{
		state->samplecountremaining = getle32(ctx->infoheader.loopend);
		startoffset = 0;
	}

	for(i=0; i<channelcount; i++)
	{
		cwav_channel* pcmchannel = &ctx->channel[i];

		state->channelstate[i].samplebuffer = state->samplebuffer + SAMPLECOUNT * i;
		state->channelstate[i].sampleoffset = ctx->offset + getle32(pcmchannel->info.sampleref.offset) + getle32(ctx->header.datablockref.offset) + 8 + startoffset;
		stream_in_allocate(&state->channelstate[i].instreamctx, BUFFERSIZE, ctx->file);
		stream_in_seek(&state->channelstate[i].instreamctx, state->channelstate[i].sampleoffset);
	}

	return 1;
}

// decode pcm to pcm signed 16-bit
int cwav_pcm_decode(cwav_pcmstate* state, cwav_context* ctx)
{
	u32 i, c;
	u32 maxsamplecount;
	u32 channelcount = ctx->channelcount;
	
	if (ctx->channel == 0 || state->samplebuffer == 0 || state->channelstate == 0)
		return 0;


	state->samplecountavailable = 0;
	if (state->samplecountremaining <= 0)
	{
		return 1;
	}

	while(state->samplecountremaining > 0)
	{
		u32 samplecountavailable = state->samplecountcapacity - state->samplecountavailable;

		if (state->samplecountremaining < 1)
			maxsamplecount = state->samplecountremaining;
		else
			maxsamplecount = 1;

		if (samplecountavailable < maxsamplecount)
			break;

		for(c=0; c<channelcount; c++)
		{	
			cwav_pcmchannelstate* channelstate = &state->channelstate[c];

			s16* samplebuffer = channelstate->samplebuffer + state->samplecountavailable;
			stream_in_context* instreamctx = &channelstate->instreamctx;
			cwav_channel* pcmchannel = &ctx->channel[c];
			

			stream_in_reseek(instreamctx);

			for(i=0; i<maxsamplecount; i++)
			{
				u8 datalo, datahi;

				if (ctx->infoheader.encoding == CWAV_ENCODING_PCM16)
				{
					if (0 == stream_in_byte(instreamctx, &datalo) || 0 == stream_in_byte(instreamctx, &datahi))
					{
						fprintf(stderr, "Error reading input stream\n");
						return 1;
					}
					samplebuffer[i] = (datahi << 8) | datalo;
				}
				else if (ctx->infoheader.encoding == CWAV_ENCODING_PCM8)
				{
					if (0 == stream_in_byte(instreamctx, &datahi))
					{
						fprintf(stderr, "Error reading input stream\n");
						return 1;
					}
					samplebuffer[i] = (datahi << 8);
				}
			}
		}

		state->samplecountremaining -= maxsamplecount;
		state->samplecountavailable += maxsamplecount;
	}

	return 1;
}

void cwav_pcm_destroy(cwav_pcmstate* state)
{
	free(state->channelstate);
	free(state->samplebuffer);

	state->channelstate = 0;
	state->samplebuffer = 0;
}

const char* cwav_encoding_string(u8 encoding)
{
	switch(encoding)
	{
		case CWAV_ENCODING_DSPADPCM: return "DSP-ADPCM";
		case CWAV_ENCODING_IMAADPCM: return "IMA-ADPCM";
		case CWAV_ENCODING_PCM8: return "PCM8";
		case CWAV_ENCODING_PCM16: return "PCM16";
		default: return "UNKNOWN";
	}
}

void cwav_print(cwav_context* ctx)
{
	cwav_header* header = &ctx->header;
	cwav_infoheader* infoheader = &ctx->infoheader;
	u32 i;
	u32 infoheaderoffset = ctx->offset + getle32(ctx->header.infoblockref.offset);
	u32 channelcount = getle32(infoheader->channelcount);

	fprintf(stdout, "Header:                 %c%c%c%c\n", header->magic[0], header->magic[1], header->magic[2], header->magic[3]);
	fprintf(stdout, "Byte order mark:        0x%04X\n", getle16(header->byteordermark));
	fprintf(stdout, "Header size:            0x%04X\n", getle16(header->headersize));
	fprintf(stdout, "Version:                0x%08X\n", getle32(header->version));
	fprintf(stdout, "Total size:             0x%08X\n", getle32(header->totalsize));
	fprintf(stdout, "Data blocks:            0x%04X\n", getle16(header->datablocks));
	fprintf(stdout, "Info block idtype:      0x%04X\n", getle16(header->infoblockref.idtype));
	fprintf(stdout, "Info block offset:      0x%08X\n", getle32(header->infoblockref.offset));
	fprintf(stdout, "Info block size:        0x%08X\n", getle32(header->infoblockref.size));
	fprintf(stdout, "Data block idtype:      0x%04X\n", getle16(header->datablockref.idtype));
	fprintf(stdout, "Data block offset:      0x%08X\n", getle32(header->datablockref.offset));
	fprintf(stdout, "Data block size:        0x%08X\n", getle32(header->datablockref.size));
	fprintf(stdout, "\n");
	fprintf(stdout, "Header:                 %c%c%c%c\n", infoheader->magic[0], infoheader->magic[1], infoheader->magic[2], infoheader->magic[3]);
	fprintf(stdout, "Size:                   0x%08X\n", getle32(infoheader->size));
	fprintf(stdout, "Encoding:               0x%02X (%s)\n", infoheader->encoding, cwav_encoding_string(infoheader->encoding));
	fprintf(stdout, "Looped:                 0x%02X\n", infoheader->looped);
	fprintf(stdout, "Samplerate:             %d\n", getle32(infoheader->samplerate));
	fprintf(stdout, "Loop start:             0x%08X\n", getle32(infoheader->loopstart));
	fprintf(stdout, "Loop end:               0x%08X\n", getle32(infoheader->loopend));
	fprintf(stdout, "Channels:               %d\n", channelcount);
	if (ctx->channel != 0) 
	{
		for(i=0; i<channelcount; i++)
		{
			u32 channeloffset = infoheaderoffset + 0x1C + getle32(ctx->channel[i].inforef.offset);
			u32 codecoffset = channeloffset + getle32(ctx->channel[i].info.codecref.offset);
			u32 sampleoffset = ctx->offset + getle32(ctx->channel[i].info.sampleref.offset) + getle32(ctx->header.datablockref.offset) + 8;

			fprintf(stdout, "Channel %d:\n", i);
			fprintf(stdout, " > Channel ref idtype:  0x%04X\n", getle16(ctx->channel[i].inforef.idtype));
			fprintf(stdout, " > Channel ref offset:  0x%08X\n", channeloffset);
			fprintf(stdout, " > Sample ref idtype:   0x%04X\n", getle16(ctx->channel[i].info.sampleref.idtype));
			fprintf(stdout, " > Sample ref offset:   0x%08X\n", sampleoffset);
			fprintf(stdout, " > Codec ref idtype:    0x%04X\n", getle16(ctx->channel[i].info.codecref.idtype));
			fprintf(stdout, " > Codec ref offset:    0x%08X\n", codecoffset);


#ifdef CWAV_CODEC_PRINT
			if (ctx->infoheader.encoding == CWAV_ENCODING_DSPADPCM && getle16(ctx->channel[i].info.codecref.idtype) == 0x300)
			{
				u32 j;

				for(j=0; j<16; j++)
					fprintf(stdout, " > Adpcm coef %02d:       0x%04X\n", j, getle16(ctx->channel[i].infodspadpcm.coef[j]));
				fprintf(stdout, " > Adpcm scale:         0x%04X\n", getle16(ctx->channel[i].infodspadpcm.scale));
				fprintf(stdout, " > Adpcm yn1:           0x%04X\n", getle16(ctx->channel[i].infodspadpcm.yn1));
				fprintf(stdout, " > Adpcm yn2:           0x%04X\n", getle16(ctx->channel[i].infodspadpcm.yn2));
				fprintf(stdout, " > Adpcm loop scale:    0x%04X\n", getle16(ctx->channel[i].infodspadpcm.loopscale));
				fprintf(stdout, " > Adpcm loop yn1:      0x%04X\n", getle16(ctx->channel[i].infodspadpcm.loopyn1));
				fprintf(stdout, " > Adpcm loop yn2:      0x%04X\n", getle16(ctx->channel[i].infodspadpcm.loopyn2));
			}

			if (ctx->infoheader.encoding == CWAV_ENCODING_IMAADPCM && getle16(ctx->channel[i].info.codecref.idtype) == 0x301)
			{
				fprintf(stdout, " > Adpcm data:          0x%04X\n", getle16(ctx->channel[i].infoimaadpcm.data));
				fprintf(stdout, " > Adpcm tblindex:      0x%02X\n", ctx->channel[i].infoimaadpcm.tableindex);
				fprintf(stdout, " > Adpcm loopdata:      0x%04X\n", getle16(ctx->channel[i].infoimaadpcm.loopdata));
				fprintf(stdout, " > Adpcm looptblindex:  0x%02X\n", ctx->channel[i].infoimaadpcm.looptableindex);
			}
#endif
		}
	}
}
