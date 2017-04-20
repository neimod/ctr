#ifndef _CWAV_H_
#define _CWAV_H_

#include <stdio.h>
#include "types.h"
#include "settings.h"
#include "stream.h"

#define CWAV_ENCODING_PCM8			0
#define CWAV_ENCODING_PCM16			1
#define CWAV_ENCODING_DSPADPCM		2
#define CWAV_ENCODING_IMAADPCM		3

typedef struct
{
	u8 idtype[2];
	u8 padding[2];
	u8 offset[4];
} cwav_reference;

typedef struct
{
	u8 idtype[2];
	u8 padding[2];
	u8 offset[4];
	u8 size[4];
} cwav_sizedreference;


typedef struct
{
	u8 magic[4];
	u8 byteordermark[2];
	u8 headersize[2];
	u8 version[4];
	u8 totalsize[4];
	u8 datablocks[2];
	u8 reserved[2];
	cwav_sizedreference infoblockref;
	cwav_sizedreference datablockref;
} cwav_header;

typedef struct
{
	u8 magic[4];
	u8 size[4];
	u8 encoding;
	u8 looped;
	u8 padding[2];
	u8 samplerate[4];
	u8 loopstart[4];
	u8 loopend[4];
	u8 reserved[4];
	u8 channelcount[4];
} cwav_infoheader;

typedef struct
{
	cwav_reference sampleref;
	cwav_reference codecref;
	u8 reserved[4];
} cwav_channelinfo;

typedef struct
{
	u8 coef[16][2];
	u8 scale[2];
	u8 yn1[2];
	u8 yn2[2];
	u8 loopscale[2];
	u8 loopyn1[2];
	u8 loopyn2[2];
} cwav_dspadpcminfo;

typedef struct
{
	u8 data[2];
	u8 tableindex;
	u8 padding;
	u8 loopdata[2];
	u8 looptableindex;
	u8 looppadding;
} cwav_imaadpcminfo;


typedef struct
{
	s16 yn1;
	s16 yn2;
	u32 sampleoffset;
	s16* samplebuffer;
	stream_in_context instreamctx;
} cwav_dspadpcmchannelstate;

typedef struct
{
	cwav_dspadpcmchannelstate* channelstate;
	s16* samplebuffer;
	u32 samplecountavailable;
	u32 samplecountcapacity;
	u32 samplecountremaining;
} cwav_dspadpcmstate;

typedef struct
{
	s16 data;
	u8 tableindex;
	u32 sampleoffset;
	s16* samplebuffer;
	stream_in_context instreamctx;
} cwav_imaadpcmchannelstate;

typedef struct
{
	cwav_imaadpcmchannelstate* channelstate;
	s16* samplebuffer;
	u32 samplecountavailable;
	u32 samplecountcapacity;
	u32 samplecountremaining;
} cwav_imaadpcmstate;

typedef struct
{
	u32 sampleoffset;
	s16* samplebuffer;
	stream_in_context instreamctx;
} cwav_pcmchannelstate;

typedef struct
{
	cwav_pcmchannelstate* channelstate;
	s16* samplebuffer;
	u32 samplecountavailable;
	u32 samplecountcapacity;
	u32 samplecountremaining;
} cwav_pcmstate;


typedef struct
{
	cwav_reference inforef;
	cwav_channelinfo info;
	cwav_dspadpcminfo infodspadpcm;
	cwav_imaadpcminfo infoimaadpcm;
} cwav_channel;

typedef struct
{
	u8 chunkid[4];
	u8 chunksize[4];
	u8 format[4];
	u8 subchunk1id[4];
	u8 subchunk1size[4];
	u8 audioformat[2];
	u8 numchannels[2];
	u8 samplerate[4];
	u8 byterate[4];
	u8 blockalign[2];
	u8 bitspersample[2];
	u8 subchunk2id[4];
	u8 subchunk2size[4];
} wav_pcm_header;

typedef struct
{
	FILE* file;
	settings* usersettings;
	u32 offset;
	u32 size;
	u32 channelcount;
	cwav_header header;
	cwav_infoheader infoheader;
	cwav_channel* channel;
} cwav_context;

void cwav_init(cwav_context* ctx);
void cwav_set_file(cwav_context* ctx, FILE* file);
void cwav_set_offset(cwav_context* ctx, u32 offset);
void cwav_set_size(cwav_context* ctx, u32 size);
void cwav_set_usersettings(cwav_context* ctx, settings* usersettings);
void cwav_process(cwav_context* ctx, u32 actions);
void cwav_dspadpcm_init(cwav_dspadpcmstate* state);
int cwav_dspadpcm_allocate(cwav_dspadpcmstate* state, cwav_context* ctx);
int  cwav_dspadpcm_setup(cwav_dspadpcmstate* state, cwav_context* ctx, int isloop);
int  cwav_dspadpcm_decode(cwav_dspadpcmstate* state, cwav_context* ctx);
int	 cwav_dspadpcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx);
void cwav_dspadpcm_destroy(cwav_dspadpcmstate* state);
void cwav_imaadpcm_init(cwav_imaadpcmstate* state);
int  cwav_imaadpcm_allocate(cwav_imaadpcmstate* state, cwav_context* ctx);
int  cwav_imaadpcm_setup(cwav_imaadpcmstate* state, cwav_context* ctx, int isloop);
int  cwav_imaadpcm_decode(cwav_imaadpcmstate* state, cwav_context* ctx);
int	 cwav_imaadpcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx);
u8   cwav_imaadpcm_clamp_tableindex(u8 tableindex, int inc);
void cwav_imaadpcm_destroy(cwav_imaadpcmstate* state);
void cwav_pcm_init(cwav_pcmstate* state);
int  cwav_pcm_allocate(cwav_pcmstate* state, cwav_context* ctx);
int  cwav_pcm_setup(cwav_pcmstate* state, cwav_context* ctx, int isloop);
int  cwav_pcm_decode(cwav_pcmstate* state, cwav_context* ctx);
int	 cwav_pcm_decode_to_wav(cwav_context* ctx, stream_out_context* outstreamctx);
void cwav_pcm_destroy(cwav_pcmstate* state);
void cwav_write_wav_header(cwav_context* ctx, stream_out_context* outstreamctx, u32 size);
int  cwav_save_to_wav(cwav_context* ctx, const char* filepath);
void cwav_print(cwav_context* ctx);

#endif // _CWAV_H_
