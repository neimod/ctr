#ifndef __TIK_H__
#define __TIK_H__

#include "types.h"
#include "keyset.h"
#include "ctr.h"


typedef struct 
{
	u32 enable_timelimit;
	u32 timelimit_seconds;
} timelimit_entry;

typedef struct 
{
	u32 sig_type;
	u8  signature[0x100];
	u8  padding1[0x3c];
	u8  issuer[0x40];
	u8  ecdsa[0x3c];
	u8  padding2[0x03];
	u8  encrypted_title_key[0x10];
	u8  unknown;
	u8  ticket_id[8];
	u32 console_id;
	u8  title_id[8];	
	u16 sys_access;
	u16 ticket_version;
	u32 time_mask;
	u32 permit_mask;
	u8 title_export;
	u8 commonkey_idx;
	u8 unknown_buf[0x30];
	u8 content_permissions[0x40];
	u16 padding0;

	timelimit_entry timelimits[8];	
} eticket;

typedef struct
{
	FILE* file;
	u32 offset;
	u32 size;
	u8 key[16];
	int keyvalid;
	u8 titlekey[16];
	eticket tik;
	ctr_aes_context aes;
} tik_context;

void tik_init(tik_context* ctx);
void tik_set_file(tik_context* ctx, FILE* file);
void tik_set_offset(tik_context* ctx, u32 offset);
void tik_set_size(tik_context* ctx, u32 size);
void tik_set_commonkey(tik_context* ctx, u8 key[16]);
void tik_get_decrypted_titlekey(tik_context* ctx, u8 decryptedkey[0x10]);
void tik_get_titleid(tik_context* ctx, u8 titleid[8]);
void tik_get_iv(tik_context* ctx, u8 iv[0x10]);
void tik_decrypt_titlekey(tik_context* ctx, u8 decryptedkey[0x10]);
void tik_print(tik_context* ctx);
void tik_process(tik_context* ctx, u32 actions);

#endif
