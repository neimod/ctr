#ifndef __TIK_H__
#define __TIK_H__

#include "types.h"
#include "keyset.h"
#include "ctr.h"
#include "settings.h"

typedef struct 
{
	u8 enable_timelimit[4];
	u8 timelimit_seconds[4];
} timelimit_entry;

typedef struct 
{
	u8 sig_type[4];
	u8 signature[0x100];
	u8 padding1[0x3c];
	u8 issuer[0x40];
	u8 ecdsa[0x3c];
	u8 padding2[0x03];
	u8 encrypted_title_key[0x10];
	u8 unknown;
	u8 ticket_id[8];
	u8 console_id[4];
	u8 title_id[8];
	u8 sys_access[2];
	u8 ticket_version[2];
	u8 time_mask[4];
	u8 permit_mask[4];
	u8 title_export;
	u8 commonkey_idx;
	u8 unknown_buf[0x30];
	u8 content_permissions[0x40];
	u8 padding0[2];

	timelimit_entry timelimits[8];	
} eticket;

typedef struct
{
	FILE* file;
	u32 offset;
	u32 size;
	u8 titlekey[16];
	eticket tik;
	ctr_aes_context aes;
	settings* usersettings;
} tik_context;

void tik_init(tik_context* ctx);
void tik_set_file(tik_context* ctx, FILE* file);
void tik_set_offset(tik_context* ctx, u32 offset);
void tik_set_size(tik_context* ctx, u32 size);
void tik_set_usersettings(tik_context* ctx, settings* usersettings);
void tik_get_decrypted_titlekey(tik_context* ctx, u8 decryptedkey[0x10]);
void tik_get_titleid(tik_context* ctx, u8 titleid[8]);
void tik_get_iv(tik_context* ctx, u8 iv[0x10]);
void tik_decrypt_titlekey(tik_context* ctx, u8 decryptedkey[0x10]);
void tik_print(tik_context* ctx);
void tik_process(tik_context* ctx, u32 actions);

#endif
