#ifndef __TIK_H__
#define __TIK_H__

#include "types.h"

typedef struct {
	u32 enable_timelimit;
	u32 timelimit_seconds;
} timelimit_entry;

typedef struct {
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

void tik_print(const u8 *blob, u32 size);

#endif
