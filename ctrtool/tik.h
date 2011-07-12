#ifndef __TIK_H__
#define __TIK_H__

#include "types.h"

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
	u8  unknown2[2];
	u16 ticket_version;
		
} eticket;

void tik_print(const u8 *blob, u32 size);

#endif
